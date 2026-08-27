#include "app.h"

#include "app_i18n.h"
#include "app_logic.h"
#include "app_prefs.h"
#include "app_ui.h"
#include "app_web.h"
#include "bsp_wifi.h"
#include "ui_pixel.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

typedef enum { VIEW_LIST = 0, VIEW_FORM, VIEW_KB, VIEW_CONFIRM } view_t;

#define FORM_N 4

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
#define COLLAPSED_H  (BOX_CHROME + TITLE_H + ROW_GAP + CODE_H)
#define EXPANDED_H   (BOX_CHROME + TITLE_H + ROW_GAP + META_H + ROW_GAP + CODE_H + ROW_GAP + BODY_H)
#define ADD_H        50
#define VIS_MAX      4

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
static lv_obj_t *s_add_box, *s_add_lab;
static vis_t s_vis[VIS_MAX];
static view_t s_view;
static int s_sel, s_fsel, s_csel, s_acct;
static int s_edit; // -1 = add
static int s_kb_sel, s_kb_set, s_kb_field;
static int s_err;  // 0 none, 1 bad, 2 save failed
static app_totp_acct_t s_draft;
static char s_kb[APP_WEB_TEXT_MAX + 1];
static int s_hold_btn = -1;
static int s_hold_ms;
static lv_timer_t *s_hold_timer, *s_tick;
/* LVGL 任务栈只有 5KB,键盘页共用这块静态缓冲。 */
static char s_paint[900];

static int acct_n(void)
{
    return (int)app_totp_store()->n;
}

static int list_n(void)
{
    return acct_n() + 1;
}

static bool clock_ok(void)
{
    return time(NULL) >= (time_t)1700000000;
}

static uint64_t now_sec(void)
{
    time_t t = time(NULL);
    return t > 0 ? (uint64_t)t : 0;
}

static const char *issuer_text(const app_totp_acct_t *a)
{
    const char *s = app_totp_issuer(a);
    return s[0] ? s : app_str(APP_STR_TOTP_OTHER);
}

static void fill_code(const app_totp_acct_t *a, char *pretty, size_t n, int *remain)
{
    char raw[12];
    if (!app_totp_code(a, now_sec(), raw, sizeof(raw), remain)) {
        snprintf(pretty, n, "------");
        if (remain) *remain = 0;
        return;
    }
    app_totp_format_code(raw, pretty, n);
}

static void acct_title(const app_totp_acct_t *a, char *out, size_t n)
{
    const char *lab = app_totp_label(a);
    if (lab[0]) snprintf(out, n, "%s  %s", issuer_text(a), lab);
    else snprintf(out, n, "%s", issuer_text(a));
}

static void paint(void);

static void move_kb(int delta)
{
    app_ui_move(&s_kb_sel, KB_N, delta);
}

static void hold_tick(lv_timer_t *t)
{
    (void)t;
    if (s_hold_btn < 0) return;
    s_hold_ms += 80;
    if (s_hold_ms < 280) return;
    if (s_view == VIEW_KB) {
        int dir = (s_hold_btn == BSP_BTN_UP) ? -1 : 1;
        int step = (s_hold_ms >= 800) ? KB_COLS : 1;
        move_kb(dir * step);
        paint();
    }
}

static void tick(lv_timer_t *t)
{
    (void)t;
    if (s_view == VIEW_LIST) paint();
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
    s_ftitle = app_ui_title(s_form, app_str(APP_STR_HOME_CODES));
    s_fhint = app_ui_hint(s_form);
    s_fbody = app_ui_body(s_form, 44);
}

static void ensure_list_chrome(void)
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

