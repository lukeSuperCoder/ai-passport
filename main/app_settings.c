#include "app.h"
#include "app_i18n.h"
#include "app_prefs.h"
#include "app_ui.h"
#include "ui_pixel.h"

#include <stdio.h>

static lv_obj_t *s_title, *s_hint, *s_body, *s_credit;
static lv_timer_t *s_lang_timer;
static int s_sel;

#define N 8

static const char *item_label(int i)
{
    switch (i) {
    case 0: return app_str(APP_STR_LANGUAGE);
    case 1: return app_str(APP_STR_WIFI);
    case 2: return app_str(APP_STR_BLUETOOTH);
    case 3: return app_str(APP_STR_DATETIME);
    case 4: return app_str(APP_STR_SCREEN);
    case 5: return app_str(APP_STR_SOUND);
    case 6: return app_str(APP_STR_HARDWARE);
    default: return app_str(APP_STR_LOG);
    }
}

static void paint(void)
{
    if (s_title) lv_label_set_text(s_title, app_str(APP_STR_SETTINGS));
    if (s_hint) lv_label_set_text(s_hint, app_str(APP_STR_HINT_OPEN));
    if (!s_body) return;

    char buf[400];
    size_t o = 0;
    buf[0] = 0;
    int window = 7;
    if (s_sel >= N) s_sel = N - 1;
    int start = s_sel - window / 2;
    if (start < 0) start = 0;
    if (start + window > N) start = N > window ? N - window : 0;
    for (int i = start; i < N && i < start + window; i++) {
        int w;
        if (i == 0) {
            w = snprintf(buf + o, sizeof(buf) - o, "%s %s  %s\n",
                         i == s_sel ? ">" : " ", item_label(i),
                         app_lang_name(app_lang()));
        } else {
            w = snprintf(buf + o, sizeof(buf) - o, "%s %s\n",
                         i == s_sel ? ">" : " ", item_label(i));
        }
        if (w < 0) break;
        o += (size_t)w;
        if (o >= sizeof(buf)) break;
    }
    lv_label_set_text(s_body, buf);
}

static void lang_apply(lv_timer_t *t)
{
    (void)t;
    s_lang_timer = NULL;
    app_prefs_save_lang();
    paint();
}

void app_settings_enter(lv_obj_t *p)
{
    lv_obj_t *card = app_ui_card(p);
    s_title = app_ui_title(card, app_str(APP_STR_SETTINGS));
    s_hint = app_ui_hint(card);
    s_body = app_ui_body(card, 48);
    s_credit = lv_label_create(card);
    lv_obj_set_style_text_font(s_credit, ui_pixel_font_14(), 0);
    lv_obj_set_style_text_color(s_credit, lv_color_hex(UI_SKY_DARK), 0);
    lv_label_set_text(s_credit, "Powered By Pax-z");
    lv_obj_align(s_credit, LV_ALIGN_BOTTOM_MID, 0, 0);
    if (s_sel < 0 || s_sel >= N) s_sel = 0;
    paint();
}

void app_settings_exit(void)
{
    if (s_lang_timer) {
        lv_timer_delete(s_lang_timer);
        s_lang_timer = NULL;
        app_prefs_save_lang();
    }
    s_title = s_hint = s_body = s_credit = NULL;
}

void app_settings_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    if (ev != BSP_BTN_CLICK) return;
    if (btn == BSP_BTN_UP) { app_ui_move(&s_sel, N, -1); paint(); return; }
    if (btn == BSP_BTN_DOWN) { app_ui_move(&s_sel, N, 1); paint(); return; }
    if (btn != BSP_BTN_OK) return;
    switch (s_sel) {
    case 0: {
        app_prefs_t *p = app_prefs();
        p->lang = (uint8_t)((p->lang + 1) % APP_LANG_N);
        app_lang_set((app_lang_t)p->lang);
        /* 按键回调跑在 button/esp_timer 小栈上。中文 glyph 排版和 NVS
         * 都放到 LVGL 任务里做,避免卡死。 */
        if (s_lang_timer) {
            lv_timer_reset(s_lang_timer);
        } else {
            s_lang_timer = lv_timer_create(lang_apply, 20, NULL);
            lv_timer_set_repeat_count(s_lang_timer, 1);
        }
        break;
    }
    case 1: app_shell_open(app_wifi_enter, app_wifi_exit, app_wifi_key); break;
    case 2: app_shell_open(app_ble_enter, app_ble_exit, app_ble_key); break;
    case 3: app_shell_open(app_clock_enter, app_clock_exit, app_clock_key); break;
    case 4: app_shell_open(app_screen_enter, app_screen_exit, app_screen_key); break;
    case 5: app_shell_open(app_sound_enter, app_sound_exit, app_sound_key); break;
    case 6: app_shell_open(app_hw_enter, app_hw_exit, app_hw_key); break;
    case 7: app_shell_open(app_logs_enter, app_logs_exit, app_logs_key); break;
    default: break;
    }
}
