#pragma once

#include "bsp_button.h"
#include "lvgl.h"
#include <stdbool.h>
#include <stddef.h>

#define APP_HEADER_H 32

typedef void (*app_enter_fn)(lv_obj_t *parent);
typedef void (*app_exit_fn)(void);
typedef void (*app_key_fn)(bsp_btn_t btn, bsp_btn_ev_t ev);

void app_shell_start(void);
void app_shell_on_key(bsp_btn_t btn, bsp_btn_ev_t ev);
void app_shell_open(app_enter_fn enter, app_exit_fn exit, app_key_fn key);
void app_shell_back(void);
void app_shell_reload(void);
void app_shell_wake(void);
bool app_shell_asleep(void);
bool app_shell_locked(void);

void app_lock_init(lv_obj_t *screen);
void app_lock_show(void);
void app_lock_hide(void);
bool app_lock_visible(void);
void app_lock_tick(void);

void app_home_enter(lv_obj_t *p);
void app_home_exit(void);
void app_home_key(bsp_btn_t btn, bsp_btn_ev_t ev);

void app_settings_enter(lv_obj_t *p);
void app_settings_exit(void);
void app_settings_key(bsp_btn_t btn, bsp_btn_ev_t ev);

void app_wifi_enter(lv_obj_t *p);
void app_wifi_exit(void);
void app_wifi_key(bsp_btn_t btn, bsp_btn_ev_t ev);

void app_ble_enter(lv_obj_t *p);
void app_ble_exit(void);
void app_ble_key(bsp_btn_t btn, bsp_btn_ev_t ev);

void app_clock_enter(lv_obj_t *p);
void app_clock_exit(void);
void app_clock_key(bsp_btn_t btn, bsp_btn_ev_t ev);

void app_screen_enter(lv_obj_t *p);
void app_screen_exit(void);
void app_screen_key(bsp_btn_t btn, bsp_btn_ev_t ev);

void app_sound_enter(lv_obj_t *p);
void app_sound_exit(void);
void app_sound_key(bsp_btn_t btn, bsp_btn_ev_t ev);

void app_ancs_enter(lv_obj_t *p);
void app_ancs_exit(void);
void app_ancs_key(bsp_btn_t btn, bsp_btn_ev_t ev);

void app_hw_enter(lv_obj_t *p);
void app_hw_exit(void);
void app_hw_key(bsp_btn_t btn, bsp_btn_ev_t ev);

void app_logs_start(void);
void app_logs_enter(lv_obj_t *p);
void app_logs_exit(void);
void app_logs_key(bsp_btn_t btn, bsp_btn_ev_t ev);

void app_wx_start(void);
void app_wx_pause(bool on);
void app_wx_stop(void);
// Wi-Fi 开启且有城市时写一行锁屏天气。无缓存时仍返回 true 并写城市。
bool app_wx_lock_line(char *out, size_t n);
// 有有效预报时返回 WMO 天气码,否则 -1。
int app_wx_wmo(void);
// 在 40x40 透明容器里画像素天气图标。wmo<0 时清空。
void app_wx_draw_icon(lv_obj_t *parent, int wmo);
void app_wx_enter(lv_obj_t *p);
void app_wx_exit(void);
void app_wx_key(bsp_btn_t btn, bsp_btn_ev_t ev);

void app_totp_enter(lv_obj_t *p);
void app_totp_exit(void);
void app_totp_key(bsp_btn_t btn, bsp_btn_ev_t ev);

void app_walkie_enter(lv_obj_t *p);
void app_walkie_exit(void);
void app_walkie_key(bsp_btn_t btn, bsp_btn_ev_t ev);
