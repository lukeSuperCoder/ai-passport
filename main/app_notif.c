#include "app_notif.h"

#include "app.h"
#include "app_i18n.h"
#include "app_logic.h"
#include "app_prefs.h"
#include "app_tone.h"
#include "ble_filter.h"
#include "bsp_ble.h"
#include "ui_pixel.h"

#include <stdio.h>
#include <string.h>

static lv_obj_t *s_box, *s_title, *s_meta, *s_code, *s_scroll, *s_body, *s_hint;
static app_notif_q_t s_q;
static app_log_t s_log;
static bool s_shown;
static bool s_pairing;
static int s_left_ms;
static int s_hint_sec = -1;
static uint32_t s_seen_key;

static void raise(void)
{
    if (s_box) lv_obj_move_foreground(s_box);
}

static void paint_hint(void)
{
    if (!s_hint || s_pairing) return;
    if (s_left_ms <= 0) {
        if (s_hint_sec != 0) {
            lv_label_set_text(s_hint, app_str(APP_STR_OK_CLOSE));
            s_hint_sec = 0;
        }
        return;
    }
    int sec = (s_left_ms + 999) / 1000;
    if (sec < 1) sec = 1;
    if (sec == s_hint_sec) return;
    s_hint_sec = sec;
    lv_label_set_text_fmt(s_hint, app_str(APP_STR_OK_CLOSE_AUTO), sec);
}

