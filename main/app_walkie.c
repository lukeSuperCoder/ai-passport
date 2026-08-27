#include "app.h"

#include "app_i18n.h"
#include "app_prefs.h"
#include "app_ui.h"
#include "app_web.h"
#include "bsp_ble.h"
#include "bsp_wifi.h"
#include "ui_pixel.h"
#include "walkie.h"

#include "esp_heap_caps.h"

#include <stdio.h>
#include <string.h>

static lv_obj_t *s_title, *s_hint, *s_body;
static lv_timer_t *s_timer;
static int s_sel;
static int s_err; /* 1=wifi 2=ble 3=fail */
static bool s_pending;
static bool s_parked_ble;
static bool s_stopped_wx;

#define WALKIE_NEED_BLK 8192

static size_t largest_blk(void)
{
    return heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
}

static void radio_borrow(walkie_mode_t mode)
{
    app_wx_pause(true);
    size_t blk = largest_blk();
    if (mode == WALKIE_MODE_WEBRTC && bsp_ble_stack_up() && blk < WALKIE_NEED_BLK) {
        if (bsp_ble_suspend() == ESP_OK) s_parked_ble = true;
    } else if (mode == WALKIE_MODE_BLE && blk < WALKIE_NEED_BLK) {
        app_wx_stop();
        s_stopped_wx = true;
    }
}

static void radio_restore(void)
{
    if (s_parked_ble) {
        bsp_ble_resume();
        s_parked_ble = false;
    }
    app_wx_pause(false);
    if (s_stopped_wx) {
        s_stopped_wx = false;
        app_wx_start();
    }
}

static int item_n(void)
{
    return 3;
}

static const char *run_text(walkie_run_t st)
{
    switch (st) {
    case WALKIE_TALK:   return app_str(APP_STR_WALKIE_TALK);
    case WALKIE_LISTEN: return app_str(APP_STR_WALKIE_LISTEN);
    case WALKIE_LINK:   return app_str(APP_STR_WALKIE_PEER);
    case WALKIE_WAIT:   return app_str(APP_STR_WALKIE_WAIT);
    default:            return app_str(APP_STR_WALKIE_STOP);
    }
}

static bool wifi_url(char *buf, size_t n, const char *path)
{
    buf[0] = 0;
    if (!buf || n < 32 || !path) return false;
    if (bsp_wifi_state() != BSP_WIFI_CONNECTED) return false;
    char ip[20];
    if (bsp_wifi_ip(ip, sizeof(ip)) != ESP_OK) return false;
    if (!ip[0] || strcmp(ip, "0.0.0.0") == 0) return false;
    snprintf(buf, n, "https://%s:%d%s", ip, APP_WEB_HTTPS_PORT, path);
    return true;
}

static void append_url(char *buf, size_t cap, size_t *used, const char *path)
{
    char url[40];
    if (!wifi_url(url, sizeof(url), path) || *used + 8 >= cap) return;
    int n = snprintf(buf + *used, cap - *used, "%s\n", url);
    if (n > 0) *used += (size_t)n;
}

static void paint(void)
{
    if (!s_title) return;
    lv_label_set_text(s_title, app_str(APP_STR_WALKIE));
    bool on = walkie_busy();
    walkie_mode_t mode = on ? walkie_mode() : (walkie_mode_t)app_prefs()->walkie_mode;
    int ch = on ? walkie_channel() : walkie_ch_clamp(app_prefs()->walkie_ch);

    if (on) {
        lv_label_set_text(s_hint, app_str(APP_STR_WALKIE_HINT));
        char buf[360];
        size_t used = 0;
        int n = snprintf(buf, sizeof(buf), "%s  %s\n%s  %d\n%s\n",
                         app_str(APP_STR_WALKIE_MODE),
                         mode == WALKIE_MODE_BLE ? app_str(APP_STR_WALKIE_BLE)
                                                 : app_str(APP_STR_WALKIE_WEBRTC),
                         app_str(APP_STR_WALKIE_CH), ch,
                         run_text(walkie_run()));
        if (n > 0) used = (size_t)n;
        int pn = walkie_peer_n();
        if (pn > 0 && used < sizeof(buf) - 8) {
            n = snprintf(buf + used, sizeof(buf) - used, "%d\n", pn);
            if (n > 0) used += (size_t)n;
            for (int i = 0; i < pn && used < sizeof(buf) - 8; i++) {
                n = snprintf(buf + used, sizeof(buf) - used, "  %s\n", walkie_peer_name(i));
                if (n > 0) used += (size_t)n;
            }
        }
        if (mode == WALKIE_MODE_WEBRTC) append_url(buf, sizeof(buf), &used, "/w");
        lv_label_set_text(s_body, buf);
        app_shell_wake();
        return;
    }

    if (s_err == 1) lv_label_set_text(s_hint, app_str(APP_STR_WALKIE_NEED_WIFI));
    else if (s_err == 2) lv_label_set_text(s_hint, app_str(APP_STR_WALKIE_NEED_BT));
    else if (s_err == 3) lv_label_set_text(s_hint, app_str(APP_STR_WALKIE_FAIL_MEM));
    else if (s_err == 4) lv_label_set_text(s_hint, app_str(APP_STR_WALKIE_FAIL_NET));
    else if (s_err == 5) lv_label_set_text(s_hint, app_str(APP_STR_WALKIE_FAIL_AUDIO));
    else if (s_err == 6) lv_label_set_text(s_hint, app_str(APP_STR_WALKIE_FAIL));
    else lv_label_set_text(s_hint, app_str(APP_STR_HINT_OPEN));

    if (s_sel >= item_n()) s_sel = item_n() - 1;
    char buf[280];
    size_t used = 0;
    int n = snprintf(buf, sizeof(buf),
                     "%s %s  %s\n"
                     "%s %s  %d\n"
                     "%s %s\n",
                     s_sel == 0 ? ">" : " ", app_str(APP_STR_WALKIE_MODE),
                     mode == WALKIE_MODE_BLE ? app_str(APP_STR_WALKIE_BLE)
                                             : app_str(APP_STR_WALKIE_WEBRTC),
                     s_sel == 1 ? ">" : " ", app_str(APP_STR_WALKIE_CH), ch,
                     s_sel == 2 ? ">" : " ", app_str(APP_STR_WALKIE_START));
    if (n > 0) used = (size_t)n;
    if (mode != WALKIE_MODE_BLE) append_url(buf, sizeof(buf), &used, "/w");
    lv_label_set_text(s_body, buf);
}

