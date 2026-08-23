// components/bsp/src/bsp_button.c
// 移植自 trae_card/components/platform/platform_esp32/src/btn_iot_button.c
#include "bsp_button.h"
#include "bsp_pins.h"
#include "iot_button.h"
#include "button_adc.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_attr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/portmacro.h"

static const char *TAG = "bsp_btn";

ESP_EVENT_DEFINE_BASE(BSP_BTN_EVENT);
#define BSP_BTN_WAKE 0

static const uint16_t BTN_MV[BSP_BTN_COUNT][2] = BSP_BTN_MV_TABLE;

static button_handle_t s_btn[BSP_BTN_COUNT];
static bsp_btn_cb_t    s_cb;
static void           *s_user;
static void          (*s_wake_cb)(void);
static bool            s_gpio_wake;

// ADC1 是 unit 级独占资源:iot_button 与 bsp_button_read_mv() 必须共用同一个 oneshot
// 句柄。谁第二个调 adc_oneshot_new_unit() 谁就拿到 "adc1 is already in use"。
static adc_oneshot_unit_handle_t s_adc;
static adc_cali_handle_t         s_cali;

// 电压读取的衰减档必须与 button 组件内部的 ADC_BUTTON_ATTEN 一致 —— 通道只被配置一次
// (由组件在 iot_button_new_adc_device() 里下发),两边对不上会让读数与按键阈值错位。
// managed_components/espressif__button/button_adc.c:26 在 C3 上取 ADC_ATTEN_DB_6+1。
#define BSP_BTN_ATTEN  ADC_ATTEN_DB_12       // 量程约 0~3100mV,覆盖松开态

// 每个按键把"哪个键"随回调带回来。button 组件的回调签名固定,故用 usr_data 传索引。
static void on_event(void *arg, void *usr_data, bsp_btn_ev_t ev) {
    (void)arg;
    if (!s_cb) return;
    s_cb((bsp_btn_t)(intptr_t)usr_data, ev, s_user);
}
static void cb_press (void *a, void *u) { on_event(a, u, BSP_BTN_PRESS);  }
static void cb_click (void *a, void *u) { on_event(a, u, BSP_BTN_CLICK);  }
static void cb_double(void *a, void *u) { on_event(a, u, BSP_BTN_DOUBLE); }
static void cb_long  (void *a, void *u) { on_event(a, u, BSP_BTN_LONG);   }
static void cb_release(void *a, void *u) { on_event(a, u, BSP_BTN_RELEASE); }

static void on_wake_evt(void *h, esp_event_base_t b, int32_t id, void *d)
{
    (void)h;
    (void)b;
    (void)id;
    (void)d;
    if (s_wake_cb) s_wake_cb();
}

static void IRAM_ATTR gpio_isr(void *arg)
{
    (void)arg;
    gpio_intr_disable(BSP_BTN_GPIO);
    BaseType_t hp = pdFALSE;
    esp_event_isr_post(BSP_BTN_EVENT, BSP_BTN_WAKE, NULL, 0, &hp);
    if (hp) portYIELD_FROM_ISR();
}

static void adc_reattach(void)
{
    if (!s_adc) return;
    adc_oneshot_chan_cfg_t cfg = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = BSP_BTN_ATTEN,
    };
    adc_oneshot_config_channel(s_adc, BSP_BTN_ADC_CHANNEL, &cfg);
}

