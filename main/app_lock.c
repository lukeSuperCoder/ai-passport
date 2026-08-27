#include "app.h"

#include "app_i18n.h"
#include "app_time.h"
#include "bsp_battery.h"
#include "ui_pixel.h"

#include <stdio.h>
#include <string.h>

static lv_obj_t *s_box, *s_date, *s_clock, *s_icon, *s_wx, *s_batt, *s_hint;
static bool s_shown;
static int s_icon_wmo = -2;
static char s_clk[16];

static void clock_draw(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_DRAW_MAIN) return;
    lv_obj_t *obj = lv_event_get_target(e);
    lv_layer_t *layer = lv_event_get_layer(e);
    lv_area_t box;
    lv_obj_get_coords(obj, &box);
    ui_pixel_draw_clock4x(layer, s_clk, &box, 0xFFFFFF);
}

static void paint(void)
{
    if (!s_shown) return;
    char date[48];
    char clk[16];
    char wx[80];
    char batt[12];

    app_time_lock_date(date, sizeof(date));
    app_time_lock_clock(clk, sizeof(clk));
    lv_label_set_text(s_date, date);
    if (strcmp(s_clk, clk) != 0) {
        strncpy(s_clk, clk, sizeof(s_clk) - 1);
        s_clk[sizeof(s_clk) - 1] = 0;
        lv_obj_invalidate(s_clock);
    }
    lv_label_set_text(s_hint, app_str(APP_STR_LOCK_HINT));

    int wmo = app_wx_wmo();
    if (s_icon && wmo != s_icon_wmo) {
        s_icon_wmo = wmo;
        if (wmo < 0) {
            lv_obj_clean(s_icon);
            lv_obj_add_flag(s_icon, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_remove_flag(s_icon, LV_OBJ_FLAG_HIDDEN);
            app_wx_draw_icon(s_icon, wmo);
        }
    }

    if (app_wx_lock_line(wx, sizeof(wx))) {
        lv_label_set_text(s_wx, wx);
        lv_obj_remove_flag(s_wx, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_wx, LV_OBJ_FLAG_HIDDEN);
    }

    int soc = bsp_battery_soc();
    if (soc < 0) lv_label_set_text(s_batt, "");
    else {
        snprintf(batt, sizeof(batt), "%d%%", soc);
        lv_label_set_text(s_batt, batt);
    }
}

void app_lock_init(lv_obj_t *screen)
{
    s_box = lv_obj_create(screen);
    ui_pixel_strip_theme(s_box);
    lv_obj_set_pos(s_box, 0, 0);
    lv_obj_set_size(s_box, 240, 320);
    lv_obj_set_style_bg_opa(s_box, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(s_box, lv_color_hex(UI_SKY), 0);

    s_batt = lv_label_create(s_box);
    lv_obj_set_style_text_font(s_batt, ui_pixel_font_14(), 0);
    lv_obj_set_style_text_color(s_batt, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(s_batt, LV_ALIGN_TOP_RIGHT, -12, 12);

    s_date = lv_label_create(s_box);
    lv_obj_set_style_text_font(s_date, ui_pixel_font_14(), 0);
    lv_obj_set_style_text_color(s_date, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_align(s_date, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(s_date, 216);
    lv_label_set_long_mode(s_date, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_date, LV_ALIGN_TOP_MID, 0, 56);

    s_clock = lv_obj_create(s_box);
    ui_pixel_strip_theme(s_clock);
    lv_obj_set_size(s_clock, 240, 88);
    lv_obj_set_style_bg_opa(s_clock, LV_OPA_TRANSP, 0);
    lv_obj_remove_flag(s_clock, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(s_clock, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_align(s_clock, LV_ALIGN_TOP_MID, 0, 96);
    lv_obj_add_event_cb(s_clock, clock_draw, LV_EVENT_DRAW_MAIN, NULL);

    s_icon = lv_obj_create(s_box);
    ui_pixel_strip_theme(s_icon);
    lv_obj_set_size(s_icon, 40, 40);
    lv_obj_align(s_icon, LV_ALIGN_TOP_MID, 0, 192);
    lv_obj_set_style_bg_opa(s_icon, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(s_icon, LV_OBJ_FLAG_HIDDEN);

    s_wx = lv_label_create(s_box);
    lv_obj_set_style_text_font(s_wx, ui_pixel_font_14(), 0);
    lv_obj_set_style_text_color(s_wx, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_align(s_wx, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(s_wx, 216);
    lv_label_set_long_mode(s_wx, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_wx, LV_ALIGN_TOP_MID, 0, 240);

    s_hint = lv_label_create(s_box);
    lv_obj_set_style_text_font(s_hint, ui_pixel_font_14(), 0);
    lv_obj_set_style_text_color(s_hint, lv_color_hex(UI_YELLOW), 0);
    lv_obj_align(s_hint, LV_ALIGN_BOTTOM_MID, 0, -24);

    lv_obj_add_flag(s_box, LV_OBJ_FLAG_HIDDEN);
    s_shown = false;
    s_icon_wmo = -2;
}

void app_lock_show(void)
{
    if (!s_box) return;
    s_shown = true;
    paint();
    lv_obj_remove_flag(s_box, LV_OBJ_FLAG_HIDDEN);
}

void app_lock_hide(void)
{
    if (!s_box) return;
    s_shown = false;
    lv_obj_add_flag(s_box, LV_OBJ_FLAG_HIDDEN);
}

bool app_lock_visible(void)
{
    return s_shown;
}

void app_lock_tick(void)
{
    if (s_shown) paint();
}
