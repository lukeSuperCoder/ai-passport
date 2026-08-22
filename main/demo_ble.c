// main/demo_ble.c —— ANCS 配对、短信关键字筛选、显示验证码。
// 过滤在板上完成。OK 长按返回由 main 拦截。
#include "demo.h"
#include "app_logic.h"
#include "ble_filter.h"
#include "bsp_ble.h"
#include "ui_pixel.h"
#include "lvgl.h"
#include "nvs.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define NVS_NS   "demo_ble"
#define NVS_MODE "mode"

typedef enum {
    FILTER_SMS_KW = 0,   // 仅短信 + 关键字
    FILTER_SMS_ALL,      // 仅短信
    FILTER_ALL_KW,       // 所有通知 + 关键字
    FILTER_COUNT
} filter_mode_t;

static lv_obj_t *s_scr, *s_name, *s_state, *s_code, *s_body, *s_menu, *s_hint;
static lv_timer_t *s_timer;
static filter_mode_t s_mode;
static int s_sel;                 // 0 Filter  1 Unpair
static char s_shown_code[12];
static char s_shown_body[320];
static bsp_ble_state_t s_seen;
static uint32_t s_seen_key;
static int s_seen_conn;
static int s_seen_bonds;

static const char *mode_text(filter_mode_t m) {
    if (m == FILTER_SMS_ALL) return "SMS all";
    if (m == FILTER_ALL_KW) return "All+kw";
    return "SMS+kw";
}

static const char *state_text(bsp_ble_state_t st) {
    switch (st) {
    case BSP_BLE_ADVERTISING: return "Advertising";
    case BSP_BLE_PAIRING:     return "Pairing";
    case BSP_BLE_WAIT_NOTIFY: return "Enable notify";
    case BSP_BLE_CONNECTED:   return "Connected";
    case BSP_BLE_ANCS:        return "ANCS ready";
    default:                  return "Idle";
    }
}

static void load_mode(void) {
    nvs_handle_t h;
    s_mode = FILTER_SMS_KW;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return;
    uint8_t v = 0;
    if (nvs_get_u8(h, NVS_MODE, &v) == ESP_OK && v < FILTER_COUNT) s_mode = v;
    nvs_close(h);
}

static void save_mode(void) {
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_u8(h, NVS_MODE, (uint8_t)s_mode);
    nvs_commit(h);
    nvs_close(h);
}

static bool notif_wanted(const bsp_ble_notif_t *n) {
    char blob[BSP_BLE_TITLE_MAX + BSP_BLE_SUBTITLE_MAX + BSP_BLE_MSG_MAX + 8];
    snprintf(blob, sizeof(blob), "%s %s %s", n->title, n->subtitle, n->message);
    bool sms = ble_filter_is_sms(n->app_id);
    bool kw = ble_filter_has_keyword(blob, NULL);
    if (s_mode == FILTER_SMS_KW) return sms && kw;
    if (s_mode == FILTER_SMS_ALL) return sms;
    return kw;
}

static const char *sms_label(const bsp_ble_notif_t *n) {
    return ble_filter_is_sms(n->app_id) ? "SMS" : "Notify";
}

static void render_menu(void) {
    int bonds = bsp_ble_bond_count();
    if (bonds > 0) {
        lv_label_set_text_fmt(s_menu, "%s Filter  %s\n%s Unpair  %d saved",
                              s_sel == 0 ? ">" : " ",
                              mode_text(s_mode),
                              s_sel == 1 ? ">" : " ",
                              bonds);
    } else {
        lv_label_set_text_fmt(s_menu, "%s Filter  %s\n%s Unpair",
                              s_sel == 0 ? ">" : " ",
                              mode_text(s_mode),
                              s_sel == 1 ? ">" : " ");
    }
}

