#pragma once

#include "bsp_button.h"
#include <stdbool.h>

#define APP_UI_DEMO_COUNT 4

// 启动《时光驿站》。ok 保留现有外设能力数组接口，用于后续降级策略。
void app_ui_start(const bool ok[APP_UI_DEMO_COUNT]);

// 分发统一按键语义；调用方必须已经持有 LVGL 锁。
void app_ui_handle_key(bsp_btn_t btn, bsp_btn_ev_t ev);
