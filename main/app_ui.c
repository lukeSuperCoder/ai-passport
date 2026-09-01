#include "app_ui.h"
#include "game/game_state.h"
#include "services/app_persistence.h"
#include "services/clock_service.h"
#include "services/telemetry.h"
#include "lvgl.h"

#include <stdio.h>

#define COLOR_NIGHT       0x172637
#define COLOR_SKY         0x78A58A
#define COLOR_HILL        0x426B55
#define COLOR_GRASS       0x78964B
#define COLOR_WOOD        0x704936
#define COLOR_WOOD_DARK   0x3B2D2A
#define COLOR_PAPER       0xF2E4C4
#define COLOR_GOLD        0xE7B85B
#define COLOR_RED         0xA94B3F
#define COLOR_INK         0x282522
#define COLOR_MUTED       0x8C806E

typedef enum {
    PAGE_STATION = 0,
    PAGE_SCHEDULE,
} app_page_t;

static game_state_t s_game;
static lv_obj_t *s_screen;
static lv_obj_t *s_previous_screen;
static lv_obj_t *s_schedule_rows[3];
static app_page_t s_page;
static int s_top_selection;
static int s_schedule_selection;

static lv_obj_t *rect(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    return obj;
}

static lv_obj_t *label(lv_obj_t *parent, const char *text, int x, int y,
                       const lv_font_t *font, uint32_t color)
{
    lv_obj_t *obj = lv_label_create(parent);
    lv_label_set_text(obj, text);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_style_text_font(obj, font, 0);
    lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
    return obj;
}

static lv_obj_t *new_screen(uint32_t color)
{
    s_previous_screen = s_screen;
    s_screen = lv_obj_create(NULL);
    lv_obj_remove_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(color), 0);
    lv_obj_set_style_border_width(s_screen, 0, 0);
    lv_obj_set_style_pad_all(s_screen, 0, 0);
    return s_screen;
}

static void activate_screen(void)
{
    lv_screen_load(s_screen);
    if (s_previous_screen) {
        lv_obj_delete(s_previous_screen);
        s_previous_screen = NULL;
    }
    telemetry_log_memory(s_page == PAGE_STATION ? "page_station" : "page_schedule");
}

static void draw_cat(lv_obj_t *parent, int x, int y)
{
    rect(parent, x + 3, y, 6, 7, COLOR_INK);
    rect(parent, x + 23, y, 6, 7, COLOR_INK);
    rect(parent, x, y + 5, 32, 24, COLOR_INK);
    rect(parent, x + 5, y + 10, 5, 5, COLOR_GOLD);
    rect(parent, x + 22, y + 10, 5, 5, COLOR_GOLD);
    rect(parent, x + 10, y + 18, 12, 7, COLOR_PAPER);
    rect(parent, x + 5, y + 27, 22, 18, COLOR_INK);
    rect(parent, x + 3, y + 28, 26, 4, COLOR_RED);
    rect(parent, x + 26, y + 35, 9, 4, COLOR_INK);
}

static void draw_inn(lv_obj_t *parent)
{
    rect(parent, 0, 44, 240, 134, COLOR_SKY);
    rect(parent, 0, 92, 240, 86, COLOR_HILL);
    rect(parent, 0, 151, 240, 27, COLOR_GRASS);
    rect(parent, 36, 76, 168, 9, COLOR_WOOD_DARK);
    rect(parent, 49, 63, 142, 16, COLOR_WOOD_DARK);
    rect(parent, 61, 55, 118, 10, COLOR_WOOD_DARK);
    rect(parent, 48, 84, 144, 70, COLOR_PAPER);
    rect(parent, 48, 84, 144, 6, COLOR_WOOD);
    rect(parent, 48, 117, 144, 5, COLOR_WOOD);
    rect(parent, 71, 90, 6, 64, COLOR_WOOD);
    rect(parent, 164, 90, 6, 64, COLOR_WOOD);
    rect(parent, 106, 111, 35, 43, COLOR_WOOD);
    rect(parent, 82, 97, 18, 15, COLOR_GOLD);
    rect(parent, 146, 97, 12, 15, COLOR_GOLD);
    rect(parent, 32, 148, 178, 7, COLOR_WOOD_DARK);
    draw_cat(parent, 63, 129);
}