static void render_hint(bsp_ble_state_t st) {
    int conn = bsp_ble_conn_count();
    int bonds = bsp_ble_bond_count();
    if (st == BSP_BLE_PAIRING) {
        lv_label_set_text(s_hint, "Confirm Pair on phone/Mac");
    } else if (st == BSP_BLE_ADVERTISING) {
        if (bonds > 0) lv_label_set_text(s_hint, "Reconnect or pair another");
        else lv_label_set_text(s_hint, "Toggle iPhone BT, find Passport");
    } else if (st == BSP_BLE_WAIT_NOTIFY) {
        lv_label_set_text(s_hint, "Tap Passport to reconnect");
    } else if (conn < bsp_ble_conn_max()) {
        lv_label_set_text(s_hint, "Another device can still pair");
    } else {
        lv_label_set_text(s_hint, "OK cycles filter");
    }
}

static void apply_match(const bsp_ble_notif_t *n) {
    ble_filter_extract_code(n->message, s_shown_code, sizeof(s_shown_code));
    if (!s_shown_code[0]) ble_filter_extract_code(n->title, s_shown_code, sizeof(s_shown_code));
    if (!s_shown_code[0]) ble_filter_extract_code(n->subtitle, s_shown_code, sizeof(s_shown_code));

    char title[BSP_BLE_TITLE_MAX + 1];
    char subtitle[BSP_BLE_SUBTITLE_MAX + 1];
    char msg[BSP_BLE_MSG_MAX + 1];
    char app[BSP_BLE_APP_NAME_MAX + 1];
    char date[16];
    ui_pixel_utf8_copy(title, sizeof(title), n->title);
    ui_pixel_utf8_copy(subtitle, sizeof(subtitle), n->subtitle);
    ui_pixel_utf8_copy(msg, sizeof(msg), n->message);
    ui_pixel_utf8_copy(app, sizeof(app), n->app_name[0] ? n->app_name : n->app_id);
    if (!title[0]) strlcpy(title, sms_label(n), sizeof(title));

    char meta[80];
    meta[0] = 0;
    bool has_date = app_ancs_date_text(n->date, date, sizeof(date));
    if (app[0] && has_date) snprintf(meta, sizeof(meta), "%s  %s", app, date);
    else if (app[0]) snprintf(meta, sizeof(meta), "%s", app);
    else if (has_date) snprintf(meta, sizeof(meta), "%s", date);

    const char *sub = app_notif_show_subtitle(title, subtitle) ? subtitle : "";
    snprintf(s_shown_body, sizeof(s_shown_body), "%s%s%s%s%s%s%s",
             meta[0] ? meta : "",
             meta[0] ? "\n" : "",
             title[0] ? title : "SMS",
             sub[0] ? "\n" : "",
             sub,
             msg[0] ? "\n" : "",
             msg);
}

static void tick(lv_timer_t *t) {
    (void)t;
    bsp_ble_state_t st = bsp_ble_state();
    uint32_t key = bsp_ble_passkey();
    int conn = bsp_ble_conn_count();
    int bonds = bsp_ble_bond_count();
    lv_label_set_text(s_name, bsp_ble_name());

    if (st != s_seen || key != s_seen_key || conn != s_seen_conn || bonds != s_seen_bonds) {
        s_seen = st;
        s_seen_key = key;
        s_seen_conn = conn;
        s_seen_bonds = bonds;
        if (st == BSP_BLE_PAIRING && key) {
            lv_label_set_text_fmt(s_state, "Pair  %06" PRIu32, key);
            lv_label_set_text_fmt(s_code, "%06" PRIu32, key);
        } else {
            lv_label_set_text_fmt(s_state, "%s  %d/%d", state_text(st), conn, bonds);
            if (!s_shown_code[0]) lv_label_set_text(s_code, "--");
        }
        if (st == BSP_BLE_WAIT_NOTIFY) {
            lv_label_set_text(s_body, "Tap i then tap Passport");
        }
        render_hint(st);
        render_menu();
    }

    bsp_ble_notif_t n;
    if (bsp_ble_take_notif(&n) && notif_wanted(&n)) {
        apply_match(&n);
        lv_label_set_text(s_code, s_shown_code[0] ? s_shown_code : "MSG");
        lv_label_set_text(s_body, s_shown_body);
    }
}

