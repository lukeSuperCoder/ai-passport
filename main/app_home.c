#include "app.h"
#include "app_i18n.h"
#include "ui_pixel.h"

#define HOME_N 5

static lv_obj_t *s_cards[HOME_N];
static int s_sel;

static void paint(void)
{
    for (int i = 0; i < HOME_N; i++) {
        ui_pixel_set_selected(s_cards[i], s_sel == i, true);
    }
}

void app_home_enter(lv_obj_t *p)
{
    s_sel = 0;
    const char *names[] = {
        app_str(APP_STR_HOME_ALERTS),
        app_str(APP_STR_HOME_WALKIE),
        app_str(APP_STR_HOME_WEATHER),
        app_str(APP_STR_HOME_CODES),
        app_str(APP_STR_HOME_SETTINGS),
    };
    for (int i = 0; i < HOME_N; i++) {
        s_cards[i] = ui_pixel_panel_create(p, 18, 6 + i * 56, 204, 50, UI_PAPER);
        lv_obj_t *lab = lv_label_create(s_cards[i]);
        lv_obj_set_style_text_font(lab, ui_pixel_font_20(), 0);
        lv_obj_set_style_text_color(lab, lv_color_hex(UI_INK), 0);
        lv_label_set_text(lab, names[i]);
        lv_obj_center(lab);
    }
    paint();
}

void app_home_exit(void)
{
    for (int i = 0; i < HOME_N; i++) s_cards[i] = NULL;
}

void app_home_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    if (ev != BSP_BTN_CLICK) return;
    if (btn == BSP_BTN_UP) {
        s_sel = (s_sel + HOME_N - 1) % HOME_N;
        paint();
        return;
    }
    if (btn == BSP_BTN_DOWN) {
        s_sel = (s_sel + 1) % HOME_N;
        paint();
        return;
    }
    if (btn != BSP_BTN_OK) return;
    if (s_sel == 0) app_shell_open(app_ancs_enter, app_ancs_exit, app_ancs_key);
    else if (s_sel == 1) app_shell_open(app_walkie_enter, app_walkie_exit, app_walkie_key);
    else if (s_sel == 2) app_shell_open(app_wx_enter, app_wx_exit, app_wx_key);
    else if (s_sel == 3) app_shell_open(app_totp_enter, app_totp_exit, app_totp_key);
    else app_shell_open(app_settings_enter, app_settings_exit, app_settings_key);
}
