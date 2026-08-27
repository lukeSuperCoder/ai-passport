#include "app.h"
#include "app_i18n.h"
#include "app_logic.h"
#include "app_notif.h"
#include "app_prefs.h"
#include "app_ui.h"
#include "app_web.h"
#include "ble_filter.h"
#include "bsp_wifi.h"
#include "ui_pixel.h"

#include <stdio.h>
#include <string.h>

static const char *const PRESETS[] = {
    "code", "otp", "verify",
    "\xE9\xAA\x8C\xE8\xAF\x81\xE7\xA0\x81",  // 验证码
    "\xE6\xA0\xA1\xE9\xAA\x8C\xE7\xA0\x81",  // 校验码
    "\xE5\x8A\xA8\xE6\x80\x81\xE7\xA0\x81",  // 动态码
};
#define PRESET_N 6
#define RECENT_N 10

#define CARD_X       10
#define CARD_W       220
#define LIST_Y       44
#define LIST_H       244
#define GAP          6
#define BOX_PAD      8
#define BOX_BORDER   4
#define BOX_CHROME   (BOX_PAD * 2 + BOX_BORDER * 2)
#define TITLE_H      28
#define META_H       16
#define CODE_H       44
#define ROW_GAP      4
#define BODY_H       36
#define COLLAPSED_H      (BOX_CHROME + TITLE_H + ROW_GAP + META_H)
#define COLLAPSED_OTP_H  (BOX_CHROME + TITLE_H + ROW_GAP + CODE_H)
#define EXPANDED_H       (BOX_CHROME + TITLE_H + ROW_GAP + META_H + ROW_GAP + BODY_H)
#define EXPANDED_OTP_H   (BOX_CHROME + TITLE_H + ROW_GAP + META_H + ROW_GAP + CODE_H + ROW_GAP + BODY_H)
#define SETTINGS_H   50
#define VIS_MAX      4

typedef enum {
    VIEW_RECENT = 0,
    VIEW_SET,
    VIEW_PRESET,
    VIEW_KB
} view_t;

typedef struct {
    lv_obj_t *box;
    lv_obj_t *title;
    lv_obj_t *meta;
    lv_obj_t *code;
    lv_obj_t *body;
} vis_t;

static lv_obj_t *s_page, *s_title, *s_hint, *s_body;
static lv_obj_t *s_recent, *s_form, *s_list;
static lv_obj_t *s_rtitle, *s_rhint, *s_ftitle, *s_fhint, *s_fbody;
static lv_obj_t *s_set_box, *s_set_lab;
static vis_t s_vis[VIS_MAX];
static view_t s_view;
static int s_sel, s_preset_sel, s_kb_sel, s_kb_set;
static char s_custom[APP_KW_LEN + 1];
static int s_hold_btn = -1;
static int s_hold_ms;
static lv_timer_t *s_hold_timer;
/* LVGL 任务栈只有 5KB,设置/键盘页共用这块静态缓冲,不要改回栈上大数组。 */
static char s_paint[900];

static const uint8_t HIDE_OPTS[] = { 0, 5, 10, 20 };

static int hide_idx(uint8_t v)
{
    for (int i = 0; i < 4; i++) if (HIDE_OPTS[i] == v) return i;
    return 2;
}

static bool kw_has(const char *t)
{
    app_prefs_t *p = app_prefs();
    for (int i = 0; i < p->kw_n; i++) {
        if (strcmp(p->kw[i].text, t) == 0) return true;
    }
    return false;
}

static int recent_n(void)
{
    int n = app_log_count(app_notif_log());
    if (n > RECENT_N) n = RECENT_N;
    return n;
}

static int set_n(void)
{
    return 1 + 2 + app_prefs()->kw_n + 1; // back, +Preset +Custom, kws, Auto-hide
}

static int list_n(void)
{
    if (s_view == VIEW_RECENT) return 1 + recent_n();
    if (s_view == VIEW_SET) return set_n();
    return 1;
}

