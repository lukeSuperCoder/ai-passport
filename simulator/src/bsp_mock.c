#include "bsp_mock.h"
#include "bsp_audio.h"
#include "bsp_battery.h"
#include "bsp_display.h"
#include "esp_log.h"
#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

static bsp_btn_cb_t s_button_cb;
static void *s_button_user;
static int s_pressed = -1;
static pthread_mutex_t s_lvgl_mutex;
static pthread_once_t s_mutex_once = PTHREAD_ONCE_INIT;
static uint32_t s_audio_hz = 16000;

static void mutex_init(void)
{
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&s_lvgl_mutex, &attr);
    pthread_mutexattr_destroy(&attr);
}

esp_err_t bsp_button_init(bsp_btn_cb_t cb, void *user)
{
    s_button_cb = cb;
    s_button_user = user;
    return ESP_OK;
}

void bsp_mock_button_set_pressed(bsp_btn_t btn, bool pressed)
{
    s_pressed = pressed ? (int)btn : -1;
}

void bsp_mock_button_emit(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    if (s_button_cb) s_button_cb(btn, ev, s_button_user);
}

int bsp_button_read_mv(void)
{
    static const int mv[] = { 20, 300, 595 };
    return s_pressed >= 0 ? mv[s_pressed] : 3300;
}

esp_err_t bsp_display_init(void) { return ESP_OK; }
esp_lcd_panel_handle_t bsp_display_panel(void) { return NULL; }
esp_lcd_panel_io_handle_t bsp_display_io(void) { return NULL; }
void bsp_display_backlight(uint8_t percent)
{
    ESP_LOGI("bsp_mock", "backlight=%u%%", percent);
}
void bsp_display_sleep(bool sleep) { (void)sleep; }
struct _lv_display_t *bsp_lvgl_init(void) { return NULL; }

bool bsp_lvgl_lock(int timeout_ms)
{
    (void)timeout_ms;
    pthread_once(&s_mutex_once, mutex_init);
    return pthread_mutex_lock(&s_lvgl_mutex) == 0;
}

void bsp_lvgl_unlock(void)
{
    pthread_mutex_unlock(&s_lvgl_mutex);
}

void bsp_lvgl_flush_enable(bool on) { (void)on; }
void bsp_lvgl_tick_enable(bool on) { (void)on; }
void bsp_lvgl_pause(void) {}
void bsp_lvgl_resume(void) {}

void bsp_button_set_wake_cb(void (*cb)(void)) { (void)cb; }
esp_err_t bsp_button_sleep_gpio(bool on) { (void)on; return ESP_OK; }

esp_err_t bsp_battery_init(void) { return ESP_OK; }
int bsp_battery_soc(void) { return 82; }
int bsp_battery_mv(void) { return 3970; }

esp_err_t bsp_audio_init(void) { return ESP_OK; }
esp_err_t bsp_audio_set_format(uint32_t hz, uint8_t bits, uint8_t ch)
{
    if (!hz || bits != 16 || ch != 1) return ESP_ERR_INVALID_ARG;
    s_audio_hz = hz;
    return ESP_OK;
}

esp_err_t bsp_audio_write(const void *pcm, size_t bytes)
{
    (void)pcm;
    usleep((useconds_t)((bytes / sizeof(int16_t)) * 1000000ULL / s_audio_hz));
    return ESP_OK;
}

esp_err_t bsp_audio_read(void *pcm, size_t bytes)
{
    int16_t *samples = pcm;
    size_t count = bytes / sizeof(*samples);
    static uint32_t phase;
    for (size_t i = 0; i < count; i++, phase++) {
        samples[i] = (int16_t)(1200.0 * sin(2.0 * 3.141592653589793 * 440.0 * phase / s_audio_hz));
    }
    usleep((useconds_t)(count * 1000000ULL / s_audio_hz));
    return ESP_OK;
}

void bsp_audio_set_volume(uint8_t percent)
{
    ESP_LOGI("bsp_mock", "volume=%u%%", percent);
}

void bsp_audio_standby(void) {}
