// components/bsp/src/bsp_battery.c
// 移植自 trae_card/components/platform/platform_esp32/src/battery_cw2017.c
// (去掉了电池 profile 写入部分:开源硬件用户电池各异,用芯片自带 Li-Poly profile 更通用)
#include "bsp_battery.h"
#include "bsp_i2c.h"
#include "bsp_pins.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "bsp_batt";

#define CW_REG_VERSION   0x00   // 版本号,上电应答即代表芯片在位
#define CW_REG_VCELL_H   0x02   // 14bit 电压,V(uV) = raw * 312.5
#define CW_REG_SOC_H     0x04   // 高字节 = 整数百分比;低字节(0x05)= 1/256 %
#define CW_REG_CONFIG    0x08
#define CW_REG_MODE      0x0A   // bit7-6: 00 工作 / 11 睡眠
#define CW_MODE_NORMAL   0x00
#define CW_MV_EMPTY      3300
#define CW_MV_FULL       4200

static i2c_master_dev_handle_t s_dev;
static int s_soc = -1;
static int s_mv = -1;

static int cw_read(uint8_t reg, uint8_t *buf, size_t n) {
    if (!s_dev) return -1;
    return i2c_master_transmit_receive(s_dev, &reg, 1, buf, n, 100) == ESP_OK ? 0 : -1;
}

static int cw_write(uint8_t reg, uint8_t val) {
    if (!s_dev) return -1;
    uint8_t b[2] = { reg, val };
    return i2c_master_transmit(s_dev, b, 2, 100) == ESP_OK ? 0 : -1;
}

esp_err_t bsp_battery_init(void) {
    if (s_dev) return ESP_OK;

    esp_err_t e = bsp_i2c_init();
    if (e != ESP_OK) return e;

    i2c_device_config_t dc = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = BSP_I2C_CW2017_ADDR,
        .scl_speed_hz    = 100000,
    };
    e = i2c_master_bus_add_device(bsp_i2c_bus(), &dc, &s_dev);
    if (e != ESP_OK) { ESP_LOGE(TAG, "添加 I2C 设备失败: %s", esp_err_to_name(e)); return e; }

    uint8_t ver = 0;
    if (cw_read(CW_REG_VERSION, &ver, 1) != 0) {
        ESP_LOGW(TAG, "CW2017 未应答 —— 用 bsp_i2c_scan() 确认 0x%02X 是否在线;"
                      "无电量计的板子可忽略本项", BSP_I2C_CW2017_ADDR);
        i2c_master_bus_rm_device(s_dev);
        s_dev = NULL;
        return ESP_ERR_NOT_FOUND;
    }
    ESP_LOGI(TAG, "检测到 CW2017 VERSION=0x%02X", ver);

    // MODE 睡眠时 CONFIG 写 0 也读不到 SOC。先唤醒,再用芯片自带 Li-Poly profile。
    cw_write(CW_REG_MODE, CW_MODE_NORMAL);
    cw_write(CW_REG_CONFIG, 0x00);
    vTaskDelay(pdMS_TO_TICKS(200));

    return ESP_OK;
}

static int soc_from_mv(int mv)
{
    if (mv <= CW_MV_EMPTY) return 0;
    if (mv >= CW_MV_FULL) return 100;
    return (mv - CW_MV_EMPTY) * 100 / (CW_MV_FULL - CW_MV_EMPTY);
}

int bsp_battery_mv(void) {
    uint8_t b[2] = { 0 };
    if (cw_read(CW_REG_VCELL_H, b, 2) != 0) return s_mv;
    uint32_t raw = ((uint32_t)b[0] << 8 | b[1]) & 0x3FFF;   // 14bit
    s_mv = (int)((raw * 3125) / 10000);                     // raw * 312.5uV → mV
    return s_mv;
}

int bsp_battery_soc(void) {
    uint8_t b[2] = { 0 };
    if (cw_read(CW_REG_SOC_H, b, 2) == 0 && b[0] <= 100) {
        s_soc = b[0];
        return s_soc;
    }
    int mv = bsp_battery_mv();
    if (mv >= 0) {
        s_soc = soc_from_mv(mv);
        return s_soc;
    }
    return s_soc;
}
