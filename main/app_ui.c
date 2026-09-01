#include "app_ui.h"
#include "game/game_state.h"
#include "game/game_content.h"
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
    PAGE_FARM,
    PAGE_TRAVEL,
    PAGE_BACKPACK,
    PAGE_FARM_DETAIL,
    PAGE_KITCHEN,
    PAGE_BUILDINGS,
} app_page_t;

static game_state_t s_game;
static lv_obj_t *s_screen;
static lv_obj_t *s_previous_screen;
static lv_obj_t *s_schedule_rows[4];
static app_page_t s_page;
static int s_top_selection;
static int s_schedule_selection;
static int s_farm_selection;
static int s_farm_crop_selection;
static int s_recipe_selection;
static int s_backpack_selection;
static int s_building_selection;
static uint32_t s_now;

static uint16_t ui_crop_count(game_crop_t crop)
{
    return crop == GAME_CROP_WHEAT
        ? s_game.inventory_wheat : s_game.inventory_crops[crop];
}

static uint16_t ui_seed_count(game_crop_t crop)
{
    return crop == GAME_CROP_WHEAT
        ? s_game.inventory_wheat_seed : s_game.inventory_seeds[crop];
}

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
    const char *checkpoint = "page_station";
    switch (s_page) {
    case PAGE_STATION: checkpoint = "page_station"; break;
    case PAGE_SCHEDULE: checkpoint = "page_schedule"; break;
    case PAGE_FARM: checkpoint = "page_farm"; break;
    case PAGE_TRAVEL: checkpoint = "page_travel"; break;
    case PAGE_BACKPACK: checkpoint = "page_backpack"; break;
    case PAGE_FARM_DETAIL: checkpoint = "page_farm_detail"; break;
    case PAGE_KITCHEN: checkpoint = "page_kitchen"; break;
    case PAGE_BUILDINGS: checkpoint = "page_buildings"; break;
    }
    telemetry_log_memory(checkpoint);
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
    uint32_t hour = (s_now / 3600U) % 24U;
    uint32_t sky = (hour >= 21U || hour < 6U) ? COLOR_NIGHT :
        ((hour >= 17U) ? 0xB96F52 : ((hour < 11U) ? 0x94AF9B : COLOR_SKY));
    rect(parent, 0, 44, 240, 134, sky);
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
    if (s_game.weather == GAME_WEATHER_RAIN ||
        s_game.weather == GAME_WEATHER_STORM) {
        for (int i = 0; i < 12; i++) {
            rect(parent, 6 + i * 20, 50 + (i % 4) * 19, 2, 10, COLOR_PAPER);
        }
    }
}

static void draw_top_status(lv_obj_t *parent)
{
    char time_text[8];
    snprintf(time_text, sizeof(time_text), "%02lu:%02lu",
             (unsigned long)((s_now / 3600U) % 24U),
             (unsigned long)((s_now / 60U) % 60U));
    label(parent, time_text, 9, 7, &lv_font_montserrat_20, COLOR_PAPER);
    char calendar[32];
    snprintf(calendar, sizeof(calendar), "SPR %u / %s", s_game.spring_day,
             game_weather_name(s_game.weather));
    label(parent, calendar, 9, 29, &lv_font_montserrat_14, COLOR_GOLD);
    label(parent, "82%", 197, 11, &lv_font_montserrat_14, COLOR_PAPER);
}

