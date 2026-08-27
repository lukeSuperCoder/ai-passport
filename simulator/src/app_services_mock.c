#include "app_meow_link.h"
#include "app_meow_web.h"
#include "app_ota.h"
#include "app_time.h"
#include "app_tone.h"
#include "bsp_ble.h"
#include "bsp_pm.h"
#include "bsp_wifi.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

void app_time_init(void) {}
void app_time_tick(void) {}

static void local_now(struct tm *out)
{
    time_t now = time(NULL);
    localtime_r(&now, out);
}

void app_time_now_text(char *out, size_t n)
{
    struct tm tm;
    local_now(&tm);
    strftime(out, n, "%Y-%m-%d %H:%M", &tm);
}

void app_time_lock_date(char *out, size_t n)
{
    struct tm tm;
    local_now(&tm);
    strftime(out, n, "%Y-%m-%d", &tm);
}

void app_time_lock_clock(char *out, size_t n)
{
    struct tm tm;
    local_now(&tm);
    strftime(out, n, "%H:%M", &tm);
}

void app_time_set(int year, int month, int day, int hour, int minute)
{
    (void)year; (void)month; (void)day; (void)hour; (void)minute;
}

void app_time_get(int *year, int *month, int *day, int *hour, int *minute)
{
    struct tm tm;
    local_now(&tm);
    if (year) *year = tm.tm_year + 1900;
    if (month) *month = tm.tm_mon + 1;
    if (day) *day = tm.tm_mday;
    if (hour) *hour = tm.tm_hour;
    if (minute) *minute = tm.tm_min;
}

bool app_time_ntp_synced(void) { return true; }
void app_time_ntp_restart(void) {}

void app_tone_start(void) {}
void app_tone_play(int id) { (void)id; }
void app_tone_note(int hz, int ms) { (void)hz; (void)ms; }
void app_tone_gate(bool on) { (void)on; }

void app_ota_init(void) {}
void app_ota_tick(bool allow_auto) { (void)allow_auto; }
app_ota_state_t app_ota_state(void) { return APP_OTA_IDLE; }
app_ota_err_t app_ota_err(void) { return APP_OTA_E_NONE; }
const char *app_ota_cur_ver(void) { return "desktop"; }
const char *app_ota_new_ver(void) { return "desktop"; }
const char *app_ota_channel(void) { return "desktop-singleplayer"; }
int app_ota_progress(void) { return 0; }
bool app_ota_busy(void) { return false; }
bool app_ota_prompt(void) { return false; }
void app_ota_check(void) {}
void app_ota_apply(void) {}
void app_ota_skip(void) {}
void app_ota_cancel(void) {}

void app_meow_link_prepare(void) {}
void app_meow_link_start(void) {}
bool app_meow_link_seek(app_meow_t *pet, int kind) { (void)pet; (void)kind; return false; }
void app_meow_link_cancel(void) {}
bool app_meow_link_busy(void) { return false; }
int app_meow_link_poll(app_meow_t *pet) { (void)pet; return APP_MEOW_LINK_IDLE; }

void app_meow_web_init(lv_obj_t *screen) { (void)screen; }
void app_meow_web_poll(void) {}
void app_meow_web_set_target(char *buf, size_t cap, void (*refresh)(void)) { (void)buf; (void)cap; (void)refresh; }
void app_meow_web_clear_target(void) {}
bool app_meow_web_url(char *buf, size_t n) { if (n) buf[0] = '\0'; return false; }
void app_meow_web_qr_open(void) {}
void app_meow_web_qr_close(void) {}
bool app_meow_web_qr_visible(void) { return false; }
bool app_meow_web_qr_key(bsp_btn_t btn, bsp_btn_ev_t ev) { (void)btn; (void)ev; return false; }

