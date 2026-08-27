#include "app.h"

#include "app_notif.h"
#include "app_prefs.h"
#include "app_time.h"
#include "app_web.h"
#include "bsp_audio.h"
#include "bsp_battery.h"
#include "bsp_ble.h"
#include "bsp_display.h"
#include "bsp_pm.h"
#include "bsp_wifi.h"
#include "esp_event.h"
#include "ui_pixel.h"
#include "walkie.h"

#define STACK_MAX 8

ESP_EVENT_DEFINE_BASE(APP_SHELL_EVENT);
#define APP_SHELL_BLE_WAKE 1

typedef struct {
    app_enter_fn enter;
    app_exit_fn exit;
    app_key_fn key;
} page_t;

static lv_obj_t *s_scr, *s_main, *s_clock, *s_batt, *s_ico_bt, *s_ico_wf;
static lv_timer_t *s_timer, *s_home_lock_timer;
static page_t s_stack[STACK_MAX];
static int s_sp = -1;
static volatile bool s_asleep;
static bool s_lock_skip;
static uint32_t s_idle_ms;
static int s_seen_soc = -2;

static lv_obj_t *pix(lv_obj_t *p, int x, int y, int w, int h)
{
    lv_obj_t *o = lv_obj_create(p);
    ui_pixel_strip_theme(o);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(0xFFFFFF), 0);
    return o;
}

static lv_obj_t *make_wifi_icon(lv_obj_t *parent)
{
    lv_obj_t *g = lv_obj_create(parent);
    ui_pixel_strip_theme(g);
    lv_obj_set_size(g, 16, 14);
    lv_obj_set_style_bg_opa(g, LV_OPA_TRANSP, 0);
    pix(g, 7, 11, 2, 2);
    pix(g, 5, 8, 6, 2);
    pix(g, 3, 4, 10, 2);
    pix(g, 1, 0, 14, 2);
    return g;
}

static lv_obj_t *make_bt_icon(lv_obj_t *parent)
{
    lv_obj_t *g = lv_obj_create(parent);
    ui_pixel_strip_theme(g);
    lv_obj_set_size(g, 12, 14);
    lv_obj_set_style_bg_opa(g, LV_OPA_TRANSP, 0);
    pix(g, 5, 0, 2, 14);
    pix(g, 7, 2, 3, 2);
    pix(g, 7, 10, 3, 2);
    pix(g, 3, 4, 3, 2);
    pix(g, 3, 8, 3, 2);
    pix(g, 9, 6, 2, 2);
    return g;
}

static void recolor(lv_obj_t *icon, uint32_t color)
{
    uint32_t n = lv_obj_get_child_count(icon);
    for (uint32_t i = 0; i < n; i++) {
        lv_obj_set_style_bg_color(lv_obj_get_child(icon, i), lv_color_hex(color), 0);
    }
}

