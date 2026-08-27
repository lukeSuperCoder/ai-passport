#include "app_ui.h"
#include "demo.h"
#include "ui_pixel.h"
#include "lvgl.h"

static const demo_entry_t DEMOS[] = {
    { "Display", demo_display_enter, demo_display_exit, demo_display_key },
    { "Button",  demo_button_enter,  demo_button_exit,  demo_button_key  },
    { "Audio",   demo_audio_enter,   demo_audio_exit,   demo_audio_key   },
    { "Battery", demo_battery_enter, demo_battery_exit, demo_battery_key },
};

static bool s_ok[APP_UI_DEMO_COUNT];
static lv_obj_t *s_menu_scr;
static lv_obj_t *s_cards[APP_UI_DEMO_COUNT];
static lv_obj_t *s_rows[APP_UI_DEMO_COUNT];
static lv_obj_t *s_mascot;
static int s_sel;
static int s_active = -1;

static void menu_refresh(void)
{
    for (size_t i = 0; i < APP_UI_DEMO_COUNT; i++) {
        lv_label_set_text_fmt(s_rows[i], "%s%s", DEMOS[i].name,
                              s_ok[i] ? "" : "  [FAIL]");
        ui_pixel_set_selected(s_cards[i], (int)i == s_sel, s_ok[i]);
        lv_obj_set_style_text_color(s_rows[i],
            s_ok[i] ? lv_color_hex(UI_INK) : lv_color_hex(0x7A2020), 0);
    }
}

static void menu_build(void)
{
    s_menu_scr = ui_pixel_screen_create("FoloToy");
    for (size_t i = 0; i < APP_UI_DEMO_COUNT; i++) {
        int x = 11 + (int)(i % 2) * 112;
        int y = 58 + (int)(i / 2) * 86;
        s_cards[i] = ui_pixel_panel_create(s_menu_scr, x, y, 102, 72, UI_PAPER);
        s_rows[i] = lv_label_create(s_cards[i]);
        lv_obj_set_style_text_font(s_rows[i], &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_align(s_rows[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(s_rows[i]);
    }
    s_mascot = ui_pixel_mascot_create(s_menu_scr, 101, 238);
    menu_refresh();
    lv_screen_load(s_menu_scr);
}

static void enter_menu(void)
{
    s_active = -1;
    menu_build();
}

void app_ui_start(const bool ok[APP_UI_DEMO_COUNT])
{
    for (size_t i = 0; i < APP_UI_DEMO_COUNT; i++) s_ok[i] = ok[i];
    s_sel = 0;
    s_active = -1;
    enter_menu();
}

void app_ui_handle_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    if (s_active >= 0) {
        if (btn == BSP_BTN_OK && ev == BSP_BTN_LONG) {
            DEMOS[s_active].exit();
            enter_menu();
        } else {
            DEMOS[s_active].key(btn, ev);
        }
        return;
    }

    if (ev != BSP_BTN_CLICK) return;
    if (btn == BSP_BTN_UP) {
        s_sel = (s_sel + APP_UI_DEMO_COUNT - 1) % APP_UI_DEMO_COUNT;
        menu_refresh();
        ui_pixel_mascot_jump(s_mascot);
    } else if (btn == BSP_BTN_DOWN) {
        s_sel = (s_sel + 1) % APP_UI_DEMO_COUNT;
        menu_refresh();
        ui_pixel_mascot_jump(s_mascot);
    } else if (btn == BSP_BTN_OK && s_ok[s_sel]) {
        s_active = s_sel;
        ui_pixel_mascot_jump(s_mascot);
        lv_obj_delete(s_menu_scr);
        s_menu_scr = NULL;
        s_mascot = NULL;
        DEMOS[s_active].enter();
    }
}
