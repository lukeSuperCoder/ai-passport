#pragma once

#include "app_tama.h"
#include <stdbool.h>

#define APP_TAMA_LINK_IDLE  0
#define APP_TAMA_LINK_WAIT  1
#define APP_TAMA_LINK_VISIT 2
#define APP_TAMA_LINK_WIN   3
#define APP_TAMA_LINK_LOSE  4
#define APP_TAMA_LINK_DRAW  5
#define APP_TAMA_LINK_NONE  6
#define APP_TAMA_LINK_FAIL  7

/* 必须在 bsp_ble_init() 之前调用。 */
void app_tama_link_prepare(void);
void app_tama_link_start(void);
bool app_tama_link_seek(app_tama_t *pet, int kind);
void app_tama_link_cancel(void);
bool app_tama_link_busy(void);
int app_tama_link_poll(app_tama_t *pet);
