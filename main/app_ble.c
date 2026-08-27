#include "app.h"
#include "app_i18n.h"
#include "app_ui.h"
#include "bsp_ble.h"
#include "ui_pixel.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static lv_obj_t *s_hint, *s_body;
static lv_timer_t *s_timer;
static int s_sel;
static int s_focus_peer;
static bsp_ble_peer_t s_peers[BSP_BLE_PEER_MAX];
static int s_peer_n;

static const char *state_text(bsp_ble_state_t st)
{
    switch (st) {
    case BSP_BLE_ADVERTISING: return app_str(APP_STR_ST_ADV);
    case BSP_BLE_PAIRING:     return app_str(APP_STR_ST_PAIR);
    case BSP_BLE_WAIT_NOTIFY: return app_str(APP_STR_ST_WAIT);
    case BSP_BLE_CONNECTED:   return app_str(APP_STR_ST_CONN);
    case BSP_BLE_ANCS:        return app_str(APP_STR_ST_ANCS);
    default:                  return app_str(APP_STR_ST_IDLE);
    }
}

static int item_count(void)
{
    return 2 + s_peer_n + 2; // Power, Quiet, peers, Advertise, Forget
}

static void paint(void)
{
    if (!s_hint || !s_body) return;
    s_peer_n = bsp_ble_list_peers(s_peers, BSP_BLE_PEER_MAX);
    int items = item_count();
    if (s_sel >= items) s_sel = items ? items - 1 : 0;
    if (s_sel >= 2 && s_sel < 2 + s_peer_n) s_focus_peer = s_sel - 2;
    if (s_focus_peer >= s_peer_n) s_focus_peer = s_peer_n ? s_peer_n - 1 : 0;

    bsp_ble_state_t st = bsp_ble_state();
    if (!bsp_ble_enabled()) lv_label_set_text(s_hint, app_str(APP_STR_BT_OFF));
    else if (st == BSP_BLE_PAIRING && bsp_ble_passkey()) {
        lv_label_set_text_fmt(s_hint, "%s %06" PRIu32,
                              app_str(APP_STR_BT_CODE), bsp_ble_passkey());
    } else {
        lv_label_set_text_fmt(s_hint, "%s  %d/%d  %s%s",
                              state_text(st),
                              bsp_ble_conn_count(), bsp_ble_bond_count(),
                              bsp_ble_name(),
                              bsp_ble_adv_active() ? app_str(APP_STR_ADV_MARK) : "");
    }

    char buf[700];
    size_t o = 0;
    buf[0] = 0;
    int window = 8;
    int start = s_sel - window / 2;
    if (start < 0) start = 0;
    if (start + window > items) start = items > window ? items - window : 0;

    for (int i = start; i < items && i < start + window; i++) {
        char line[80];
        const char *m = (i == s_sel) ? ">" : " ";
        if (i == 0) {
            snprintf(line, sizeof(line), "%s %s  %s\n", m,
                     app_str(APP_STR_POWER), app_str_onoff(bsp_ble_enabled()));
        } else if (i == 1) {
            snprintf(line, sizeof(line), "%s %s  %s\n", m,
                     app_str(APP_STR_STOP_ADV), app_str_onoff(bsp_ble_quiet()));
        } else if (i < 2 + s_peer_n) {
            int p = i - 2;
            char shown[40];
            const char *src = s_peers[p].name[0] ? s_peers[p].name : s_peers[p].addr;
            ui_pixel_utf8_copy(shown, sizeof(shown), src);
            snprintf(line, sizeof(line), "%s%s %s\n", m,
                     s_peers[p].connected ? "*" : " ",
                     shown);
        } else if (i == 2 + s_peer_n) {
            snprintf(line, sizeof(line), "%s %s  %s\n", m,
                     app_str(APP_STR_ADVERTISE),
                     app_str_onoff(bsp_ble_adv_active()));
        } else {
            snprintf(line, sizeof(line), "%s %s\n", m, app_str(APP_STR_FORGET_SEL));
        }
        size_t ln = strlen(line);
        if (o + ln >= sizeof(buf)) break;
        memcpy(buf + o, line, ln + 1);
        o += ln;
    }
    lv_label_set_text(s_body, buf);
}

static void tick(lv_timer_t *t)
{
    (void)t;
    paint();
}

void app_ble_enter(lv_obj_t *p)
{
    s_sel = 0;
    lv_obj_t *card = app_ui_card(p);
    app_ui_title(card, app_str(APP_STR_BLUETOOTH));
    s_hint = app_ui_hint(card);
    s_body = app_ui_body(card, 44);
    s_timer = lv_timer_create(tick, 250, NULL);
    paint();
}

void app_ble_exit(void)
{
    if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
    s_hint = s_body = NULL;
}

void app_ble_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    if (bsp_ble_pair_needs_confirm() && ev == BSP_BTN_CLICK) {
        if (btn == BSP_BTN_OK) bsp_ble_pair_reply(true);
        if (btn == BSP_BTN_DOWN) bsp_ble_pair_reply(false);
        return;
    }
    if (ev != BSP_BTN_CLICK) return;
    if (btn == BSP_BTN_UP) { app_ui_move(&s_sel, item_count(), -1); paint(); return; }
    if (btn == BSP_BTN_DOWN) { app_ui_move(&s_sel, item_count(), 1); paint(); return; }
    if (btn != BSP_BTN_OK) return;

    if (s_sel == 0) {
        bsp_ble_set_enabled(!bsp_ble_enabled());
    } else if (s_sel == 1) {
        bsp_ble_set_quiet(!bsp_ble_quiet());
    } else if (s_sel < 2 + s_peer_n) {
        s_focus_peer = s_sel - 2;
    } else if (s_sel == 2 + s_peer_n) {
        if (bsp_ble_enabled()) {
            if (bsp_ble_state() == BSP_BLE_WAIT_NOTIFY) {
                bsp_ble_resume_advertising();
            } else {
                bsp_ble_set_advertising(!bsp_ble_adv_active());
            }
        }
    } else if (s_peer_n > 0) {
        bsp_ble_forget_at(s_focus_peer);
    } else {
        bsp_ble_unpair();
    }
    paint();
}