void demo_ble_enter(void) {
    bsp_ble_ensure_advertising();
    load_mode();
    s_sel = 0;
    s_seen = BSP_BLE_IDLE;
    s_seen_key = 0;
    s_seen_conn = -1;
    s_seen_bonds = -1;
    s_shown_code[0] = 0;
    s_shown_body[0] = 0;

    s_scr = ui_pixel_screen_create("BLE");
    lv_obj_t *panel = ui_pixel_panel_create(s_scr, 10, 50, 220, 230, UI_PAPER);

    s_name = lv_label_create(panel);
    lv_obj_set_style_text_font(s_name, ui_pixel_font_14(), 0);
    lv_obj_set_style_text_color(s_name, lv_color_hex(UI_INK), 0);
    lv_obj_align(s_name, LV_ALIGN_TOP_MID, 0, 0);

    s_state = lv_label_create(panel);
    lv_obj_set_style_text_color(s_state, lv_color_hex(UI_SKY_DARK), 0);
    lv_obj_align(s_state, LV_ALIGN_TOP_MID, 0, 22);

    s_code = lv_label_create(panel);
    lv_obj_set_style_text_font(s_code, ui_pixel_font_20(), 0);
    lv_obj_set_style_text_color(s_code, lv_color_hex(UI_INK), 0);
    lv_obj_align(s_code, LV_ALIGN_TOP_MID, 0, 48);
    lv_label_set_text(s_code, "--");

    s_body = lv_label_create(panel);
    lv_obj_set_width(s_body, 190);
    lv_label_set_long_mode(s_body, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(s_body, ui_pixel_font_14(), 0);
    lv_obj_set_style_text_color(s_body, lv_color_hex(UI_INK), 0);
    lv_obj_align(s_body, LV_ALIGN_TOP_MID, 0, 80);
    lv_label_set_text(s_body, "Waiting");

    s_menu = lv_label_create(panel);
    lv_obj_set_style_text_color(s_menu, lv_color_hex(UI_INK), 0);
    lv_obj_align(s_menu, LV_ALIGN_BOTTOM_LEFT, 4, -28);

    s_hint = lv_label_create(panel);
    lv_obj_set_width(s_hint, 190);
    lv_label_set_long_mode(s_hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(s_hint, lv_color_hex(UI_SKY_DARK), 0);
    lv_obj_align(s_hint, LV_ALIGN_BOTTOM_MID, 0, -2);

    render_menu();
    s_timer = lv_timer_create(tick, 250, NULL);
    tick(NULL);
    lv_screen_load(s_scr);
}

void demo_ble_exit(void) {
    if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
    if (s_scr) {
        lv_obj_delete(s_scr);
        s_scr = NULL;
        s_name = s_state = s_code = s_body = s_menu = s_hint = NULL;
    }
}

void demo_ble_key(bsp_btn_t btn, bsp_btn_ev_t ev) {
    if (bsp_ble_pair_needs_confirm() && ev == BSP_BTN_CLICK) {
        if (btn == BSP_BTN_OK) bsp_ble_pair_reply(true);
        if (btn == BSP_BTN_DOWN) bsp_ble_pair_reply(false);
        return;
    }
    if (ev != BSP_BTN_CLICK) return;
    if (bsp_ble_state() == BSP_BLE_WAIT_NOTIFY && btn == BSP_BTN_OK) {
        bsp_ble_resume_advertising();
        lv_label_set_text(s_body, "Reconnecting");
        return;
    }
    if (btn == BSP_BTN_UP || btn == BSP_BTN_DOWN) {
        s_sel = 1 - s_sel;
        render_menu();
        return;
    }
    if (btn != BSP_BTN_OK) return;
    if (s_sel == 0) {
        s_mode = (filter_mode_t)((s_mode + 1) % FILTER_COUNT);
        save_mode();
        render_menu();
    } else {
        bsp_ble_unpair();
        lv_label_set_text(s_body, "Forgot connected");
        s_shown_code[0] = 0;
        lv_label_set_text(s_code, "--");
    }
}