static int row_h(int i, int n)
{
    if (i >= n) return ADD_H;
    return (i == s_sel) ? EXPANDED_H : COLLAPSED_H;
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

static void fill_vis(int slot, int y, int h, const app_totp_acct_t *a, bool sel)
{
    vis_t *v = vis_get(slot);
    if (!v || !v->box || !a) return;

    uint32_t bg = sel ? UI_YELLOW : UI_PAPER;
    uint32_t border = sel ? 0xFFFFFF : UI_INK;

    lv_obj_remove_flag(v->box, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(v->box, CARD_X, y);
    lv_obj_set_size(v->box, CARD_W, h);
    lv_obj_set_style_bg_color(v->box, lv_color_hex(bg), 0);
    lv_obj_set_style_border_color(v->box, lv_color_hex(border), 0);

    const char *lab = app_totp_label(a);
    const char *iss = issuer_text(a);
    const char *title = lab[0] ? lab : iss;
    const char *meta = lab[0] ? iss : "";

    char code[16];
    int remain = 0;
    fill_code(a, code, sizeof(code), &remain);

    int cy = 0;
    if (v->title) {
        lv_label_set_long_mode(v->title, LV_LABEL_LONG_CLIP);
        lv_label_set_text(v->title, title);
        lv_obj_set_style_text_color(v->title, lv_color_hex(UI_INK), 0);
        lv_obj_set_pos(v->title, 0, cy);
        lv_obj_set_height(v->title, TITLE_H);
    }
    cy += TITLE_H + ROW_GAP;

    if (v->meta) {
        if (sel && meta[0]) {
            lv_label_set_text(v->meta, meta);
            lv_obj_set_style_text_color(v->meta, lv_color_hex(UI_SKY_DARK), 0);
            lv_obj_set_pos(v->meta, 0, cy);
            lv_obj_set_height(v->meta, META_H);
            lv_obj_remove_flag(v->meta, LV_OBJ_FLAG_HIDDEN);
            cy += META_H + ROW_GAP;
        } else {
            lv_obj_add_flag(v->meta, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (v->code) {
        lv_label_set_text(v->code, code);
        lv_obj_set_style_bg_color(v->code, lv_color_hex(sel ? 0xFFFFFF : UI_YELLOW), 0);
        lv_obj_set_pos(v->code, 0, cy);
        lv_obj_remove_flag(v->code, LV_OBJ_FLAG_HIDDEN);
        cy += CODE_H + ROW_GAP;
    }

    if (v->body) {
        if (sel) {
            char body[80];
            int n = snprintf(body, sizeof(body), app_str(APP_STR_TOTP_LEFT), remain);
            if (n < 0) n = 0;
            snprintf(body + n, sizeof(body) - (size_t)n, "\n");
            n = (int)strlen(body);
            snprintf(body + n, sizeof(body) - (size_t)n, app_str(APP_STR_TOTP_META),
                     (int)(a->digits ? a->digits : 6),
                     (int)(a->period ? a->period : 30));
            int body_h = h - BOX_CHROME - cy;
            if (body_h < META_H) body_h = META_H;
            lv_label_set_text(v->body, body);
            lv_obj_set_style_text_color(v->body, lv_color_hex(UI_INK), 0);
            lv_obj_set_pos(v->body, 0, cy);
            lv_obj_set_height(v->body, body_h);
            lv_obj_remove_flag(v->body, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(v->body, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void fill_add(int y, bool sel)
{
    if (!s_add_box) {
        s_add_box = make_box(s_list, y, ADD_H,
                             sel ? UI_YELLOW : UI_PAPER, sel ? 0xFFFFFF : UI_INK);
        if (!s_add_box) return;
        s_add_lab = make_lab(s_add_box, ui_pixel_font_20(), UI_INK, 196, LV_LABEL_LONG_CLIP);
        if (s_add_lab) {
            lv_label_set_text(s_add_lab, app_str(APP_STR_TOTP_ADD));
            lv_obj_center(s_add_lab);
        }
    }
    lv_obj_remove_flag(s_add_box, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_y(s_add_box, y);
    lv_obj_set_style_bg_color(s_add_box, lv_color_hex(sel ? UI_YELLOW : UI_PAPER), 0);
    lv_obj_set_style_border_color(s_add_box,
        lv_color_hex(sel ? 0xFFFFFF : UI_INK), 0);
    if (s_add_lab) lv_label_set_text(s_add_lab, app_str(APP_STR_TOTP_ADD));
}

static void paint_list(void)
{
    ensure_list_chrome();
    show_only(s_recent);
    s_title = s_rtitle;
    s_hint = s_rhint;
    s_body = NULL;
    if (s_title) lv_label_set_text(s_title, app_str(APP_STR_HOME_CODES));

    int n = acct_n();
    int rows = list_n();
    if (s_sel >= rows) s_sel = rows ? rows - 1 : 0;
    if (s_sel < 0) s_sel = 0;

    if (s_hint) {
        if (!clock_ok()) lv_label_set_text(s_hint, app_str(APP_STR_TOTP_TIME));
        else if (n == 0) lv_label_set_text(s_hint, app_str(APP_STR_TOTP_EMPTY));
        else if (s_sel < n) lv_label_set_text(s_hint, app_str(APP_STR_HOLD_DELETE));
        else lv_label_set_text(s_hint, app_str(APP_STR_HINT_OPEN));
    }

    int start = s_sel;
    int end = s_sel;
    int used = row_h(s_sel, n);
    while (end + 1 < rows) {
        int next = used + GAP + row_h(end + 1, n);
        if (next > LIST_H) break;
        end++;
        used = next;
    }
    while (start > 0) {
        int next = used + GAP + row_h(start - 1, n);
        if (next > LIST_H) break;
        start--;
        used = next;
    }

    app_totp_list_t *l = app_totp_store();
    int slot = 0;
    int y = 0;
    bool add_shown = false;
    for (int i = start; i <= end; i++) {
        int h = row_h(i, n);
        if (i < n) {
            fill_vis(slot++, y, h, &l->items[i], i == s_sel);
        } else {
            fill_add(y, i == s_sel);
            add_shown = true;
        }
        y += h + GAP;
    }
    vis_hide_from(slot);
    if (!add_shown && s_add_box) lv_obj_add_flag(s_add_box, LV_OBJ_FLAG_HIDDEN);
    if (s_page) lv_obj_invalidate(s_page);
}

static const char *secret_label(void)
{
    static char mask[16];
    if (s_draft.secret[0]) {
        app_totp_mask(s_draft.secret, mask, sizeof(mask));
        return mask;
    }
    if (s_edit >= 0) return app_str(APP_STR_TOTP_KEEP);
    return app_str(APP_STR_EMPTY);
}

static void paint_form(void)
{
    lv_label_set_text(s_title, s_edit < 0 ? app_str(APP_STR_TOTP_ADD)
                                          : app_str(APP_STR_TOTP_EDIT));
    if (s_err == 1) lv_label_set_text(s_hint, app_str(APP_STR_TOTP_BAD));
    else if (s_err == 2) lv_label_set_text(s_hint, app_str(APP_STR_TOTP_FULL));
    else lv_label_set_text(s_hint, app_str(APP_STR_OK_CHOOSE));

    snprintf(s_paint, sizeof(s_paint),
             "%s %s\n  %s\n%s %s\n  %s\n%s %s\n  %s\n%s %s\n",
             s_fsel == 0 ? ">" : " ", app_str(APP_STR_TOTP_APP),
             s_draft.issuer[0] ? s_draft.issuer : app_str(APP_STR_EMPTY),
             s_fsel == 1 ? ">" : " ", app_str(APP_STR_TOTP_NAME),
             s_draft.label[0] ? s_draft.label : app_str(APP_STR_EMPTY),
             s_fsel == 2 ? ">" : " ", app_str(APP_STR_TOTP_SECRET),
             secret_label(),
             s_fsel == 3 ? ">" : " ", app_str(APP_STR_SAVE));
    lv_label_set_text(s_body, s_paint);
}

static void paint_confirm(void)
{
    app_totp_list_t *l = app_totp_store();
    char name[64];
    name[0] = 0;
    if (s_acct >= 0 && s_acct < (int)l->n) acct_title(&l->items[s_acct], name, sizeof(name));
    if (!name[0]) snprintf(name, sizeof(name), "TOTP");
    lv_label_set_text(s_title, app_str(APP_STR_TOTP_DELETE));
    lv_label_set_text(s_hint, app_str(APP_STR_OK_CHOOSE));
    int n = snprintf(s_paint, sizeof(s_paint), app_str(APP_STR_TOTP_CONFIRM), name);
    if (n < 0) n = 0;
    snprintf(s_paint + n, sizeof(s_paint) - (size_t)n,
             "\n\n%s %s\n%s %s\n",
             s_csel == 0 ? ">" : " ", app_str(APP_STR_TOTP_CANCEL),
             s_csel == 1 ? ">" : " ", app_str(APP_STR_TOTP_DELETE));
    lv_label_set_text(s_body, s_paint);
}

static const char *kb_field_name(int field)
{
    if (field == 0) return "app";
    if (field == 1) return "account";
    return "secret";
}

static const char *kb_field_str(int field)
{
    if (field == 0) return app_str(APP_STR_TOTP_APP);
    if (field == 1) return app_str(APP_STR_TOTP_NAME);
    return app_str(APP_STR_TOTP_SECRET);
}

static void paint_kb(void)
{
    lv_label_set_text(s_title, kb_field_str(s_kb_field));
    char ip[20];
    if (bsp_wifi_state() == BSP_WIFI_CONNECTED &&
        bsp_wifi_ip(ip, sizeof(ip)) == ESP_OK &&
        ip[0] && strcmp(ip, "0.0.0.0") != 0) {
        lv_label_set_text_fmt(s_hint, app_str(APP_STR_WEB_IP), ip);
    } else if (s_err == 1) {
        lv_label_set_text(s_hint, app_str(APP_STR_TOTP_BAD));
    } else if (s_kb_field == 2) {
        lv_label_set_text(s_hint, app_str(APP_STR_TOTP_HINT_SEC));
    } else {
        lv_label_set_text(s_hint, app_str(APP_STR_TOTP_HINT_FORM));
    }
    const char *head = s_kb_field == 2 ? app_str(APP_STR_TOTP_HINT_SEC)
                                       : kb_field_str(s_kb_field);
    app_kb_render(s_paint, sizeof(s_paint), head, s_kb, s_kb_sel, s_kb_set);
    lv_label_set_text(s_body, s_paint);
    app_web_set_target(kb_field_name(s_kb_field), s_kb, sizeof(s_kb), paint);
}

static void paint(void)
{
    if (!s_page) return;
    if (s_view != VIEW_KB) app_web_clear_target();

    if (s_view == VIEW_LIST) {
        paint_list();
        return;
    }

    ensure_form_chrome();
    show_only(s_form);
    s_title = s_ftitle;
    s_hint = s_fhint;
    s_body = s_fbody;
    if (!s_title || !s_hint || !s_body) return;

    switch (s_view) {
    case VIEW_FORM: paint_form(); break;
    case VIEW_KB: paint_kb(); break;
    case VIEW_CONFIRM: paint_confirm(); break;
    default: paint_list(); break;
    }
}

static void open_kb(int field)
{
    s_kb_field = field;
    s_kb_sel = 0;
    s_kb_set = field == 2 ? 1 : 0;
    s_err = 0;
    s_kb[0] = 0;
    if (field == 0 && s_draft.issuer[0]) {
        ui_pixel_utf8_copy(s_kb, sizeof(s_kb), s_draft.issuer);
    } else if (field == 1 && s_draft.label[0]) {
        ui_pixel_utf8_copy(s_kb, sizeof(s_kb), s_draft.label);
    }
    s_view = VIEW_KB;
    paint();
}

static bool looks_otpauth(const char *s)
{
    return s && s[0] && (s[0] == 'o' || s[0] == 'O');
}

static void apply_kb(void)
{
    bool fill = (s_kb_field != 2) ||
                (!s_draft.issuer[0] && !s_draft.label[0]);
    if (s_kb_field == 2 && !s_kb[0]) {
        s_view = VIEW_FORM;
        app_web_qr_close();
        s_err = 0;
        return;
    }
    if (s_kb[0] && (looks_otpauth(s_kb) || s_kb_field == 2)) {
        if (!app_totp_ingest(s_kb, &s_draft, fill)) {
            if (s_kb_field == 2 || looks_otpauth(s_kb)) {
                s_err = 1;
                return;
            }
        } else {
            s_view = VIEW_FORM;
            app_web_qr_close();
            s_err = 0;
            return;
        }
    }
    if (s_kb_field == 0) {
        ui_pixel_utf8_copy(s_draft.issuer, sizeof(s_draft.issuer), s_kb);
        s_view = VIEW_FORM;
        app_web_qr_close();
        s_err = 0;
        return;
    }
    if (s_kb_field == 1) {
        ui_pixel_utf8_copy(s_draft.label, sizeof(s_draft.label), s_kb);
        s_view = VIEW_FORM;
        app_web_qr_close();
        s_err = 0;
        return;
    }
    s_err = 1;
}

static void save_form(void)
{
    app_totp_list_t *l = app_totp_store();
    s_err = 0;
    if (!s_draft.issuer[0] && !s_draft.label[0]) {
        strncpy(s_draft.issuer, "TOTP", sizeof(s_draft.issuer) - 1);
    }
    if (!s_draft.secret[0]) {
        s_err = 1;
        return;
    }
    if (s_edit < 0) {
        if (!app_totp_list_add(l, &s_draft)) {
            s_err = 2;
            return;
        }
    } else if (!app_totp_list_update(l, s_edit, &s_draft)) {
        s_err = 1;
        return;
    }
    if (!app_totp_persist()) {
        s_err = 2;
        return;
    }
    s_view = VIEW_LIST;
    int found = app_totp_list_find(l, &s_draft);
    s_sel = found >= 0 ? found : (int)l->n;
}

static void open_add(void)
{
    memset(&s_draft, 0, sizeof(s_draft));
    s_edit = -1;
    s_fsel = 0;
    s_err = 0;
    s_view = VIEW_FORM;
}

static void open_edit(int i)
{
    app_totp_list_t *l = app_totp_store();
    if (i < 0 || i >= (int)l->n) return;
    s_draft = l->items[i];
    s_edit = i;
    s_acct = i;
    s_fsel = 0;
    s_err = 0;
    s_view = VIEW_FORM;
}

void app_totp_enter(lv_obj_t *p)
{
    s_page = p;
    lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(p, lv_color_hex(UI_SKY), 0);
    s_view = VIEW_LIST;
    s_sel = 0;
    s_fsel = 0;
    s_csel = 0;
    s_acct = 0;
    s_edit = -1;
    s_err = 0;
    s_hold_btn = -1;
    memset(&s_draft, 0, sizeof(s_draft));
    s_kb[0] = 0;
    s_title = s_hint = s_body = s_list = NULL;
    s_recent = s_form = s_rtitle = s_rhint = NULL;
    s_ftitle = s_fhint = s_fbody = s_add_box = s_add_lab = NULL;
    memset(s_vis, 0, sizeof(s_vis));
    s_hold_timer = lv_timer_create(hold_tick, 80, NULL);
    s_tick = lv_timer_create(tick, 500, NULL);
    paint();
}

void app_totp_exit(void)
{
    s_hold_btn = -1;
    if (s_hold_timer) { lv_timer_delete(s_hold_timer); s_hold_timer = NULL; }
    if (s_tick) { lv_timer_delete(s_tick); s_tick = NULL; }
    app_web_clear_target();
    app_web_qr_close();
    s_page = s_title = s_hint = s_body = s_list = NULL;
    s_recent = s_form = s_rtitle = s_rhint = NULL;
    s_ftitle = s_fhint = s_fbody = s_add_box = s_add_lab = NULL;
    memset(s_vis, 0, sizeof(s_vis));
}

void app_totp_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    if (s_view == VIEW_LIST && btn == BSP_BTN_UP && ev == BSP_BTN_LONG) {
        if (s_sel < acct_n()) {
            s_acct = s_sel;
            s_csel = 0;
            s_view = VIEW_CONFIRM;
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

    if (s_view == VIEW_KB) {
        if (btn != BSP_BTN_OK) return;
        int r = app_kb_click(s_kb, sizeof(s_kb), &s_kb_sel, &s_kb_set);
        if (r == 2) apply_kb();
        else if (r == 3) {
            s_view = VIEW_FORM;
            app_web_qr_close();
            s_err = 0;
        } else if (r == 4) {
            app_web_qr_open();
        }
        paint();
        return;
    }
    if (s_view == VIEW_CONFIRM) {
        if (btn == BSP_BTN_UP || btn == BSP_BTN_DOWN) {
            s_csel ^= 1;
            paint();
            return;
        }
        if (btn != BSP_BTN_OK) return;
        if (s_csel == 1) {
            app_totp_list_t *l = app_totp_store();
            app_totp_list_delete(l, s_acct);
            app_totp_persist();
            s_view = VIEW_LIST;
            if (s_sel >= (int)l->n) s_sel = (int)l->n;
        } else {
            s_view = VIEW_LIST;
        }
        paint();
        return;
    }
    if (s_view == VIEW_FORM) {
        if (btn == BSP_BTN_UP) { app_ui_move(&s_fsel, FORM_N, -1); paint(); return; }
        if (btn == BSP_BTN_DOWN) { app_ui_move(&s_fsel, FORM_N, 1); paint(); return; }
        if (btn != BSP_BTN_OK) return;
        if (s_fsel < 3) open_kb(s_fsel);
        else {
            save_form();
            paint();
        }
        return;
    }

    if (btn == BSP_BTN_UP) { app_ui_move(&s_sel, list_n(), -1); paint(); return; }
    if (btn == BSP_BTN_DOWN) { app_ui_move(&s_sel, list_n(), 1); paint(); return; }
    if (btn != BSP_BTN_OK) return;

    if (s_sel >= acct_n()) open_add();
    else open_edit(s_sel);
    paint();
}