static void draw_bottom_tabs(lv_obj_t *parent)
{
    const char *items[5] = { "INN", "PLAN", "FARM", "TRIP", "BAG" };
    for (int i = 0; i < 5; i++) {
        int x = 3 + i * 47;
        lv_obj_t *tab = rect(parent, x, 278, 46, 32,
                             s_top_selection == i ? COLOR_GOLD : COLOR_PAPER);
        lv_obj_set_style_border_width(tab, 3, 0);
        lv_obj_set_style_border_color(tab, lv_color_hex(COLOR_INK), 0);
        lv_obj_t *text = label(tab, items[i], 0, 7, &lv_font_montserrat_14, COLOR_INK);
        lv_obj_set_width(text, 42);
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
    if (s_game.pending_events != 0U) {
        label(note, "! STORY EVENT IN PLAN", 10, 52,
              &lv_font_montserrat_14, COLOR_RED);
    } else if (s_game.travel.active) {
        label(note, "TRAVEL TEAM IS AWAY", 10, 52,
              &lv_font_montserrat_14, COLOR_RED);
    }
    draw_bottom_tabs(screen);
    activate_screen();
}

static void refresh_schedule_selection(void)
{
    for (int i = 0; i < 4; i++) {
        lv_obj_set_style_bg_color(s_schedule_rows[i],
            lv_color_hex(i == s_schedule_selection ? COLOR_GOLD : COLOR_PAPER), 0);
    }
}

static void build_schedule(void)
{
    lv_obj_t *screen = new_screen(COLOR_PAPER);
    rect(screen, 0, 0, 240, 48, COLOR_NIGHT);
    label(screen, "SCHEDULE", 10, 10, &lv_font_montserrat_20, COLOR_PAPER);
    s_schedule_rows[0] = rect(screen, 9, 52, 222, 45, COLOR_PAPER);
    s_schedule_rows[1] = rect(screen, 9, 102, 222, 45, COLOR_PAPER);
    s_schedule_rows[2] = rect(screen, 9, 152, 222, 45, COLOR_PAPER);
    s_schedule_rows[3] = rect(screen, 9, 202, 222, 45, COLOR_PAPER);
    for (int i = 0; i < 4; i++) {
        lv_obj_set_style_border_width(s_schedule_rows[i], 3, 0);
        lv_obj_set_style_border_color(s_schedule_rows[i], lv_color_hex(COLOR_INK), 0);
    }

    char report_line[40];
    if (s_game.pending_events & GAME_EVENT_MARKET) {
        snprintf(report_line, sizeof(report_line), "ROAD MARKET - OK TO VISIT");
    } else if (s_game.pending_events & GAME_EVENT_FESTIVAL) {
        snprintf(report_line, sizeof(report_line), "LANTERN FEST - OK TO JOIN");
    } else if (s_game.pending.available) {
        snprintf(report_line, sizeof(report_line), "+%luG +%uW +%uB +%uH +%uF",
                 (unsigned long)s_game.pending.coins,
                 s_game.pending.wood, s_game.pending.berries,
                 s_game.pending.hot_bread, s_game.pending.wheat);
    } else {
        snprintf(report_line, sizeof(report_line), "NO REWARD PENDING");
    }
    label(s_schedule_rows[0], s_game.pending_events ? "STORY EVENT" : "OFFLINE REPORT", 9, 4,
          &lv_font_montserrat_14, COLOR_RED);
    label(s_schedule_rows[0], report_line, 9, 23,
          &lv_font_montserrat_14, COLOR_INK);
    label(s_schedule_rows[1], "RECEPTION", 9, 4,
          &lv_font_montserrat_14, COLOR_RED);
    label(s_schedule_rows[1], "MOMO", 9, 23,
          &lv_font_montserrat_14, COLOR_INK);
    char stamina[24];
    snprintf(stamina, sizeof(stamina), "ENERGY %u", s_game.momo.stamina);
    label(s_schedule_rows[1], stamina, 128, 23,
          &lv_font_montserrat_14, COLOR_MUTED);

    label(s_schedule_rows[2], "FOREST  /  30 MIN", 9, 4,
          &lv_font_montserrat_14, COLOR_RED);
    label(s_schedule_rows[2], s_game.forest.active ? "AMAI EXPLORING" : "OK TO SEND AMAI",
          9, 23, &lv_font_montserrat_14, COLOR_INK);
    label(s_schedule_rows[3], "KITCHEN / HOT BREAD", 9, 4,
          &lv_font_montserrat_14, COLOR_RED);
    char kitchen_line[40];
    snprintf(kitchen_line, sizeof(kitchen_line), "%s  WHEAT %u",
             s_game.kitchen.active ? "ATUAN COOKING" : "OK TO COOK",
             s_game.inventory_wheat);
    label(s_schedule_rows[3], kitchen_line, 9, 23,
          &lv_font_montserrat_14, COLOR_INK);
    label(screen, "UP/DOWN SELECT   OK ACTION", 10, 252,
          &lv_font_montserrat_14, COLOR_MUTED);
    label(screen, "HOLD OK: STATION", 10, 282,
          &lv_font_montserrat_14, COLOR_INK);
    refresh_schedule_selection();
    activate_screen();
}

static void build_farm(void)
{
    lv_obj_t *screen = new_screen(COLOR_GRASS);
    rect(screen, 0, 0, 240, 48, COLOR_NIGHT);
    label(screen, "FARM", 10, 10, &lv_font_montserrat_20, COLOR_PAPER);
    char stock[32];
    snprintf(stock, sizeof(stock), "SEEDS %u  WHEAT %u",
             s_game.inventory_wheat_seed, s_game.inventory_wheat);
    label(screen, stock, 78, 16, &lv_font_montserrat_14, COLOR_GOLD);

    for (int i = 0; i < (int)GAME_FARM_PLOT_COUNT; i++) {
        int y = 58 + i * 51;
        lv_obj_t *plot = rect(screen, 10, y, 220, 43,
                              i == s_farm_selection ? COLOR_GOLD : COLOR_PAPER);
        lv_obj_set_style_border_width(plot, 3, 0);
        lv_obj_set_style_border_color(plot, lv_color_hex(COLOR_WOOD_DARK), 0);
        char line[36];
        const game_crop_definition_t *crop = s_game.farm[i].active
            ? game_crop_definition(s_game.farm[i].crop) : NULL;
        snprintf(line, sizeof(line), "PLOT %d  %s", i + 1,
                 crop ? crop->name : "OK FOR DETAILS");
        label(plot, line, 8, 11, &lv_font_montserrat_14, COLOR_INK);
    }
    label(screen, "HOLD OK: STATION", 10, 276,
          &lv_font_montserrat_14, COLOR_INK);
    activate_screen();
}

static void build_farm_detail(void)
{
    lv_obj_t *screen = new_screen(COLOR_GRASS);
    rect(screen, 0, 0, 240, 48, COLOR_NIGHT);
    char title[24];
    snprintf(title, sizeof(title), "PLOT %d", s_farm_selection + 1);
    label(screen, title, 10, 10, &lv_font_montserrat_20, COLOR_PAPER);
    lv_obj_t *card = rect(screen, 10, 61, 220, 174, COLOR_PAPER);
    lv_obj_set_style_border_width(card, 3, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(COLOR_WOOD_DARK), 0);
    if (s_game.farm[s_farm_selection].active) {
        const game_crop_definition_t *definition =
            game_crop_definition(s_game.farm[s_farm_selection].crop);
        label(card, definition->name, 10, 12, &lv_font_montserrat_20, COLOR_RED);
        uint32_t remaining = s_game.farm[s_farm_selection].matures_at > s_now
            ? s_game.farm[s_farm_selection].matures_at - s_now : 0U;
        char line[36];
        snprintf(line, sizeof(line), "HEALTHY / %luh %02lum",
                 (unsigned long)(remaining / 3600U),
                 (unsigned long)((remaining / 60U) % 60U));
        label(card, line, 10, 55, &lv_font_montserrat_14, COLOR_INK);
        label(card, "CARETAKER: LULU", 10, 88, &lv_font_montserrat_14, COLOR_INK);
    } else {
        game_crop_t crop = (game_crop_t)s_farm_crop_selection;
        const game_crop_definition_t *definition = game_crop_definition(crop);
        label(card, definition->name, 10, 12, &lv_font_montserrat_20, COLOR_RED);
        char line[36];
        snprintf(line, sizeof(line), "SEEDS %u / GROW %luh",
                 ui_seed_count(crop),
                 (unsigned long)(definition->grow_seconds / 3600U));
        label(card, line, 10, 55, &lv_font_montserrat_14, COLOR_INK);
        label(card, "UP/DOWN: CHOOSE CROP", 10, 88,
              &lv_font_montserrat_14, COLOR_MUTED);
        label(card, "OK: PLANT", 10, 120, &lv_font_montserrat_14, COLOR_RED);
    }
    label(screen, "HOLD OK: BACK", 10, 276, &lv_font_montserrat_14, COLOR_INK);
    activate_screen();
}

static void build_kitchen(void)
{
    lv_obj_t *screen = new_screen(COLOR_PAPER);
    rect(screen, 0, 0, 240, 48, COLOR_NIGHT);
    label(screen, "KITCHEN", 10, 10, &lv_font_montserrat_20, COLOR_PAPER);
    game_recipe_t recipe = (game_recipe_t)s_recipe_selection;
    const game_recipe_definition_t *definition = game_recipe_definition(recipe);
    lv_obj_t *card = rect(screen, 10, 62, 220, 178, COLOR_GOLD);
    lv_obj_set_style_border_width(card, 3, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(COLOR_INK), 0);
    label(card, definition->name, 10, 12, &lv_font_montserrat_20, COLOR_RED);
    char line[42];
    bool unlocked = (s_game.unlocked_recipes & (1U << recipe)) != 0U;
    snprintf(line, sizeof(line), "%s / %lu MIN / VALUE %u",
             unlocked ? "UNLOCKED" : "LOCKED",
             (unsigned long)(definition->cook_seconds / 60U),
             definition->sell_price);
    label(card, line, 10, 52, &lv_font_montserrat_14, COLOR_INK);
    snprintf(line, sizeof(line), "CROP %u  BERRIES %u",
             ui_crop_count(definition->crop_a), s_game.inventory_berries);
    label(card, line, 10, 84, &lv_font_montserrat_14, COLOR_INK);
    label(card, s_game.kitchen.active ? "ATUAN IS COOKING" : "OK: START COOKING",
          10, 124, &lv_font_montserrat_14,
          s_game.kitchen.active ? COLOR_MUTED : COLOR_RED);
    label(screen, "UP/DOWN RECIPE  HOLD OK: BACK", 10, 276,
          &lv_font_montserrat_14, COLOR_INK);
    activate_screen();
}

static void build_travel(void)
{
    lv_obj_t *screen = new_screen(COLOR_HILL);
    rect(screen, 0, 0, 240, 48, COLOR_NIGHT);
    label(screen, "TRAVEL", 10, 10, &lv_font_montserrat_20, COLOR_PAPER);
    lv_obj_t *card = rect(screen, 10, 60, 220, 172, COLOR_PAPER);
    lv_obj_set_style_border_width(card, 3, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(COLOR_WOOD_DARK), 0);
    label(card, "MISTPINE FOREST", 10, 12, &lv_font_montserrat_20, COLOR_RED);
    if (s_game.travel.active) {
        label(card, "AMAI + ATUAN EXPLORING", 10, 51,
              &lv_font_montserrat_14, COLOR_INK);
        char eta[32];
        uint32_t remaining = s_game.travel.ends_at > s_now
            ? s_game.travel.ends_at - s_now : 0U;
        snprintf(eta, sizeof(eta), "RETURN IN %luh %02lum",
                 (unsigned long)(remaining / 3600U),
                 (unsigned long)((remaining / 60U) % 60U));
        label(card, eta, 10, 78, &lv_font_montserrat_14, COLOR_MUTED);
    } else if (s_game.spring_day < 8U) {
        label(card, "LOCKED UNTIL SPRING 8", 10, 51,
              &lv_font_montserrat_14, COLOR_MUTED);
    } else {
        label(card, "8 HOURS / 1 HOT BREAD", 10, 51,
              &lv_font_montserrat_14, COLOR_INK);
        label(card, "OK: SEND AMAI + ATUAN", 10, 78,
              &lv_font_montserrat_14, COLOR_RED);
    }
    char journal[32];
    snprintf(journal, sizeof(journal), "TRAVEL JOURNALS %u",
             s_game.travel_journal_count);
    label(card, journal, 10, 120, &lv_font_montserrat_14, COLOR_INK);
    label(screen, "HOLD OK: STATION", 10, 276,
          &lv_font_montserrat_14, COLOR_INK);
    activate_screen();
}

static void build_backpack(void)
{
    lv_obj_t *screen = new_screen(COLOR_PAPER);
    rect(screen, 0, 0, 240, 48, COLOR_NIGHT);
    label(screen, "BACKPACK", 10, 10, &lv_font_montserrat_20, COLOR_PAPER);
    const char *titles[] = { "ITEMS", "PARTNERS", "STATION", "TASKS", "ALBUM", "SETTINGS" };
    for (int i = 0; i < 6; i++) {
        int x = 9 + (i % 2) * 113;
        int y = 57 + (i / 2) * 59;
        lv_obj_t *card = rect(screen, x, y, 109, 49,
                              i == s_backpack_selection ? COLOR_GOLD : COLOR_PAPER);
        lv_obj_set_style_border_width(card, 2, 0);
        lv_obj_set_style_border_color(card, lv_color_hex(COLOR_INK), 0);
        label(card, titles[i], 7, 5, &lv_font_montserrat_14, COLOR_RED);
    }
    char inventory[48];
    snprintf(inventory, sizeof(inventory), "G%lu W%u B%u F%u M%u J%u",
             (unsigned long)s_game.coins, s_game.inventory_wood,
             s_game.inventory_berries, s_game.inventory_hot_bread,
             s_game.inventory_mushrooms, s_game.travel_journal_count);
    label(screen, inventory, 10, 236, &lv_font_montserrat_14, COLOR_INK);
    char quest[40];
    snprintf(quest, sizeof(quest), "MAIN QUEST %u/10%s",
             s_game.quest_stage > 10U ? 10U : s_game.quest_stage,
             s_game.chapter_complete ? " COMPLETE" : "");
    label(screen, quest, 10, 256, &lv_font_montserrat_14, COLOR_RED);
    label(screen, "HOLD OK: STATION", 10, 276,
          &lv_font_montserrat_14, COLOR_INK);
    activate_screen();
}

static void build_buildings(void)
{
    lv_obj_t *screen = new_screen(COLOR_PAPER);
    rect(screen, 0, 0, 240, 48, COLOR_NIGHT);
    label(screen, "BUILDINGS", 10, 10, &lv_font_montserrat_20, COLOR_PAPER);
    for (int i = 0; i < GAME_BUILD_COUNT; i++) {
        const game_building_definition_t *definition =
            game_building_definition((game_building_t)i);
        int y = 53 + i * 34;
        lv_obj_t *row = rect(screen, 8, y, 224, 30,
                             i == s_building_selection ? COLOR_GOLD : COLOR_PAPER);
        lv_obj_set_style_border_width(row, 2, 0);
        lv_obj_set_style_border_color(row, lv_color_hex(COLOR_INK), 0);
        char line[42];
        bool done = (s_game.completed_buildings & (1U << i)) != 0U;
        snprintf(line, sizeof(line), "%s  %s W%u G%u", definition->name,
                 done ? "DONE" : "", definition->wood, definition->coins);
        label(row, line, 6, 6, &lv_font_montserrat_14,
              done ? COLOR_MUTED : COLOR_INK);
    }
    label(screen, "OK BUILD  HOLD OK: BACK", 10, 276,
          &lv_font_montserrat_14, COLOR_INK);
    activate_screen();
}

void app_ui_start(const bool ok[APP_UI_DEMO_COUNT])
{
    (void)ok;
    uint32_t now = 22600U;
    clock_service_now(&now);
    s_now = now;
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
    s_farm_selection = 0;
    s_farm_crop_selection = GAME_CROP_WHEAT;
    s_recipe_selection = GAME_RECIPE_HOT_BREAD;
    s_backpack_selection = 0;
    s_building_selection = 0;
    build_station();
}

void app_ui_handle_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    if (ev != BSP_BTN_CLICK && ev != BSP_BTN_LONG) return;

    if (s_page == PAGE_FARM_DETAIL) {
        if (btn == BSP_BTN_OK && ev == BSP_BTN_LONG) {
            s_page = PAGE_FARM;
            build_farm();
        } else if (!s_game.farm[s_farm_selection].active &&
                   ev == BSP_BTN_CLICK &&
                   (btn == BSP_BTN_UP || btn == BSP_BTN_DOWN)) {
            int delta = btn == BSP_BTN_UP ? -1 : 1;
            s_farm_crop_selection += delta;
            if (s_farm_crop_selection < GAME_CROP_WHEAT) {
                s_farm_crop_selection = GAME_CROP_HERB;
            } else if (s_farm_crop_selection >= GAME_CROP_COUNT) {
                s_farm_crop_selection = GAME_CROP_WHEAT;
            }
            build_farm_detail();
        } else if (!s_game.farm[s_farm_selection].active &&
                   ev == BSP_BTN_CLICK && btn == BSP_BTN_OK) {
            uint32_t now = s_game.last_trusted_time;
            clock_service_now(&now);
            s_now = now;
            game_state_t candidate = s_game;
            if (now > candidate.last_settled_time) {
                game_reduce(&candidate, (game_action_t){
                    .type = GAME_ACTION_SETTLE_TO_TIME, .now = now,
                });
            }
            if (game_reduce(&candidate, (game_action_t){
                    .type = GAME_ACTION_PLANT_CROP,
                    .now = now,
                    .target = (uint8_t)s_farm_selection,
                    .option = (uint8_t)s_farm_crop_selection,
                }) && app_persistence_store(&candidate)) {
                s_game = candidate;
            }
            build_farm_detail();
        }
        return;
    }

    if (s_page == PAGE_KITCHEN) {
        if (btn == BSP_BTN_OK && ev == BSP_BTN_LONG) {
            s_page = PAGE_SCHEDULE;
            build_schedule();
        } else if (ev == BSP_BTN_CLICK &&
                   (btn == BSP_BTN_UP || btn == BSP_BTN_DOWN)) {
            int delta = btn == BSP_BTN_UP ? -1 : 1;
            s_recipe_selection = (s_recipe_selection + delta +
                                  GAME_RECIPE_COUNT) % GAME_RECIPE_COUNT;
            build_kitchen();
        } else if (ev == BSP_BTN_CLICK && btn == BSP_BTN_OK &&
                   !s_game.kitchen.active) {
            uint32_t now = s_game.last_trusted_time;
            clock_service_now(&now);
            s_now = now;
            game_state_t candidate = s_game;
            if (now > candidate.last_settled_time) {
                game_reduce(&candidate, (game_action_t){
                    .type = GAME_ACTION_SETTLE_TO_TIME, .now = now,
                });
            }
            if (game_reduce(&candidate, (game_action_t){
                    .type = GAME_ACTION_START_RECIPE,
                    .now = now,
                    .target = (uint8_t)s_recipe_selection,
                }) && app_persistence_store(&candidate)) {
                s_game = candidate;
            }
            build_kitchen();
        }
        return;
    }

    if (s_page == PAGE_BUILDINGS) {
        if (btn == BSP_BTN_OK && ev == BSP_BTN_LONG) {
            s_page = PAGE_BACKPACK;
            build_backpack();
        } else if (ev == BSP_BTN_CLICK &&
                   (btn == BSP_BTN_UP || btn == BSP_BTN_DOWN)) {
            int delta = btn == BSP_BTN_UP ? -1 : 1;
            s_building_selection = (s_building_selection + delta +
                                    GAME_BUILD_COUNT) % GAME_BUILD_COUNT;
            build_buildings();
        } else if (ev == BSP_BTN_CLICK && btn == BSP_BTN_OK) {
            uint32_t now = s_game.last_trusted_time;
            clock_service_now(&now);
            s_now = now;
            game_state_t candidate = s_game;
            if (now > candidate.last_settled_time) {
                game_reduce(&candidate, (game_action_t){
                    .type = GAME_ACTION_SETTLE_TO_TIME, .now = now,
                });
            }
            if (game_reduce(&candidate, (game_action_t){
                    .type = GAME_ACTION_START_BUILDING,
                    .now = now,
                    .target = (uint8_t)s_building_selection,
                }) && app_persistence_store(&candidate)) {
                s_game = candidate;
            }
            build_buildings();
        }
        return;
    }

    if (s_page == PAGE_TRAVEL || s_page == PAGE_BACKPACK) {
        if (btn == BSP_BTN_OK && ev == BSP_BTN_LONG) {
            s_page = PAGE_STATION;
            s_top_selection = 0;
            build_station();
        } else if (s_page == PAGE_BACKPACK && ev == BSP_BTN_CLICK &&
                   (btn == BSP_BTN_UP || btn == BSP_BTN_DOWN)) {
            int delta = btn == BSP_BTN_UP ? -1 : 1;
            s_backpack_selection = (s_backpack_selection + delta + 6) % 6;
            build_backpack();
        } else if (s_page == PAGE_BACKPACK && ev == BSP_BTN_CLICK &&
                   btn == BSP_BTN_OK && s_backpack_selection == 2) {
            s_page = PAGE_BUILDINGS;
            s_building_selection = 0;
            build_buildings();
        } else if (s_page == PAGE_TRAVEL && ev == BSP_BTN_CLICK &&
                   btn == BSP_BTN_OK && !s_game.travel.active) {
            uint32_t now = s_game.last_trusted_time;
            clock_service_now(&now);
            s_now = now;
            game_state_t candidate = s_game;
            if (now > candidate.last_settled_time) {
                game_reduce(&candidate, (game_action_t){
                    .type = GAME_ACTION_SETTLE_TO_TIME, .now = now,
                });
            }
            if (game_reduce(&candidate, (game_action_t){
                    .type = GAME_ACTION_START_TRAVEL, .now = now,
                }) && app_persistence_store(&candidate)) {
                s_game = candidate;
            }
            build_travel();
        }
        return;
    }

    if (s_page == PAGE_FARM) {
        if (btn == BSP_BTN_OK && ev == BSP_BTN_LONG) {
            s_page = PAGE_STATION;
            s_top_selection = 0;
            build_station();
        } else if (ev == BSP_BTN_CLICK &&
                   (btn == BSP_BTN_UP || btn == BSP_BTN_DOWN)) {
            s_farm_selection = (s_farm_selection + 1) % GAME_FARM_PLOT_COUNT;
            build_farm();
        } else if (ev == BSP_BTN_CLICK && btn == BSP_BTN_OK) {
            s_page = PAGE_FARM_DETAIL;
            s_farm_crop_selection = GAME_CROP_WHEAT;
            build_farm_detail();
        }
        return;
    }

    if (s_page == PAGE_SCHEDULE) {
        if (btn == BSP_BTN_OK && ev == BSP_BTN_LONG) {
            s_page = PAGE_STATION;
            s_top_selection = 0;
            build_station();
        } else if (ev == BSP_BTN_CLICK &&
                   (btn == BSP_BTN_UP || btn == BSP_BTN_DOWN)) {
            s_schedule_selection = (s_schedule_selection + 1) % 4;
            refresh_schedule_selection();
        } else if (ev == BSP_BTN_CLICK && btn == BSP_BTN_OK &&
                   s_schedule_selection == 0 && s_game.pending_events != 0U) {
            game_state_t candidate = s_game;
            uint8_t event = (candidate.pending_events & GAME_EVENT_MARKET)
                ? GAME_EVENT_MARKET : GAME_EVENT_FESTIVAL;
            if (game_reduce(&candidate, (game_action_t){
                    .type = GAME_ACTION_RESOLVE_EVENT, .target = event,
                }) && app_persistence_store(&candidate)) {
                s_game = candidate;
            }
            build_schedule();
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
                   s_schedule_selection == 3 && !s_game.kitchen.active) {
            s_page = PAGE_KITCHEN;
            s_recipe_selection = GAME_RECIPE_HOT_BREAD;
            build_kitchen();
        } else if (ev == BSP_BTN_CLICK && btn == BSP_BTN_OK &&
                   s_schedule_selection == 2 && !s_game.forest.active) {
            uint32_t now = s_game.last_trusted_time;
            clock_service_now(&now);
            game_state_t candidate = s_game;
            if (now > candidate.last_settled_time) {
                game_reduce(&candidate, (game_action_t){
                    .type = GAME_ACTION_SETTLE_TO_TIME,
                    .now = now,
                });
            }
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
        s_top_selection = (s_top_selection + 1) % 5;
        build_station();
    } else if (btn == BSP_BTN_OK && s_top_selection == 1) {
        s_page = PAGE_SCHEDULE;
        s_schedule_selection = 0;
        build_schedule();
    } else if (btn == BSP_BTN_OK && s_top_selection == 2) {
        s_page = PAGE_FARM;
        s_farm_selection = 0;
        build_farm();
    } else if (btn == BSP_BTN_OK && s_top_selection == 3) {
        s_page = PAGE_TRAVEL;
        build_travel();
    } else if (btn == BSP_BTN_OK && s_top_selection == 4) {
        s_page = PAGE_BACKPACK;
        build_backpack();
    }
}
