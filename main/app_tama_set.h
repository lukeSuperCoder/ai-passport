#pragma once

#include "bsp_button.h"
#include "lvgl.h"
#include <stdbool.h>

typedef enum {
    TAMA_SET_WIFI = 0,
    TAMA_SET_BLE,
    TAMA_SET_CLOCK,
    TAMA_SET_SCREEN,
    TAMA_SET_SOUND,
    TAMA_SET_OTA
} tama_set_id_t;

void app_tama_set_open(lv_obj_t *lcd, tama_set_id_t id);
void app_tama_set_close(void);
bool app_tama_set_open_now(void);
bool app_tama_set_busy(void);
void app_tama_set_on_key(bsp_btn_t btn, bsp_btn_ev_t ev);
void app_tama_set_tick(void);