static void draw_top_status(lv_obj_t *parent)
{
    label(parent, "08:42", 9, 7, &lv_font_montserrat_20, COLOR_PAPER);
    label(parent, "SPR 6  /  RAIN", 9, 29, &lv_font_montserrat_14, COLOR_GOLD);
    label(parent, "82%", 197, 11, &lv_font_montserrat_14, COLOR_PAPER);
}

static void draw_bottom_tabs(lv_obj_t *parent)
{
    const char *items[2] = { "STATION", "SCHEDULE" };
    for (int i = 0; i < 2; i++) {
        int x = 8 + i * 113;
        lv_obj_t *tab = rect(parent, x, 278, 105, 32,
                             s_top_selection == i ? COLOR_GOLD : COLOR_PAPER);
        lv_obj_set_style_border_width(tab, 3, 0);
        lv_obj_set_style_border_color(tab, lv_color_hex(COLOR_INK), 0);
        lv_obj_t *text = label(tab, items[i], 0, 6, &lv_font_montserrat_14, COLOR_INK);
        lv_obj_set_width(text, 99);
        lv_obj_set_style_text_align(text, LV_TEXT_ALIGN_CENTER, 0);
    }
}

static void build_station(void)
{
    lv_obj_t *screen = new_screen(COLOR_NIGHT);
    draw_inn(screen);
    draw_top_status(screen);
    lv_obj_t *note = rect(screen, 9, 187, 222, 77, COLOR_PAPER);
    lv_obj_set_style_border_width(note, 3, 0);
    lv_obj_set_style_border_color(note, lv_color_hex(COLOR_WOOD_DARK), 0);
    label(note, "THE LANTERN IS LIT", 10, 9, &lv_font_montserrat_14, COLOR_RED);
    label(note, "Momo is greeting travelers.", 10, 32,
          &lv_font_montserrat_14, COLOR_INK);
    if (s_game.pending.available) {
        label(note, "! OFFLINE REPORT READY", 10, 52,
              &lv_font_montserrat_14, COLOR_RED);
    }
    draw_bottom_tabs(screen);
    activate_screen();
}

static void refresh_schedule_selection(void)
{
    for (int i = 0; i < 3; i++) {
        lv_obj_set_style_bg_color(s_schedule_rows[i],
            lv_color_hex(i == s_schedule_selection ? COLOR_GOLD : COLOR_PAPER), 0);
    }
}

static void build_schedule(void)
{
    lv_obj_t *screen = new_screen(COLOR_PAPER);
    rect(screen, 0, 0, 240, 48, COLOR_NIGHT);
    label(screen, "SCHEDULE", 10, 10, &lv_font_montserrat_20, COLOR_PAPER);
    s_schedule_rows[0] = rect(screen, 9, 57, 222, 55, COLOR_PAPER);
    s_schedule_rows[1] = rect(screen, 9, 120, 222, 55, COLOR_PAPER);
    s_schedule_rows[2] = rect(screen, 9, 183, 222, 55, COLOR_PAPER);
    for (int i = 0; i < 3; i++) {
        lv_obj_set_style_border_width(s_schedule_rows[i], 3, 0);
        lv_obj_set_style_border_color(s_schedule_rows[i], lv_color_hex(COLOR_INK), 0);
    }

    char report_line[40];
    if (s_game.pending.available) {
        snprintf(report_line, sizeof(report_line), "+%luG  +%uW  +%uB",
                 (unsigned long)s_game.pending.coins,
                 s_game.pending.wood, s_game.pending.berries);
    } else {
        snprintf(report_line, sizeof(report_line), "NO REWARD PENDING");
    }
    label(s_schedule_rows[0], "OFFLINE REPORT", 9, 9,
          &lv_font_montserrat_14, COLOR_RED);
    label(s_schedule_rows[0], report_line, 9, 29,
          &lv_font_montserrat_14, COLOR_INK);
    label(s_schedule_rows[1], "RECEPTION", 9, 9,
          &lv_font_montserrat_14, COLOR_RED);
    label(s_schedule_rows[1], "MOMO", 9, 28,
          &lv_font_montserrat_14, COLOR_INK);
    char stamina[24];
    snprintf(stamina, sizeof(stamina), "ENERGY %u", s_game.momo.stamina);
    label(s_schedule_rows[1], stamina, 128, 28,
          &lv_font_montserrat_14, COLOR_MUTED);

    label(s_schedule_rows[2], "FOREST  /  30 MIN", 9, 7,
          &lv_font_montserrat_14, COLOR_RED);
    label(s_schedule_rows[2], s_game.forest.active ? "AMAI EXPLORING" : "OK TO SEND AMAI",
          9, 29, &lv_font_montserrat_14, COLOR_INK);
    label(screen, "UP/DOWN SELECT   OK ACTION", 10, 249,
          &lv_font_montserrat_14, COLOR_MUTED);
    label(screen, "HOLD OK: STATION", 10, 279,
          &lv_font_montserrat_14, COLOR_INK);
    refresh_schedule_selection();
    activate_screen();
}