static void apply_err(void)
{
    if (walkie_busy()) {
        s_err = 0;
        return;
    }
    switch (walkie_last_err()) {
    case WALKIE_E_WIFI:  s_err = 1; break;
    case WALKIE_E_BLE:   s_err = 2; break;
    case WALKIE_E_MEM:   s_err = 3; break;
    case WALKIE_E_NET:   s_err = 4; break;
    case WALKIE_E_AUDIO: s_err = 5; break;
    case WALKIE_E_OK:    break;
    default:             s_err = 6; break;
    }
}

static void tick(lv_timer_t *t)
{
    (void)t;
    if (s_pending && !walkie_busy()) {
        s_pending = false;
        walkie_mode_t mode = (walkie_mode_t)app_prefs()->walkie_mode;
        int ch = walkie_ch_clamp(app_prefs()->walkie_ch);
        s_err = 0;
        walkie_clear_err();
        radio_borrow(mode);
        if (mode == WALKIE_MODE_WEBRTC && bsp_wifi_state() != BSP_WIFI_CONNECTED) {
            s_err = 1;
            radio_restore();
        } else if (walkie_start(mode, ch) != ESP_OK) {
            radio_restore();
            apply_err();
        }
    }
    if (walkie_busy()) s_err = 0;
    else {
        radio_restore();
        if (walkie_last_err() != WALKIE_E_OK) apply_err();
    }
    paint();
}

static void persist(void)
{
    app_prefs()->walkie_mode = (uint8_t)(walkie_busy() ? walkie_mode()
                                                       : app_prefs()->walkie_mode);
    app_prefs()->walkie_ch = (uint8_t)walkie_ch_clamp(app_prefs()->walkie_ch);
    app_prefs_save();
}

void app_walkie_enter(lv_obj_t *p)
{
    s_sel = 0;
    s_err = 0;
    s_pending = false;
    s_parked_ble = false;
    s_stopped_wx = false;
    walkie_clear_err();
    if (!app_prefs()->walkie_ch) app_prefs()->walkie_ch = 1;
    lv_obj_t *card = app_ui_card(p);
    s_title = app_ui_title(card, app_str(APP_STR_WALKIE));
    s_hint = app_ui_hint(card);
    s_body = app_ui_body(card, 44);
    s_timer = lv_timer_create(tick, 250, NULL);
    paint();
}

void app_walkie_exit(void)
{
    walkie_stop();
    radio_restore();
    if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
    s_title = s_hint = s_body = NULL;
}

void app_walkie_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    if (walkie_busy() && btn == BSP_BTN_UP) {
        if (ev == BSP_BTN_PRESS) walkie_set_ptt(true);
        else if (ev == BSP_BTN_RELEASE) walkie_set_ptt(false);
        paint();
        return;
    }
    if (ev != BSP_BTN_CLICK) return;
    if (walkie_busy()) {
        if (btn == BSP_BTN_OK) {
            walkie_stop();
            radio_restore();
            paint();
        }
        return;
    }
    if (btn == BSP_BTN_UP) { app_ui_move(&s_sel, item_n(), -1); paint(); return; }
    if (btn == BSP_BTN_DOWN) { app_ui_move(&s_sel, item_n(), 1); paint(); return; }
    if (btn != BSP_BTN_OK) return;

    if (s_sel == 0) {
        app_prefs()->walkie_mode = app_prefs()->walkie_mode ? 0 : 1;
        walkie_clear_err();
        s_err = 0;
        persist();
        paint();
        return;
    }
    if (s_sel == 1) {
        int ch = walkie_ch_clamp(app_prefs()->walkie_ch) + 1;
        if (ch > WALKIE_CH_MAX) ch = WALKIE_CH_MIN;
        app_prefs()->walkie_ch = (uint8_t)ch;
        persist();
        paint();
        return;
    }

    walkie_mode_t mode = (walkie_mode_t)app_prefs()->walkie_mode;
    s_err = 0;
    walkie_clear_err();
    if (mode == WALKIE_MODE_WEBRTC && bsp_wifi_state() != BSP_WIFI_CONNECTED) {
        s_err = 1;
        paint();
        return;
    }
    s_pending = true;
    paint();
}