esp_err_t bsp_button_init(bsp_btn_cb_t cb, void *user) {
    s_cb = cb; s_user = user;

    // 先由 BSP 建 unit,再把句柄交给 button 组件(button_adc.h:adc_handle 非 NULL 即复用),
    // 这样本文件的 bsp_button_read_mv() 也能读同一路 ADC。
    const adc_oneshot_unit_init_cfg_t ucfg = { .unit_id = BSP_BTN_ADC_UNIT };
    esp_err_t ae = adc_oneshot_new_unit(&ucfg, &s_adc);
    if (ae != ESP_OK) {
        ESP_LOGE(TAG, "ADC unit 创建失败 (%s)", esp_err_to_name(ae));
        s_adc = NULL;
        return ae;
    }

    for (int i = 0; i < BSP_BTN_COUNT; i++) {
        const button_adc_config_t ac = {
            .adc_handle   = &s_adc,          // 复用上面这一个,别让组件自建
            .unit_id      = BSP_BTN_ADC_UNIT,
            .adc_channel  = BSP_BTN_ADC_CHANNEL,
            .button_index = i,
            .min          = BTN_MV[i][0],
            .max          = BTN_MV[i][1],
        };
        const button_config_t bc = {
            .long_press_time = 800,
            .short_press_time = 180,
        };
        esp_err_t e = iot_button_new_adc_device(&bc, &ac, &s_btn[i]);
        if (e != ESP_OK || !s_btn[i]) {
            ESP_LOGE(TAG, "按键 %d 创建失败 (%s) —— 检查 GPIO%d 的 ADC 配置与分压电阻",
                     i, esp_err_to_name(e), BSP_BTN_ADC_CHANNEL);
            return e == ESP_OK ? ESP_FAIL : e;
        }
        void *idx = (void *)(intptr_t)i;
        iot_button_register_cb(s_btn[i], BUTTON_PRESS_DOWN,      NULL, cb_press,  idx);
        iot_button_register_cb(s_btn[i], BUTTON_PRESS_UP,        NULL, cb_release, idx);
        iot_button_register_cb(s_btn[i], BUTTON_SINGLE_CLICK,    NULL, cb_click,  idx);
        iot_button_register_cb(s_btn[i], BUTTON_DOUBLE_CLICK,    NULL, cb_double, idx);
        iot_button_register_cb(s_btn[i], BUTTON_LONG_PRESS_START,NULL, cb_long,   idx);
    }

    // 通道已由组件配置好,这里只补一份校准句柄给 bsp_button_read_mv() 用。
    // 失败不致命:按键照常工作,只是读不出电压(标定分压电阻时才需要)。
    const adc_cali_curve_fitting_config_t cal = {
        .unit_id  = BSP_BTN_ADC_UNIT,
        .chan     = BSP_BTN_ADC_CHANNEL,
        .atten    = BSP_BTN_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    if (adc_cali_create_scheme_curve_fitting(&cal, &s_cali) != ESP_OK) {
        ESP_LOGW(TAG, "ADC 校准创建失败,Button 页将无法显示电压");
        s_cali = NULL;
    }

    esp_err_t le = esp_event_loop_create_default();
    if (le != ESP_OK && le != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "事件循环创建失败: %s", esp_err_to_name(le));
    }
    esp_event_handler_register(BSP_BTN_EVENT, BSP_BTN_WAKE, on_wake_evt, NULL);

    ESP_LOGI(TAG, "按键就绪:ADC1_CH%d 三键分压", BSP_BTN_ADC_CHANNEL);
    return ESP_OK;
}

void bsp_button_set_wake_cb(void (*cb)(void))
{
    s_wake_cb = cb;
}

esp_err_t bsp_button_sleep_gpio(bool on)
{
    if (!s_btn[0]) return ESP_ERR_INVALID_STATE;
    if (on == s_gpio_wake) return ESP_OK;

    if (on) {
        esp_err_t e = iot_button_stop();
        if (e != ESP_OK) ESP_LOGW(TAG, "button stop: %s", esp_err_to_name(e));

        gpio_config_t io = {
            .pin_bit_mask = 1ULL << BSP_BTN_GPIO,
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_config(&io);
        vTaskDelay(pdMS_TO_TICKS(10));

        e = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
        if (e != ESP_OK && e != ESP_ERR_INVALID_STATE) {
            ESP_LOGW(TAG, "gpio isr service: %s", esp_err_to_name(e));
        }
        gpio_wakeup_enable(BSP_BTN_GPIO, GPIO_INTR_LOW_LEVEL);
        esp_sleep_enable_gpio_wakeup();
        gpio_set_intr_type(BSP_BTN_GPIO, GPIO_INTR_LOW_LEVEL);
        gpio_isr_handler_remove(BSP_BTN_GPIO);
        gpio_isr_handler_add(BSP_BTN_GPIO, gpio_isr, NULL);
        gpio_intr_enable(BSP_BTN_GPIO);
        s_gpio_wake = true;
        ESP_LOGI(TAG, "按键切 GPIO%d 唤醒", BSP_BTN_GPIO);
        return ESP_OK;
    }

    gpio_intr_disable(BSP_BTN_GPIO);
    gpio_isr_handler_remove(BSP_BTN_GPIO);
    gpio_set_intr_type(BSP_BTN_GPIO, GPIO_INTR_DISABLE);
    gpio_wakeup_disable(BSP_BTN_GPIO);
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);
    adc_reattach();
    iot_button_resume();
    s_gpio_wake = false;
    return ESP_OK;
}

int bsp_button_read_mv(void) {
    // 读的是 bsp_button_init() 建好、并与 iot_button 共用的那一路 ADC。
    // 单次采样与组件的按键轮询互不干扰(oneshot 内部自带锁)。
    if (!s_adc || !s_cali || s_gpio_wake) return -1;

    int raw = 0, mv = 0;
    if (adc_oneshot_read(s_adc, BSP_BTN_ADC_CHANNEL, &raw) != ESP_OK) return -1;
    if (adc_cali_raw_to_voltage(s_cali, raw, &mv) != ESP_OK) return -1;
    return mv;
}
