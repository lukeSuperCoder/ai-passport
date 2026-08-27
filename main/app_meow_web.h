#pragma once

#include "bsp_button.h"
#include "lvgl.h"

#include <stdbool.h>
#include <stddef.h>

#define APP_MEOW_WEB_PORT 8080

void app_meow_web_init(lv_obj_t *screen);
void app_meow_web_poll(void);
void app_meow_web_set_target(char *buf, size_t cap, void (*refresh)(void));
void app_meow_web_clear_target(void);
bool app_meow_web_url(char *buf, size_t n);
void app_meow_web_qr_open(void);
void app_meow_web_qr_close(void);
bool app_meow_web_qr_visible(void);
bool app_meow_web_qr_key(bsp_btn_t btn, bsp_btn_ev_t ev);
