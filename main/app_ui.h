#pragma once

#include "bsp_button.h"
#include <stdbool.h>

#define APP_UI_DEMO_COUNT 4

// 构建主菜单。ok 按 Display/Button/Audio/Battery 顺序描述外设是否可用。
void app_ui_start(const bool ok[APP_UI_DEMO_COUNT]);

// 分发统一按键语义；调用方必须已经持有 LVGL 锁。
void app_ui_handle_key(bsp_btn_t btn, bsp_btn_ev_t ev);