static void header_refresh(void)
{
    char clk[24];
    app_time_now_text(clk, sizeof(clk));
    lv_label_set_text(s_clock, clk);

    int soc = bsp_battery_soc();
    if (soc != s_seen_soc) {
        s_seen_soc = soc;
        if (soc < 0) lv_label_set_text(s_batt, "");
        else lv_label_set_text_fmt(s_batt, "%d%%", soc);
    }

    int x = 232;
    bool wf = bsp_wifi_enabled();
    bool bt = bsp_ble_enabled();
    if (wf) {
        x -= 18;
        lv_obj_remove_flag(s_ico_wf, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(s_ico_wf, x, 9);
        bsp_wifi_state_t st = bsp_wifi_state();
        uint32_t c = 0x7A8A96;
        if (st == BSP_WIFI_CONNECTED) c = 0xFFFFFF;
        else if (st == BSP_WIFI_CONNECTING) c = UI_YELLOW;
        recolor(s_ico_wf, c);
    } else {
        lv_obj_add_flag(s_ico_wf, LV_OBJ_FLAG_HIDDEN);
    }
    if (bt) {
        x -= 16;
        lv_obj_remove_flag(s_ico_bt, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(s_ico_bt, x, 9);
        bsp_ble_state_t st = bsp_ble_state();
        uint32_t c = 0x7A8A96;
        if (st == BSP_BLE_ANCS || st == BSP_BLE_CONNECTED) c = 0xFFFFFF;
        else if (st == BSP_BLE_ADVERTISING || st == BSP_BLE_PAIRING) c = UI_YELLOW;
        recolor(s_ico_bt, c);
    } else {
        lv_obj_add_flag(s_ico_bt, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_set_pos(s_batt, x - 46, 7);
}

static void sleep_now(void)
{
    if (s_asleep) return;
    if (walkie_busy()) return;

    const app_prefs_t *p = app_prefs();
    if (p->lock_on || app_lock_visible()) {
        app_lock_show();
        lv_refr_now(NULL);
        if (p->lock_stay) {
            s_idle_ms = 0;
            return;
        }
    }

    s_asleep = true;
    bsp_display_backlight(0);
    lv_refr_now(NULL);
    bsp_lvgl_flush_enable(false);
    bsp_display_sleep(true);
    bsp_audio_standby();
    app_wx_pause(true);
    bsp_wifi_radio_suspend();
    bsp_button_sleep_gpio(true);
    bsp_lvgl_tick_enable(false);
    bsp_pm_set_sleeping(true);
}

void app_shell_wake(void)
{
    s_idle_ms = 0;
    if (!s_asleep) return;
    s_asleep = false;
    bsp_pm_set_sleeping(false);
    bsp_button_sleep_gpio(false);
    bsp_lvgl_tick_enable(true);
    app_wx_pause(false);
    if (app_prefs()->lock_on || app_lock_visible()) {
        if (!app_lock_visible()) app_lock_show();
        app_lock_tick();
    } else {
        app_lock_hide();
    }
    bsp_display_sleep(false);
    bsp_lvgl_flush_enable(true);
    lv_obj_invalidate(s_scr);
    lv_refr_now(NULL);
    app_prefs_apply_display();
    if (s_timer) lv_timer_set_period(s_timer, 250);
}

static void tick(lv_timer_t *t)
{
    (void)t;
    app_time_tick();
    app_web_poll();
    app_wx_start();
    app_notif_poll();
    app_notif_tick(s_asleep ? 1000 : 250);
    if (app_lock_visible() && !s_asleep) app_lock_tick();
    if (!s_asleep && !app_lock_visible()) header_refresh();

    if (app_notif_visible() || app_web_visible() || app_web_qr_visible()) {
        s_idle_ms = 0;
        return;
    }
    if (app_lock_visible() && app_prefs()->lock_stay) {
        s_idle_ms = 0;
        return;
    }
    uint16_t lim = app_prefs()->sleep_sec;
    if (lim == 0) return;
    if (walkie_busy()) {
        s_idle_ms = 0;
        return;
    }
    s_idle_ms += s_asleep ? 1000 : 250;
    if (s_idle_ms >= (uint32_t)lim * 1000) sleep_now();
}

static void show_page(void)
{
    if (s_sp < 0) return;
    lv_obj_clean(s_main);
    s_stack[s_sp].enter(s_main);
}

void app_shell_open(app_enter_fn enter, app_exit_fn exit, app_key_fn key)
{
    if (s_sp + 1 >= STACK_MAX) return;
    if (s_sp >= 0 && s_stack[s_sp].exit) s_stack[s_sp].exit();
    s_sp++;
    s_stack[s_sp].enter = enter;
    s_stack[s_sp].exit = exit;
    s_stack[s_sp].key = key;
    show_page();
}

void app_shell_back(void)
{
    if (s_sp <= 0) return;
    if (s_stack[s_sp].exit) s_stack[s_sp].exit();
    s_sp--;
    show_page();
}

void app_shell_reload(void)
{
    if (s_sp < 0) return;
    if (s_stack[s_sp].exit) s_stack[s_sp].exit();
    show_page();
}

bool app_shell_asleep(void)
{
    return s_asleep;
}

bool app_shell_locked(void)
{
    return app_lock_visible();
}

static void lock_now(void)
{
    if (app_lock_visible()) return;
    if (walkie_ptt()) walkie_set_ptt(false);
    app_lock_show();
}

static void home_lock_cancel(void)
{
    if (s_home_lock_timer) {
        lv_timer_delete(s_home_lock_timer);
        s_home_lock_timer = NULL;
    }
}

static void home_lock_timeout(lv_timer_t *t)
{
    (void)t;
    s_home_lock_timer = NULL;
    if (s_sp <= 0 && !app_lock_visible() && !s_asleep) lock_now();
}

static bool on_home(void)
{
    return s_sp <= 0;
}

static void wifi_bringup(void)
{
    bsp_wifi_radio_resume();
}

static void on_gpio_wake(void)
{
    if (!bsp_lvgl_lock(1000)) return;
    s_lock_skip = true;
    app_shell_wake();
    wifi_bringup();
    bsp_lvgl_unlock();
}

static void on_shell_evt(void *h, esp_event_base_t b, int32_t id, void *d)
{
    (void)h;
    (void)b;
    (void)id;
    (void)d;
    if (!bsp_lvgl_lock(1000)) return;
    app_shell_wake();
    app_notif_poll();
    bsp_lvgl_unlock();
}

static void on_ble_activity(void)
{
    if (!app_shell_asleep()) return;
    esp_event_post(APP_SHELL_EVENT, APP_SHELL_BLE_WAKE, NULL, 0, 0);
}

void app_shell_on_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    if (s_asleep) {
        home_lock_cancel();
        if (ev == BSP_BTN_PRESS || ev == BSP_BTN_CLICK) {
            app_shell_wake();
            wifi_bringup();
            s_lock_skip = true;
        }
        return;
    }
    s_idle_ms = 0;
    wifi_bringup();

    if (btn == BSP_BTN_OK && ev == BSP_BTN_PRESS && on_home() &&
        !app_lock_visible()) {
        home_lock_cancel();
        s_home_lock_timer = lv_timer_create(home_lock_timeout, 800, NULL);
        lv_timer_set_repeat_count(s_home_lock_timer, 1);
    } else if (ev == BSP_BTN_RELEASE || btn != BSP_BTN_OK) {
        home_lock_cancel();
    }

    if (app_notif_visible() && app_notif_pairing()) {
        app_notif_key(btn, ev);
        return;
    }

    if (s_lock_skip) {
        if (ev == BSP_BTN_CLICK) s_lock_skip = false;
        return;
    }

    if (app_notif_visible()) {
        app_notif_key(btn, ev);
        return;
    }
    if (app_web_visible()) {
        app_web_key(btn, ev);
        return;
    }
    if (app_web_qr_visible()) {
        app_web_qr_key(btn, ev);
        return;
    }

    if (app_lock_visible()) {
        if (ev == BSP_BTN_CLICK) {
            app_lock_hide();
            header_refresh();
        }
        return;
    }

    if (btn == BSP_BTN_OK && ev == BSP_BTN_LONG) {
        if (on_home()) lock_now();
        else app_shell_back();
        return;
    }
    if (s_sp >= 0 && s_stack[s_sp].key) s_stack[s_sp].key(btn, ev);
}

void app_shell_start(void)
{
    s_scr = lv_obj_create(NULL);
    ui_pixel_strip_theme(s_scr);
    lv_obj_set_style_bg_opa(s_scr, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(s_scr, lv_color_hex(UI_SKY), 0);
    lv_obj_set_style_text_font(s_scr, ui_pixel_font_14(), 0);

    lv_obj_t *hdr = lv_obj_create(s_scr);
    ui_pixel_strip_theme(hdr);
    lv_obj_set_pos(hdr, 0, 0);
    lv_obj_set_size(hdr, 240, APP_HEADER_H);
    lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(hdr, lv_color_hex(UI_INK), 0);

    s_clock = lv_label_create(hdr);
    lv_obj_set_style_text_font(s_clock, ui_pixel_font_14(), 0);
    lv_obj_set_style_text_color(s_clock, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_pos(s_clock, 8, 7);

    s_batt = lv_label_create(hdr);
    lv_obj_set_style_text_font(s_batt, ui_pixel_font_14(), 0);
    lv_obj_set_style_text_color(s_batt, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_pos(s_batt, 150, 7);
    lv_label_set_text(s_batt, "");

    s_ico_bt = make_bt_icon(hdr);
    s_ico_wf = make_wifi_icon(hdr);

    s_main = lv_obj_create(s_scr);
    ui_pixel_strip_theme(s_main);
    lv_obj_add_flag(s_main, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_set_pos(s_main, 0, APP_HEADER_H);
    lv_obj_set_size(s_main, 240, 320 - APP_HEADER_H);
    lv_obj_set_style_bg_opa(s_main, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(s_main, lv_color_hex(UI_SKY), 0);

    app_lock_init(s_scr);
    app_notif_init(s_scr);
    app_web_init(s_scr);
    s_sp = -1;
    s_asleep = false;
    s_idle_ms = 0;
    bsp_button_set_wake_cb(on_gpio_wake);
    bsp_ble_set_activity_cb(on_ble_activity);
    esp_event_handler_register(APP_SHELL_EVENT, APP_SHELL_BLE_WAKE, on_shell_evt, NULL);
    app_shell_open(app_home_enter, app_home_exit, app_home_key);
    header_refresh();
    s_timer = lv_timer_create(tick, 250, NULL);
    lv_screen_load(s_scr);
}