void app_ui_start(const bool ok[APP_UI_DEMO_COUNT])
{
    (void)ok;
    uint32_t now = 22600U;
    clock_service_now(&now);
    if (!app_persistence_load(&s_game)) {
        uint32_t started_at = now >= 6U * 3600U ? now - 6U * 3600U : 0U;
        game_state_init(&s_game, started_at);
        game_reduce(&s_game, (game_action_t){
            .type = GAME_ACTION_ASSIGN_MOMO_RECEPTION,
            .now = started_at,
        });
        game_reduce(&s_game, (game_action_t){
            .type = GAME_ACTION_SETTLE_TO_TIME,
            .now = now,
        });
        app_persistence_store(&s_game);
    } else {
        game_state_t candidate = s_game;
        if (game_reduce(&candidate, (game_action_t){
                .type = GAME_ACTION_SETTLE_TO_TIME,
                .now = now,
            }) && app_persistence_store(&candidate)) {
            s_game = candidate;
        }
    }
    s_page = PAGE_STATION;
    s_top_selection = 0;
    s_schedule_selection = 0;
    build_station();
}

void app_ui_handle_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    if (ev != BSP_BTN_CLICK && ev != BSP_BTN_LONG) return;

    if (s_page == PAGE_SCHEDULE) {
        if (btn == BSP_BTN_OK && ev == BSP_BTN_LONG) {
            s_page = PAGE_STATION;
            s_top_selection = 0;
            build_station();
        } else if (ev == BSP_BTN_CLICK &&
                   (btn == BSP_BTN_UP || btn == BSP_BTN_DOWN)) {
            s_schedule_selection = (s_schedule_selection + 1) % 3;
            refresh_schedule_selection();
        } else if (ev == BSP_BTN_CLICK && btn == BSP_BTN_OK &&
                   s_schedule_selection == 0 && s_game.pending.available) {
            game_state_t candidate = s_game;
            game_reduce(&candidate, (game_action_t){
                .type = GAME_ACTION_CLAIM_REPORT,
            });
            if (app_persistence_store(&candidate)) {
                s_game = candidate;
            }
            build_schedule();
        } else if (ev == BSP_BTN_CLICK && btn == BSP_BTN_OK &&
                   s_schedule_selection == 2 && !s_game.forest.active) {
            uint32_t now = s_game.last_trusted_time;
            clock_service_now(&now);
            game_state_t candidate = s_game;
            if (game_reduce(&candidate, (game_action_t){
                    .type = GAME_ACTION_START_AMAI_FOREST,
                    .now = now,
                }) && app_persistence_store(&candidate)) {
                s_game = candidate;
            }
            build_schedule();
        }
        return;
    }

    if (ev != BSP_BTN_CLICK) return;
    if (btn == BSP_BTN_UP || btn == BSP_BTN_DOWN) {
        s_top_selection = (s_top_selection + 1) % 2;
        build_station();
    } else if (btn == BSP_BTN_OK && s_top_selection == 1) {
        s_page = PAGE_SCHEDULE;
        s_schedule_selection = 0;
        build_schedule();
    }
}