static void add_kw(const char *t, uint8_t prio)
{
    app_prefs_t *p = app_prefs();
    if (!t || !t[0] || p->kw_n >= APP_KW_MAX || kw_has(t)) return;
    ui_pixel_utf8_copy(p->kw[p->kw_n].text, sizeof(p->kw[0].text), t);
    p->kw[p->kw_n].prio = prio;
    p->kw_n++;
    app_prefs_save();
}

static void show_only(lv_obj_t *keep)
{
    if (s_recent) {
        if (keep == s_recent) lv_obj_remove_flag(s_recent, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(s_recent, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_form) {
        if (keep == s_form) lv_obj_remove_flag(s_form, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(s_form, LV_OBJ_FLAG_HIDDEN);
    }
}

static void ensure_form_chrome(void)
{
    if (s_form || !s_page) return;
    s_form = app_ui_card(s_page);
    if (!s_form) return;
    lv_obj_add_flag(s_form, LV_OBJ_FLAG_HIDDEN);
    s_ftitle = app_ui_title(s_form, app_str(APP_STR_SETTINGS));
    s_fhint = app_ui_hint(s_form);
    s_fbody = app_ui_body(s_form, 44);
}

static void ensure_recent_chrome(void)
{
    if (s_recent || !s_page) return;

    s_recent = lv_obj_create(s_page);
    if (!s_recent) return;
    ui_pixel_strip_theme(s_recent);
    lv_obj_set_pos(s_recent, 0, 0);
    lv_obj_set_size(s_recent, 240, 288);
    lv_obj_set_style_bg_opa(s_recent, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(s_recent, lv_color_hex(UI_SKY), 0);

    s_rtitle = lv_label_create(s_recent);
    if (s_rtitle) {
        lv_obj_set_style_text_font(s_rtitle, ui_pixel_font_20(), 0);
        lv_obj_set_style_text_color(s_rtitle, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_pos(s_rtitle, CARD_X, 4);
    }

    s_rhint = lv_label_create(s_recent);
    if (s_rhint) {
        lv_obj_set_style_text_font(s_rhint, ui_pixel_font_14(), 0);
        lv_obj_set_style_text_color(s_rhint, lv_color_hex(UI_PAPER), 0);
        lv_obj_set_width(s_rhint, CARD_W);
        lv_label_set_long_mode(s_rhint, LV_LABEL_LONG_CLIP);
        lv_obj_set_pos(s_rhint, CARD_X, 26);
    }

    s_list = lv_obj_create(s_recent);
    if (!s_list) return;
    ui_pixel_strip_theme(s_list);
    lv_obj_set_pos(s_list, 0, LIST_Y);
    lv_obj_set_size(s_list, 240, LIST_H);
    lv_obj_set_style_bg_opa(s_list, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(s_list, lv_color_hex(UI_SKY), 0);
}

static bool item_high(const app_log_item_t *it)
{
    if (!it) return false;
    char blob[APP_LOG_TITLE + APP_LOG_SUB + APP_LOG_MSG + 8];
    snprintf(blob, sizeof(blob), "%s %s %s", it->title, it->subtitle, it->message);
    return app_kw_match(blob, app_prefs()->kw, app_prefs()->kw_n) == APP_PRIO_HIGH;
}

static bool item_has_otp(const app_log_item_t *it)
{
    char otp[12];
    if (!it) return false;
    ble_filter_pick_code(it->title, it->subtitle, it->message, otp, sizeof(otp));
    return otp[0] != 0;
}

static int row_h(int i, int rec)
{
    if (i >= rec) return SETTINGS_H;
    bool otp = item_has_otp(app_log_at(app_notif_log(), i));
    if (i == s_sel) return otp ? EXPANDED_OTP_H : EXPANDED_H;
    return otp ? COLLAPSED_OTP_H : COLLAPSED_H;
}

static lv_obj_t *make_box(lv_obj_t *p, int y, int h, uint32_t bg, uint32_t border)
{
    if (!p) return NULL;
    lv_obj_t *o = lv_obj_create(p);
    if (!o) return NULL;
    ui_pixel_strip_theme(o);
    lv_obj_set_pos(o, CARD_X, y);
    lv_obj_set_size(o, CARD_W, h);
    lv_obj_set_style_border_width(o, BOX_BORDER, 0);
    lv_obj_set_style_border_color(o, lv_color_hex(border), 0);
    lv_obj_set_style_pad_all(o, BOX_PAD, 0);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(bg), 0);
    return o;
}

static lv_obj_t *make_lab(lv_obj_t *p, const lv_font_t *font, uint32_t color,
                          int w, lv_label_long_mode_t mode)
{
    if (!p) return NULL;
    lv_obj_t *l = lv_label_create(p);
    if (!l) return NULL;
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_obj_set_width(l, w);
    lv_label_set_long_mode(l, mode);
    return l;
}

/* 字段排布与弹出通知一致:大标题、应用+时间、副标题/正文。 */
static void fill_notif(const app_log_item_t *it, bool open, char *title, size_t tn,
                       char *meta, size_t mn, char *body, size_t bn)
{
    title[0] = 0;
    meta[0] = 0;
    body[0] = 0;
    if (!it) {
        ui_pixel_utf8_copy(title, tn, app_str(APP_STR_LOG_UNKNOWN));
        return;
    }

    char app_name[40];
    char date[16];
    ui_pixel_utf8_copy(title, tn, it->title);
    ui_pixel_utf8_copy(app_name, sizeof(app_name), it->app_name[0] ? it->app_name : "");
    if (!app_name[0]) app_log_app_label(it, app_name, sizeof(app_name));
    const char *sub = it->subtitle;
    const char *msg = it->message;
    bool has_date = app_ancs_date_text(it->date, date, sizeof(date));
    bool title_from_app = !title[0] && app_name[0];
    if (title_from_app) ui_pixel_utf8_copy(title, tn, app_name);
    if (!title[0]) ui_pixel_utf8_copy(title, tn, app_str(APP_STR_ALERT));

    const char *meta_app = title_from_app ? "" : app_name;
    if (meta_app[0] && has_date) snprintf(meta, mn, "%s  %s", meta_app, date);
    else if (meta_app[0]) snprintf(meta, mn, "%s", meta_app);
    else if (has_date) snprintf(meta, mn, "%s", date);

    if (!open) return;
    bool show_sub = app_notif_show_subtitle(title, sub);
    if (show_sub && msg[0]) snprintf(body, bn, "%s\n%s", sub, msg);
    else if (show_sub) snprintf(body, bn, "%s", sub);
    else if (msg[0]) snprintf(body, bn, "%s", msg);
}

static char s_ntitle[64];
static char s_nmeta[80];
static char s_nbody[230];

static vis_t *vis_get(int slot)
{
    if (slot < 0 || slot >= VIS_MAX || !s_list) return NULL;
    vis_t *v = &s_vis[slot];
    if (v->box) return v;
    v->box = make_box(s_list, 0, COLLAPSED_H, UI_PAPER, UI_INK);
    if (!v->box) return NULL;
    v->title = make_lab(v->box, ui_pixel_font_20(), UI_INK, 196, LV_LABEL_LONG_CLIP);
    v->meta = make_lab(v->box, ui_pixel_font_14(), UI_SKY_DARK, 196, LV_LABEL_LONG_CLIP);
    v->code = make_lab(v->box, ui_pixel_font_20(), UI_INK, LV_SIZE_CONTENT, LV_LABEL_LONG_CLIP);
    v->body = make_lab(v->box, ui_pixel_font_14(), UI_INK, 196, LV_LABEL_LONG_WRAP);
    if (v->title) {
        lv_obj_set_pos(v->title, 0, 0);
        lv_obj_set_height(v->title, TITLE_H);
    }
    if (v->meta) {
        lv_obj_set_pos(v->meta, 0, TITLE_H + ROW_GAP);
        lv_obj_set_height(v->meta, META_H);
    }
    if (v->code) {
        lv_obj_set_style_bg_opa(v->code, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(v->code, lv_color_hex(UI_YELLOW), 0);
        lv_obj_set_style_pad_hor(v->code, 8, 0);
        lv_obj_set_style_pad_ver(v->code, 4, 0);
        lv_obj_set_style_border_width(v->code, 4, 0);
        lv_obj_set_style_border_color(v->code, lv_color_hex(UI_INK), 0);
        lv_obj_set_pos(v->code, 0, TITLE_H + ROW_GAP);
        lv_obj_add_flag(v->code, LV_OBJ_FLAG_HIDDEN);
    }
    if (v->body) {
        lv_obj_set_pos(v->body, 0, TITLE_H + ROW_GAP + META_H + ROW_GAP);
        lv_obj_add_flag(v->body, LV_OBJ_FLAG_HIDDEN);
    }
    return v;
}

static void vis_hide_from(int slot)
{
    for (int i = slot; i < VIS_MAX; i++) {
        if (s_vis[i].box) lv_obj_add_flag(s_vis[i].box, LV_OBJ_FLAG_HIDDEN);
    }
}

static void fill_vis(int slot, int y, int h, const app_log_item_t *it, bool sel)
{
    vis_t *v = vis_get(slot);
    if (!v || !v->box) return;

    bool high = item_high(it);
    uint32_t bg = high ? UI_RED : (sel ? UI_YELLOW : UI_PAPER);
    uint32_t fg = high ? 0xFFFFFF : UI_INK;
    uint32_t meta_fg = high ? 0xFFD0D0 : UI_SKY_DARK;
    uint32_t border = sel ? 0xFFFFFF : UI_INK;

    lv_obj_remove_flag(v->box, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(v->box, CARD_X, y);
    lv_obj_set_size(v->box, CARD_W, h);
    lv_obj_set_style_bg_color(v->box, lv_color_hex(bg), 0);
    lv_obj_set_style_border_color(v->box, lv_color_hex(border), 0);

    fill_notif(it, sel, s_ntitle, sizeof(s_ntitle),
               s_nmeta, sizeof(s_nmeta), s_nbody, sizeof(s_nbody));

    char otp[12];
    ble_filter_pick_code(it ? it->title : NULL, it ? it->subtitle : NULL,
                         it ? it->message : NULL, otp, sizeof(otp));
    bool has_otp = otp[0] != 0;

    int cy = 0;
    if (v->title) {
        lv_label_set_long_mode(v->title, LV_LABEL_LONG_CLIP);
        lv_label_set_text(v->title, s_ntitle);
        lv_obj_set_style_text_color(v->title, lv_color_hex(fg), 0);
        lv_obj_set_pos(v->title, 0, cy);
        lv_obj_set_height(v->title, TITLE_H);
    }
    cy += TITLE_H + ROW_GAP;

    if (v->meta) {
        lv_label_set_text(v->meta, s_nmeta[0] ? s_nmeta : " ");
        lv_obj_set_style_text_color(v->meta, lv_color_hex(meta_fg), 0);
        if (sel || !has_otp) {
            lv_obj_set_pos(v->meta, 0, cy);
            lv_obj_set_height(v->meta, META_H);
            lv_obj_remove_flag(v->meta, LV_OBJ_FLAG_HIDDEN);
            cy += META_H + ROW_GAP;
        } else {
            lv_obj_add_flag(v->meta, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (v->code) {
        if (has_otp) {
            lv_label_set_text(v->code, otp);
            lv_obj_set_style_bg_color(v->code,
                lv_color_hex((sel && !high) ? 0xFFFFFF : UI_YELLOW), 0);
            lv_obj_set_pos(v->code, 0, cy);
            lv_obj_remove_flag(v->code, LV_OBJ_FLAG_HIDDEN);
            cy += CODE_H + ROW_GAP;
        } else {
            lv_obj_add_flag(v->code, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (v->body) {
        if (sel && s_nbody[0]) {
            int body_h = h - BOX_CHROME - cy;
            if (body_h < META_H) body_h = META_H;
            lv_label_set_text(v->body, s_nbody);
            lv_obj_set_style_text_color(v->body, lv_color_hex(fg), 0);
            lv_obj_set_pos(v->body, 0, cy);
            lv_obj_set_height(v->body, body_h);
            lv_obj_remove_flag(v->body, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(v->body, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void fill_set(int y, bool sel)
{
    if (!s_set_box) {
        s_set_box = make_box(s_list, y, SETTINGS_H,
                             sel ? UI_YELLOW : UI_PAPER, sel ? 0xFFFFFF : UI_INK);
        if (!s_set_box) return;
        s_set_lab = make_lab(s_set_box, ui_pixel_font_20(), UI_INK, 196, LV_LABEL_LONG_CLIP);
        if (s_set_lab) {
            lv_label_set_text(s_set_lab, app_str(APP_STR_SETTINGS));
            lv_obj_center(s_set_lab);
        }
    }
    lv_obj_remove_flag(s_set_box, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_y(s_set_box, y);
    lv_obj_set_style_bg_color(s_set_box, lv_color_hex(sel ? UI_YELLOW : UI_PAPER), 0);
    lv_obj_set_style_border_color(s_set_box,
        lv_color_hex(sel ? 0xFFFFFF : UI_INK), 0);
    if (s_set_lab) lv_label_set_text(s_set_lab, app_str(APP_STR_SETTINGS));
}

static void paint_recent(void)
{
    ensure_recent_chrome();
    show_only(s_recent);
    s_title = s_rtitle;
    s_hint = s_rhint;
    s_body = NULL;
    if (s_title) lv_label_set_text(s_title, app_str(APP_STR_ALERTS));

    int rec = recent_n();
    int n = rec + 1;
    if (s_sel >= n) s_sel = n ? n - 1 : 0;
    if (s_sel < 0) s_sel = 0;

    if (s_hint) {
        if (rec == 0) lv_label_set_text(s_hint, app_str(APP_STR_LOG_EMPTY));
        else if (s_sel < rec) lv_label_set_text(s_hint, app_str(APP_STR_HOLD_DELETE));
        else lv_label_set_text(s_hint, app_str(APP_STR_HINT_OPEN));
    }

    int start = s_sel;
    int end = s_sel;
    int used = row_h(s_sel, rec);
    while (end + 1 < n) {
        int next = used + GAP + row_h(end + 1, rec);
        if (next > LIST_H) break;
        end++;
        used = next;
    }
    while (start > 0) {
        int next = used + GAP + row_h(start - 1, rec);
        if (next > LIST_H) break;
        start--;
        used = next;
    }

    int slot = 0;
    int y = 0;
    bool set_shown = false;
    for (int i = start; i <= end; i++) {
        int h = row_h(i, rec);
        if (i < rec) {
            fill_vis(slot++, y, h, app_log_at(app_notif_log(), i), i == s_sel);
        } else {
            fill_set(y, i == s_sel);
            set_shown = true;
        }
        y += h + GAP;
    }
    vis_hide_from(slot);
    if (!set_shown && s_set_box) lv_obj_add_flag(s_set_box, LV_OBJ_FLAG_HIDDEN);
    if (s_page) lv_obj_invalidate(s_page);
}

static void paint_set(void)
{
    app_prefs_t *p = app_prefs();
    if (s_hint) lv_label_set_text(s_hint, app_str(APP_STR_ANC_HINT));
    int n = set_n();
    if (s_sel >= n) s_sel = n ? n - 1 : 0;
    if (s_sel < 0) s_sel = 0;
    int window = 8;
    int start = s_sel - window / 2;
    if (start < 0) start = 0;
    if (start + window > n) start = n > window ? n - window : 0;

    size_t used = 0;
    s_paint[0] = 0;
    for (int i = start; i < n && i < start + window; i++) {
        char line[72];
        const char *m = (i == s_sel) ? ">" : " ";
        if (i == 0) snprintf(line, sizeof(line), "%s %s\n", m, app_str(APP_STR_LOG_BACK));
        else if (i == 1) snprintf(line, sizeof(line), "%s %s\n", m, app_str(APP_STR_PRESET));
        else if (i == 2) snprintf(line, sizeof(line), "%s %s\n", m, app_str(APP_STR_CUSTOM));
        else if (i < 3 + p->kw_n) {
            int k = i - 3;
            snprintf(line, sizeof(line), "%s %s  %s\n", m,
                     p->kw[k].text,
                     p->kw[k].prio ? "!" : "N");
        } else {
            uint8_t h = p->auto_hide;
            if (h == 0) snprintf(line, sizeof(line), "%s %s\n", m,
                                 app_str(APP_STR_AUTOHIDE_OFF));
            else snprintf(line, sizeof(line), "%s %s  %ds\n", m,
                          app_str(APP_STR_AUTOHIDE), (int)h);
        }
        size_t ln = strlen(line);
        if (used + ln >= sizeof(s_paint)) break;
        memcpy(s_paint + used, line, ln + 1);
        used += ln;
    }
    if (s_body) lv_label_set_text(s_body, s_paint);
}

static void paint(void)
{
    if (!s_page) return;

    if (s_view == VIEW_RECENT) {
        paint_recent();
        app_web_clear_target();
        return;
    }

    ensure_form_chrome();
    show_only(s_form);
    s_title = s_ftitle;
    s_hint = s_fhint;
    s_body = s_fbody;
    if (!s_body) return;
    if (s_title) lv_label_set_text(s_title, app_str(APP_STR_SETTINGS));

    if (s_view == VIEW_KB) {
        char ip[20];
        if (bsp_wifi_state() == BSP_WIFI_CONNECTED &&
            bsp_wifi_ip(ip, sizeof(ip)) == ESP_OK &&
            ip[0] && strcmp(ip, "0.0.0.0") != 0) {
            if (s_hint) lv_label_set_text_fmt(s_hint, app_str(APP_STR_WEB_IP), ip);
        } else {
            if (s_hint) lv_label_set_text(s_hint, app_str(APP_STR_ANC_KB));
        }
        app_kb_render(s_paint, sizeof(s_paint), app_str(APP_STR_KEYWORD),
                      s_custom, s_kb_sel, s_kb_set);
        lv_label_set_text(s_body, s_paint);
        app_web_set_target("keyword", s_custom, sizeof(s_custom), paint);
        return;
    }
    if (s_view == VIEW_PRESET) {
        if (s_hint) lv_label_set_text(s_hint, app_str(APP_STR_ANC_ADD));
        size_t o = 0;
        s_paint[0] = 0;
        for (int i = 0; i < PRESET_N; i++) {
            int w = snprintf(s_paint + o, sizeof(s_paint) - o, "%s %s%s\n",
                             i == s_preset_sel ? ">" : " ",
                             PRESETS[i],
                             kw_has(PRESETS[i]) ? " +" : "");
            if (w < 0) break;
            o += (size_t)w;
        }
        lv_label_set_text(s_body, s_paint);
        app_web_clear_target();
        return;
    }
    if (s_view == VIEW_SET) {
        paint_set();
        app_web_clear_target();
        return;
    }

    app_web_clear_target();
}

static void move_kb(int delta)
{
    app_ui_move(&s_kb_sel, KB_N, delta);
}

static void hold_tick(lv_timer_t *t)
{
    (void)t;
    if (s_view != VIEW_KB || s_hold_btn < 0) return;
    s_hold_ms += 80;
    if (s_hold_ms < 280) return;
    int dir = (s_hold_btn == BSP_BTN_UP) ? -1 : 1;
    int step = (s_hold_ms >= 800) ? KB_COLS : 1;
    move_kb(dir * step);
    paint();
}

void app_ancs_enter(lv_obj_t *p)
{
    s_page = p;
    lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(p, lv_color_hex(UI_SKY), 0);
    s_view = VIEW_RECENT;
    s_sel = 0;
    s_preset_sel = 0;
    s_kb_sel = 0;
    s_kb_set = 0;
    s_custom[0] = 0;
    s_hold_btn = -1;
    s_title = s_hint = s_body = s_list = NULL;
    s_recent = s_form = s_rtitle = s_rhint = NULL;
    s_ftitle = s_fhint = s_fbody = s_set_box = s_set_lab = NULL;
    memset(s_vis, 0, sizeof(s_vis));
    s_hold_timer = lv_timer_create(hold_tick, 80, NULL);
    paint();
}

void app_ancs_exit(void)
{
    s_hold_btn = -1;
    if (s_hold_timer) { lv_timer_delete(s_hold_timer); s_hold_timer = NULL; }
    app_web_clear_target();
    app_web_qr_close();
    s_page = s_title = s_hint = s_body = s_list = NULL;
    s_recent = s_form = s_rtitle = s_rhint = NULL;
    s_ftitle = s_fhint = s_fbody = s_set_box = s_set_lab = NULL;
    memset(s_vis, 0, sizeof(s_vis));
}

static void cycle_kw(int k)
{
    app_prefs_t *p = app_prefs();
    if (k < 0 || k >= p->kw_n) return;
    if (p->kw[k].prio == APP_PRIO_NORMAL) {
        p->kw[k].prio = APP_PRIO_HIGH;
    } else {
        for (int i = k; i < p->kw_n - 1; i++) p->kw[i] = p->kw[i + 1];
        p->kw_n--;
        memset(&p->kw[p->kw_n], 0, sizeof(p->kw[0]));
    }
    app_prefs_save();
}

void app_ancs_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    if (s_view == VIEW_RECENT && btn == BSP_BTN_UP && ev == BSP_BTN_LONG) {
        if (s_sel < recent_n() && app_notif_log_remove(s_sel)) {
            int rec = recent_n();
            if (s_sel >= rec) s_sel = rec > 0 ? rec - 1 : 0;
            paint();
        }
        return;
    }
    if (s_view == VIEW_KB && (btn == BSP_BTN_UP || btn == BSP_BTN_DOWN)) {
        if (ev == BSP_BTN_PRESS) {
            s_hold_btn = (int)btn;
            s_hold_ms = 0;
            move_kb(btn == BSP_BTN_UP ? -1 : 1);
            paint();
        } else if (ev == BSP_BTN_RELEASE && s_hold_btn == (int)btn) {
            s_hold_btn = -1;
            s_hold_ms = 0;
        }
        return;
    }
    if (ev != BSP_BTN_CLICK) return;

    if (s_view == VIEW_PRESET) {
        if (btn == BSP_BTN_UP) { app_ui_move(&s_preset_sel, PRESET_N, -1); paint(); return; }
        if (btn == BSP_BTN_DOWN) { app_ui_move(&s_preset_sel, PRESET_N, 1); paint(); return; }
        if (btn == BSP_BTN_OK) {
            add_kw(PRESETS[s_preset_sel], APP_PRIO_NORMAL);
            s_view = VIEW_SET;
            paint();
        }
        return;
    }
    if (s_view == VIEW_KB) {
        if (btn != BSP_BTN_OK) return;
        int r = app_kb_click(s_custom, sizeof(s_custom), &s_kb_sel, &s_kb_set);
        if (r == 2) {
            add_kw(s_custom, APP_PRIO_NORMAL);
            s_custom[0] = 0;
            s_view = VIEW_SET;
            app_web_qr_close();
        } else if (r == 3) {
            s_view = VIEW_SET;
            app_web_qr_close();
        } else if (r == 4) {
            app_web_qr_open();
        }
        paint();
        return;
    }

    if (btn == BSP_BTN_UP) { app_ui_move(&s_sel, list_n(), -1); paint(); return; }
    if (btn == BSP_BTN_DOWN) { app_ui_move(&s_sel, list_n(), 1); paint(); return; }
    if (btn != BSP_BTN_OK) return;

    if (s_view == VIEW_RECENT) {
        int rec = recent_n();
        if (s_sel >= rec) {
            s_view = VIEW_SET;
            s_sel = 0;
            paint();
        }
        return;
    }

    app_prefs_t *p = app_prefs();
    if (s_sel == 0) {
        s_view = VIEW_RECENT;
        s_sel = recent_n();
    } else if (s_sel == 1) {
        s_view = VIEW_PRESET;
        s_preset_sel = 0;
    } else if (s_sel == 2) {
        s_view = VIEW_KB;
        s_kb_sel = 0;
        s_kb_set = 0;
        s_custom[0] = 0;
        paint();
        return;
    } else if (s_sel < 3 + p->kw_n) {
        cycle_kw(s_sel - 3);
    } else {
        int i = (hide_idx(p->auto_hide) + 1) % 4;
        p->auto_hide = HIDE_OPTS[i];
        app_prefs_save();
    }
    paint();
}
