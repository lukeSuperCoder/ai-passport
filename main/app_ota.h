#pragma once

#include <stdbool.h>

typedef enum {
    APP_OTA_IDLE = 0,
    APP_OTA_CHECKING,
    APP_OTA_LATEST,
    APP_OTA_AVAILABLE,
    APP_OTA_APPLYING,
    APP_OTA_FAIL
} app_ota_state_t;

typedef enum {
    APP_OTA_E_NONE = 0,
    APP_OTA_E_WIFI,
    APP_OTA_E_NET,
    APP_OTA_E_PARSE,
    APP_OTA_E_LOWBAT,
    APP_OTA_E_HASH,
    APP_OTA_E_CANCEL
} app_ota_err_t;

void app_ota_init(void);
void app_ota_tick(bool allow_auto);
app_ota_state_t app_ota_state(void);
app_ota_err_t app_ota_err(void);
const char *app_ota_cur_ver(void);
const char *app_ota_new_ver(void);
const char *app_ota_channel(void);
int app_ota_progress(void);
bool app_ota_busy(void);
bool app_ota_prompt(void);
void app_ota_check(void);
void app_ota_apply(void);
void app_ota_skip(void);
void app_ota_cancel(void);