esp_err_t bsp_wifi_init(void) { return ESP_OK; }
bsp_wifi_state_t bsp_wifi_state(void) { return BSP_WIFI_IDLE; }
const char *bsp_wifi_ssid(void) { return ""; }
esp_err_t bsp_wifi_ip(char *buf, size_t n) { snprintf(buf, n, "0.0.0.0"); return ESP_OK; }
int bsp_wifi_scan(bsp_wifi_ap_t *out, int max) { (void)out; (void)max; return 0; }
esp_err_t bsp_wifi_connect(const char *ssid, const char *password) { (void)ssid; (void)password; return ESP_FAIL; }
esp_err_t bsp_wifi_forget(void) { return ESP_OK; }
bool bsp_wifi_has_saved(void) { return false; }
bool bsp_wifi_saved_pass(const char *ssid, char *pass, size_t n) { (void)ssid; if (n) pass[0] = '\0'; return false; }
bool bsp_wifi_enabled(void) { return false; }
esp_err_t bsp_wifi_set_enabled(bool on) { (void)on; return ESP_OK; }
bool bsp_wifi_auto_connect(void) { return false; }
esp_err_t bsp_wifi_set_auto_connect(bool on) { (void)on; return ESP_OK; }
bool bsp_wifi_power_save(void) { return true; }
esp_err_t bsp_wifi_set_power_save(bool on) { (void)on; return ESP_OK; }
void bsp_wifi_ps_hold(void) {}
void bsp_wifi_ps_release(void) {}
esp_err_t bsp_wifi_radio_suspend(void) { return ESP_OK; }
esp_err_t bsp_wifi_radio_resume(void) { return ESP_OK; }

esp_err_t bsp_ble_init(void) { return ESP_OK; }
bsp_ble_state_t bsp_ble_state(void) { return BSP_BLE_IDLE; }
const char *bsp_ble_name(void) { return "Meow Desktop"; }
uint32_t bsp_ble_passkey(void) { return 0; }
bool bsp_ble_pair_needs_confirm(void) { return false; }
esp_err_t bsp_ble_pair_reply(bool accept) { (void)accept; return ESP_OK; }
esp_err_t bsp_ble_unpair(void) { return ESP_OK; }
int bsp_ble_conn_count(void) { return 0; }
int bsp_ble_conn_max(void) { return 0; }
int bsp_ble_bond_count(void) { return 0; }
esp_err_t bsp_ble_ensure_advertising(void) { return ESP_FAIL; }
bool bsp_ble_adv_active(void) { return false; }
esp_err_t bsp_ble_set_advertising(bool on) { (void)on; return ESP_OK; }
esp_err_t bsp_ble_resume_advertising(void) { return ESP_OK; }
bool bsp_ble_take_notif(bsp_ble_notif_t *out) { (void)out; return false; }
void bsp_ble_set_activity_cb(void (*cb)(void)) { (void)cb; }
bool bsp_ble_enabled(void) { return false; }
esp_err_t bsp_ble_set_enabled(bool on) { (void)on; return ESP_OK; }
bool bsp_ble_stack_up(void) { return false; }
esp_err_t bsp_ble_suspend(void) { return ESP_OK; }
esp_err_t bsp_ble_resume(void) { return ESP_OK; }
bool bsp_ble_quiet(void) { return true; }
esp_err_t bsp_ble_set_quiet(bool on) { (void)on; return ESP_OK; }
int bsp_ble_list_peers(bsp_ble_peer_t *out, int max) { (void)out; (void)max; return 0; }
esp_err_t bsp_ble_forget_at(int index) { (void)index; return ESP_OK; }
void bsp_ble_set_extra_svcs(const void *svcs) { (void)svcs; }
void bsp_ble_set_scan_uuid128(const uint8_t uuid128[16]) { (void)uuid128; }
void bsp_ble_set_scan_mfg(const uint8_t *data, size_t n) { (void)data; (void)n; }
void bsp_ble_set_gap_cb(void (*cb)(void *event)) { (void)cb; }
esp_err_t bsp_ble_note_app_conn(int delta) { (void)delta; return ESP_OK; }
esp_err_t bsp_ble_refresh_adv(void) { return ESP_OK; }
uint8_t bsp_ble_own_addr_type(void) { return 0; }

esp_err_t bsp_pm_init(void) { return ESP_OK; }
void bsp_pm_set_sleeping(bool on) { (void)on; }
void bsp_pm_set_perf(bool on) { (void)on; }
