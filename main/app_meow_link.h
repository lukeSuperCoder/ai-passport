#pragma once

#include "app_meow.h"
#include <stdbool.h>

#define APP_MEOW_LINK_IDLE  0
#define APP_MEOW_LINK_WAIT  1
#define APP_MEOW_LINK_VISIT 2
#define APP_MEOW_LINK_WIN   3
#define APP_MEOW_LINK_LOSE  4
#define APP_MEOW_LINK_DRAW  5
#define APP_MEOW_LINK_NONE  6
#define APP_MEOW_LINK_FAIL  7

/* 必须在 bsp_ble_init() 之前调用。 */
void app_meow_link_prepare(void);
void app_meow_link_start(void);
bool app_meow_link_seek(app_meow_t *pet, int kind);
void app_meow_link_cancel(void);
bool app_meow_link_busy(void);
int app_meow_link_poll(app_meow_t *pet);