static void layout_body(void)
{
    if (!s_box || !s_scroll) return;
    lv_obj_update_layout(s_box);
    int inner_h = (int)lv_obj_get_content_height(s_box);
    int inner_w = (int)lv_obj_get_content_width(s_box);
    if (inner_h < 80) inner_h = 248;
    if (inner_w < 80) inner_w = 196;

    int y = 0;
    if (s_title) {
        lv_obj_align(s_title, LV_ALIGN_TOP_LEFT, 0, 0);
        lv_obj_update_layout(s_title);
        y = (int)lv_obj_get_height(s_title);
    }
    if (s_meta && !lv_obj_has_flag(s_meta, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_set_pos(s_meta, 0, y + 4);
        lv_obj_update_layout(s_meta);
        y += 4 + (int)lv_obj_get_height(s_meta) + 6;
    } else {
        y += 8;
    }
    if (s_code && !lv_obj_has_flag(s_code, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_set_pos(s_code, 0, y);
        lv_obj_update_layout(s_code);
        y += (int)lv_obj_get_height(s_code) + 8;
    }

    int hint_h = 20;
    int h = inner_h - y - hint_h;
    if (h < 36) h = 36;
    lv_obj_set_pos(s_scroll, 0, y);
    lv_obj_set_size(s_scroll, inner_w, h);
    if (s_body) lv_obj_set_width(s_body, inner_w);
    lv_obj_scroll_to_y(s_scroll, 0, LV_ANIM_OFF);
}

static void scroll_body(int dir)
{
    if (!s_scroll || s_pairing) return;
    lv_obj_scroll_by(s_scroll, 0, dir * 24, LV_ANIM_OFF);
    if (s_left_ms > 0) {
        int sec = app_prefs()->auto_hide;
        if (sec > 0) {
            s_left_ms = sec * 1000;
            s_hint_sec = -1;
            paint_hint();
        }
    }
}

static void hide(void)
{
    if (s_box) lv_obj_add_flag(s_box, LV_OBJ_FLAG_HIDDEN);
    s_shown = false;
    s_pairing = false;
    s_left_ms = 0;
    s_hint_sec = -1;
}

static void paint_pairing(uint32_t key)
{
    s_pairing = true;
    s_shown = true;
    s_left_ms = 0;
    lv_obj_set_style_bg_color(s_box, lv_color_hex(UI_PAPER), 0);
    lv_obj_set_style_text_color(s_title, lv_color_hex(UI_INK), 0);
    lv_obj_set_style_text_color(s_meta, lv_color_hex(UI_SKY_DARK), 0);
    lv_obj_set_style_text_color(s_body, lv_color_hex(UI_INK), 0);
    lv_obj_set_style_text_color(s_hint, lv_color_hex(UI_SKY_DARK), 0);
    lv_label_set_text(s_title, app_str(APP_STR_PAIRING));
    lv_obj_add_flag(s_meta, LV_OBJ_FLAG_HIDDEN);
    if (s_code) lv_obj_add_flag(s_code, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text_fmt(s_body, "%06lu", (unsigned long)key);
    lv_label_set_text(s_hint, app_str(APP_STR_PAIR_CONFIRM));
    layout_body();
    lv_obj_remove_flag(s_box, LV_OBJ_FLAG_HIDDEN);
    raise();
}

static void paint_notif(const app_notif_item_t *it)
{
    s_pairing = false;
    s_shown = true;
    bool high = it->prio == APP_PRIO_HIGH;
    lv_obj_set_style_bg_color(s_box, lv_color_hex(high ? UI_RED : UI_PAPER), 0);
    uint32_t fg = high ? 0xFFFFFF : UI_INK;
    uint32_t meta_fg = high ? 0xFFD0D0 : UI_SKY_DARK;
    lv_obj_set_style_text_color(s_title, lv_color_hex(fg), 0);
    lv_obj_set_style_text_color(s_meta, lv_color_hex(meta_fg), 0);
    lv_obj_set_style_text_color(s_body, lv_color_hex(fg), 0);
    lv_obj_set_style_text_color(s_hint, lv_color_hex(meta_fg), 0);

    char title[64];
    char app_name[40];
    char subtitle[64];
    char msg[160];
    char date[16];
    ui_pixel_utf8_copy(title, sizeof(title), it->title);
    ui_pixel_utf8_copy(app_name, sizeof(app_name), it->app_name);
    ui_pixel_utf8_copy(subtitle, sizeof(subtitle), it->subtitle);
    ui_pixel_utf8_copy(msg, sizeof(msg), it->message);
    bool has_date = app_ancs_date_text(it->date, date, sizeof(date));
    bool title_from_app = !title[0] && app_name[0];
    if (title_from_app) ui_pixel_utf8_copy(title, sizeof(title), app_name);
    lv_obj_align(s_title, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_label_set_text(s_title, title[0] ? title : app_str(APP_STR_ALERT));

    bool show_sub = app_notif_show_subtitle(title, subtitle);
    if (show_sub && msg[0]) {
        char body[230];
        snprintf(body, sizeof(body), "%s\n%s", subtitle, msg);
        lv_label_set_text(s_body, body);
    } else if (show_sub) {
        lv_label_set_text(s_body, subtitle);
    } else {
        lv_label_set_text(s_body, msg[0] ? msg : " ");
    }

    char meta[80];
    meta[0] = 0;
    const char *meta_app = title_from_app ? "" : app_name;
    if (meta_app[0] && has_date) snprintf(meta, sizeof(meta), "%s  %s", meta_app, date);
    else if (meta_app[0]) snprintf(meta, sizeof(meta), "%s", meta_app);
    else if (has_date) snprintf(meta, sizeof(meta), "%s", date);
    if (meta[0]) {
        lv_label_set_text(s_meta, meta);
        lv_obj_remove_flag(s_meta, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_meta, LV_OBJ_FLAG_HIDDEN);
    }

    char otp[12];
    ble_filter_pick_code(it->title, it->subtitle, it->message, otp, sizeof(otp));
    if (s_code) {
        if (otp[0]) {
            lv_label_set_text(s_code, otp);
            lv_obj_remove_flag(s_code, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_code, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (high) {
        s_left_ms = 0;
    } else {
        int sec = app_prefs()->auto_hide;
        s_left_ms = sec > 0 ? sec * 1000 : 0;
    }
    s_hint_sec = -1;
    paint_hint();
    layout_body();
    lv_obj_remove_flag(s_box, LV_OBJ_FLAG_HIDDEN);
    raise();
}

static void show_front(void)
{
    const app_notif_item_t *it = app_notif_q_front(&s_q);
    if (!it) {
        hide();
        return;
    }
    paint_notif(it);
}

static void dismiss(void)
{
    if (s_pairing) return;
    app_notif_q_pop(&s_q);
    show_front();
}

void app_notif_init(lv_obj_t *screen)
{
    app_notif_q_init(&s_q);
    app_log_init(&s_log);
    s_box = lv_obj_create(screen);
    ui_pixel_strip_theme(s_box);
    lv_obj_set_pos(s_box, 10, APP_HEADER_H + 10);
    lv_obj_set_size(s_box, 220, 320 - APP_HEADER_H - 20);
    lv_obj_set_style_border_width(s_box, 4, 0);
    lv_obj_set_style_border_color(s_box, lv_color_hex(UI_INK), 0);
    lv_obj_set_style_pad_all(s_box, 10, 0);
    lv_obj_set_style_bg_opa(s_box, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(s_box, lv_color_hex(UI_PAPER), 0);

    s_title = lv_label_create(s_box);
    lv_obj_set_style_text_font(s_title, ui_pixel_font_20(), 0);
    lv_obj_set_width(s_title, 196);
    lv_label_set_long_mode(s_title, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_title, LV_ALIGN_TOP_LEFT, 0, 0);

    s_meta = lv_label_create(s_box);
    lv_obj_set_style_text_font(s_meta, ui_pixel_font_14(), 0);
    lv_obj_set_width(s_meta, 196);
    lv_label_set_long_mode(s_meta, LV_LABEL_LONG_CLIP);
    lv_obj_align(s_meta, LV_ALIGN_TOP_LEFT, 0, 36);
    lv_obj_add_flag(s_meta, LV_OBJ_FLAG_HIDDEN);

    s_code = lv_label_create(s_box);
    lv_obj_set_style_text_font(s_code, ui_pixel_font_20(), 0);
    lv_obj_set_style_text_color(s_code, lv_color_hex(UI_INK), 0);
    lv_obj_set_style_bg_opa(s_code, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(s_code, lv_color_hex(UI_YELLOW), 0);
    lv_obj_set_style_pad_hor(s_code, 8, 0);
    lv_obj_set_style_pad_ver(s_code, 4, 0);
    lv_obj_set_style_border_width(s_code, 4, 0);
    lv_obj_set_style_border_color(s_code, lv_color_hex(UI_INK), 0);
    lv_obj_add_flag(s_code, LV_OBJ_FLAG_HIDDEN);

    s_scroll = lv_obj_create(s_box);
    ui_pixel_strip_theme(s_scroll);
    lv_obj_add_flag(s_scroll, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(s_scroll, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_scroll, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_opa(s_scroll, LV_OPA_TRANSP, 0);
    lv_obj_set_pos(s_scroll, 0, 40);
    lv_obj_set_size(s_scroll, 196, 180);

    s_body = lv_label_create(s_scroll);
    lv_obj_set_style_text_font(s_body, ui_pixel_font_14(), 0);
    lv_obj_set_width(s_body, 196);
    lv_label_set_long_mode(s_body, LV_LABEL_LONG_WRAP);

    s_hint = lv_label_create(s_box);
    lv_obj_set_style_text_font(s_hint, ui_pixel_font_14(), 0);
    lv_obj_align(s_hint, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    hide();
}

void app_notif_poll(void)
{
    bsp_ble_state_t st = bsp_ble_state();
    uint32_t key = bsp_ble_passkey();
    if (st == BSP_BLE_PAIRING && key) {
        if (!s_pairing || key != s_seen_key) paint_pairing(key);
        s_seen_key = key;
        raise();
        app_shell_wake();
        return;
    }
    if (s_pairing) {
        hide();
        show_front();
    }
    s_seen_key = 0;

    bsp_ble_notif_t n;
    while (bsp_ble_take_notif(&n)) {
        app_log_item_t rec;
        memset(&rec, 0, sizeof(rec));
        ui_pixel_utf8_copy(rec.app_id, sizeof(rec.app_id), n.app_id);
        ui_pixel_utf8_copy(rec.app_name, sizeof(rec.app_name), n.app_name);
        ui_pixel_utf8_copy(rec.title, sizeof(rec.title), n.title);
        ui_pixel_utf8_copy(rec.subtitle, sizeof(rec.subtitle), n.subtitle);
        ui_pixel_utf8_copy(rec.message, sizeof(rec.message), n.message);
        ui_pixel_utf8_copy(rec.date, sizeof(rec.date), n.date);
        rec.category = n.category;
        app_log_push(&s_log, &rec);

        char blob[BSP_BLE_TITLE_MAX + BSP_BLE_SUBTITLE_MAX + BSP_BLE_MSG_MAX + 8];
        snprintf(blob, sizeof(blob), "%s %s %s", n.title, n.subtitle, n.message);
        int prio = app_kw_match(blob, app_prefs()->kw, app_prefs()->kw_n);
        if (prio < 0) continue;
        app_notif_item_t it;
        memset(&it, 0, sizeof(it));
        ui_pixel_utf8_copy(it.title, sizeof(it.title), n.title);
        ui_pixel_utf8_copy(it.subtitle, sizeof(it.subtitle), n.subtitle);
        ui_pixel_utf8_copy(it.message, sizeof(it.message), n.message);
        ui_pixel_utf8_copy(it.app_name, sizeof(it.app_name), n.app_name);
        ui_pixel_utf8_copy(it.date, sizeof(it.date), n.date);
        it.prio = (uint8_t)prio;
        app_notif_q_push(&s_q, &it);
        app_tone_play(prio == APP_PRIO_HIGH ? (int)app_prefs()->tone_alert
                                            : (int)app_prefs()->tone_msg);
    }

    if (!s_shown && app_notif_q_front(&s_q)) show_front();
    if (s_shown && app_shell_asleep()) app_shell_wake();
}

bool app_notif_visible(void)
{
    return s_shown;
}

bool app_notif_pairing(void)
{
    return s_pairing;
}

bool app_notif_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    if (!s_shown) return false;
    if (s_pairing) return true;
    if (btn == BSP_BTN_UP || btn == BSP_BTN_DOWN) {
        if (ev == BSP_BTN_PRESS || ev == BSP_BTN_LONG) {
            scroll_body(btn == BSP_BTN_DOWN ? 1 : -1);
        }
        return true;
    }
    if (ev != BSP_BTN_CLICK) return true;
    if (btn == BSP_BTN_OK) dismiss();
    return true;
}

void app_notif_tick(uint32_t ms)
{
    if (!s_shown || s_pairing || s_left_ms <= 0) return;
    s_left_ms -= (int)ms;
    if (s_left_ms <= 0) {
        dismiss();
        return;
    }
    paint_hint();
}

const app_log_t *app_notif_log(void)
{
    return &s_log;
}

bool app_notif_log_remove(int newest_i)
{
    return app_log_remove(&s_log, newest_i);
}

void app_notif_log_clear(void)
{
    app_log_clear(&s_log);
}
