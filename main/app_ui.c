#include "app_ui.h"
#include "app_i18n.h"
#include "ui_theme.h"
#include "visual_assets.h"
#include "game/game_state.h"
#include "game/game_content.h"
#include "services/app_persistence.h"
#include "services/clock_service.h"
#include "services/telemetry.h"
#include "lvgl.h"

#include <stdio.h>

LV_FONT_DECLARE(app_font_zh_14)
LV_FONT_DECLARE(app_font_zh_20)

#define COLOR_NIGHT       UI_THEME_HEADER_BLUE
#define COLOR_SKY         UI_THEME_SKY_BLUE
#define COLOR_HILL        UI_THEME_MOUNTAIN_TEAL
#define COLOR_GRASS       UI_THEME_SPRING_GREEN
#define COLOR_WOOD        UI_THEME_TIMBER
#define COLOR_WOOD_DARK   UI_THEME_DARK_TIMBER
#define COLOR_PAPER       UI_THEME_PARCHMENT
#define COLOR_GOLD        UI_THEME_WARM_YELLOW
#define COLOR_RED         UI_THEME_RED_ACCENT
#define COLOR_INK         UI_THEME_INK
#define COLOR_MUTED       UI_THEME_MUTED

typedef enum {
    PAGE_STATION = 0,
    PAGE_SCHEDULE,
    PAGE_FARM,
    PAGE_TRAVEL,
    PAGE_BACKPACK,
    PAGE_FARM_DETAIL,
    PAGE_KITCHEN,
    PAGE_BUILDINGS,
    PAGE_BACKPACK_DETAIL,
    PAGE_EVENT,
    PAGE_COOK_ASSIST,
    PAGE_NOTICE,
    PAGE_FOREST,
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
static int s_partner_selection;
static int s_event_choice;
static int s_item_selection;
static int s_travel_goal;
static int s_forest_duration;
static uint32_t s_now;
static lv_obj_t *s_bottom_tabs[5];
static lv_timer_t *s_menu_timer;
static lv_timer_t *s_heat_timer;
static lv_obj_t *s_heat_marker;
static uint32_t s_heat_started_at;
static uint8_t s_heat_accuracy;
static uint32_t s_last_input_tick;
static bool s_menu_hidden;

static app_language_t ui_language(void)
{
    return s_game.language_english ? APP_LANG_EN : APP_LANG_ZH_CN;
}

static const char *tr(const char *zh_cn, const char *en)
{
    return app_i18n_pick(ui_language(), zh_cn, en);
}

static const lv_font_t *ui_font_14(void)
{
    return &app_font_zh_14;
}

static const lv_font_t *ui_font_20(void)
{
    return &app_font_zh_20;
}

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

static lv_obj_t *label_one_line(lv_obj_t *parent, const char *text, int x, int y,
                                int width, const lv_font_t *font, uint32_t color)
{
    lv_obj_t *obj = label(parent, text, x, y, font, color);
    lv_obj_set_size(obj, width, lv_font_get_line_height(font));
    lv_label_set_long_mode(obj, LV_LABEL_LONG_DOT);
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
    case PAGE_BACKPACK_DETAIL: {
        static const char *details[6] = {
            "page_items", "page_partners", "page_buildings",
            "page_tasks", "page_album", "page_settings",
        };
        checkpoint = details[s_backpack_selection];
        break;
    }
    case PAGE_EVENT: checkpoint = "page_event"; break;
    case PAGE_COOK_ASSIST: checkpoint = "page_cook_assist"; break;
    case PAGE_NOTICE: checkpoint = "page_notice"; break;
    case PAGE_FOREST: checkpoint = "page_forest"; break;
    }
    telemetry_log_memory(checkpoint);
}

static void draw_inn(lv_obj_t *parent)
{
    lv_obj_t *scene = lv_image_create(parent);
    lv_image_set_src(scene, &visual_station_scene);
    lv_obj_set_pos(scene, 0, 0);
    lv_obj_remove_flag(scene, LV_OBJ_FLAG_SCROLLABLE);
    if (s_game.weather == GAME_WEATHER_RAIN ||
        s_game.weather == GAME_WEATHER_STORM) {
        for (int i = 0; i < 12; i++) {
            rect(parent, 6 + i * 20, 35 + (i % 7) * 39, 2, 12, COLOR_PAPER);
        }
    }
}

static void draw_top_status(lv_obj_t *parent)
{
    char time_text[8];
    uint32_t hour = (s_now / 3600U) % 24U;
    if (!s_game.clock_24_hour) {
        hour %= 12U;
        if (hour == 0U) hour = 12U;
    }
    snprintf(time_text, sizeof(time_text), "%02lu:%02lu",
             (unsigned long)hour,
             (unsigned long)((s_now / 60U) % 60U));
    lv_obj_t *time_label = label(parent, time_text, 9, 7, ui_font_20(), COLOR_PAPER);
    char calendar[128];
    snprintf(calendar, sizeof(calendar), tr("春 %u日 / %s", "SPR %u / %s"),
             s_game.spring_day, app_i18n_weather(ui_language(), s_game.weather));
    lv_obj_t *calendar_pill = rect(parent, 73, 6, 112, 24, COLOR_NIGHT);
    lv_obj_set_style_bg_opa(calendar_pill, UI_THEME_STATUS_OPA, 0);
    lv_obj_set_style_radius(calendar_pill, 12, 0);
    lv_obj_set_style_border_width(calendar_pill, 1, 0);
    lv_obj_set_style_border_color(calendar_pill, lv_color_hex(COLOR_PAPER), 0);
    lv_obj_set_style_border_opa(calendar_pill, LV_OPA_50, 0);
    lv_obj_t *calendar_label = label_one_line(calendar_pill, calendar, 6, 4, 100,
                                               ui_font_14(), COLOR_PAPER);
    lv_obj_set_style_text_align(calendar_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_t *battery_label = label(parent, "82%", 197, 11, ui_font_14(), COLOR_PAPER);
    lv_obj_t *status_labels[] = { time_label, calendar_label, battery_label };
    for (size_t i = 0; i < sizeof(status_labels) / sizeof(status_labels[0]); i++) {
        lv_obj_set_style_text_outline_stroke_width(
            status_labels[i], UI_THEME_TEXT_OUTLINE_WIDTH, 0);
        lv_obj_set_style_text_outline_stroke_color(status_labels[i],
                                                   lv_color_hex(COLOR_NIGHT), 0);
        lv_obj_set_style_text_outline_stroke_opa(status_labels[i], LV_OPA_COVER, 0);
    }
}

static void draw_bottom_tabs(lv_obj_t *parent)
{
    const char *items[5] = {
        tr("驿站", "INN"), tr("计划", "PLAN"), tr("农田", "FARM"),
        tr("旅行", "TRIP"), tr("背包", "BAG"),
    };
    for (int i = 0; i < 5; i++) {
        int x = 3 + i * 47;
        lv_obj_t *tab = rect(parent, x, 278, 46, 32,
                             s_top_selection == i ? COLOR_GOLD : COLOR_PAPER);
        s_bottom_tabs[i] = tab;
        lv_obj_set_style_border_width(tab, 3, 0);
        lv_obj_set_style_border_color(tab, lv_color_hex(COLOR_INK), 0);
        lv_obj_set_style_bg_opa(tab, UI_THEME_OVERLAY_OPA, 0);
        lv_obj_t *text = label(tab, items[i], 0, 7, ui_font_14(), COLOR_INK);
        lv_obj_set_width(text, 42);
        lv_obj_set_style_text_align(text, LV_TEXT_ALIGN_CENTER, 0);
        if (s_menu_hidden) lv_obj_add_flag(tab, LV_OBJ_FLAG_HIDDEN);
    }
}

static void menu_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (s_page != PAGE_STATION || s_menu_hidden ||
        lv_tick_elaps(s_last_input_tick) < 5000U) {
        return;
    }
    s_menu_hidden = true;
    for (size_t i = 0; i < 5U; i++) {
        if (s_bottom_tabs[i]) lv_obj_add_flag(s_bottom_tabs[i], LV_OBJ_FLAG_HIDDEN);
    }
}

static void build_station(void)
{
    lv_obj_t *screen = new_screen(COLOR_NIGHT);
    draw_inn(screen);
    draw_top_status(screen);
    lv_obj_t *note = rect(screen, 9, 184, 222, 82, COLOR_PAPER);
    lv_obj_set_style_border_width(note, 3, 0);
    lv_obj_set_style_border_color(note, lv_color_hex(COLOR_GOLD), 0);
    lv_obj_set_style_radius(note, UI_THEME_CARD_RADIUS, 0);
    lv_obj_set_style_shadow_width(note, 8, 0);
    lv_obj_set_style_shadow_color(note, lv_color_hex(COLOR_INK), 0);
    lv_obj_set_style_shadow_opa(note, LV_OPA_40, 0);
    lv_obj_set_style_bg_opa(note, UI_THEME_OVERLAY_OPA, 0);
    label(note, tr("灯火已经点亮", "THE LANTERN IS LIT"),
          10, 9, ui_font_14(), COLOR_RED);
    uint32_t hour = (s_now / 3600U) % 24U;
    uint8_t period = hour < 6U ? 3U : (hour < 11U ? 0U :
                     (hour < 17U ? 1U : (hour < 21U ? 2U : 3U)));
    const char *dialogue = app_i18n_dialogue(
        ui_language(), s_game.weather, period,
        s_game.weather_seed ^ s_game.spring_day);
    label_one_line(note, dialogue, 10, 31, 202,
                   ui_font_14(), COLOR_INK);
    if (s_game.pending_events != 0U || s_game.event_queue_count > 0U) {
        label(note, tr("！计划中有剧情事件", "! STORY EVENT IN PLAN"), 10, 52,
              ui_font_14(), COLOR_RED);
    } else if (s_game.notifications != 0U) {
        label(note, tr("！有新的任务结果", "! NEW TASK RESULT"), 10, 52,
              ui_font_14(), COLOR_RED);
    } else if (s_game.pending.available) {
        label(note, tr("！离线报告已生成", "! OFFLINE REPORT READY"), 10, 52,
              ui_font_14(), COLOR_RED);
    } else if (s_game.travel.active) {
        label(note, tr("旅行队正在外出", "TRAVEL TEAM IS AWAY"), 10, 52,
              ui_font_14(), COLOR_RED);
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
    label(screen, tr("计划", "SCHEDULE"), 10, 10, ui_font_20(), COLOR_PAPER);
    s_schedule_rows[0] = rect(screen, 9, 52, 222, 45, COLOR_PAPER);
    s_schedule_rows[1] = rect(screen, 9, 102, 222, 45, COLOR_PAPER);
    s_schedule_rows[2] = rect(screen, 9, 152, 222, 45, COLOR_PAPER);
    s_schedule_rows[3] = rect(screen, 9, 202, 222, 45, COLOR_PAPER);
    for (int i = 0; i < 4; i++) {
        lv_obj_set_style_border_width(s_schedule_rows[i], 3, 0);
        lv_obj_set_style_border_color(s_schedule_rows[i], lv_color_hex(COLOR_INK), 0);
    }

    char report_line[128];
    if (s_game.pending_events & GAME_EVENT_MARKET) {
        snprintf(report_line, sizeof(report_line), "%s",
                 tr("路边集市 · 按OK前往", "ROAD MARKET - OK TO VISIT"));
    } else if (s_game.pending_events & GAME_EVENT_FESTIVAL) {
        snprintf(report_line, sizeof(report_line), "%s",
                 tr("灯会 · 按OK参加", "LANTERN FEST - OK TO JOIN"));
    } else if (s_game.event_queue_count > 0U) {
        snprintf(report_line, sizeof(report_line), "%s",
                 app_i18n_event(ui_language(), s_game.event_queue[0].id));
    } else if (s_game.notifications != 0U) {
        snprintf(report_line, sizeof(report_line),
                 tr("%u项结果 · 按OK查看", "%u RESULT(S) - OK TO VIEW"),
                 (unsigned)__builtin_popcount((unsigned)s_game.notifications));
    } else if (s_game.pending.available) {
        snprintf(report_line, sizeof(report_line), "+%luG +%uW +%uB +%uH +%uF",
                 (unsigned long)s_game.pending.coins,
                 s_game.pending.wood, s_game.pending.berries,
                 s_game.pending.hot_bread, s_game.pending.wheat);
    } else {
        snprintf(report_line, sizeof(report_line), "%s",
                 tr("暂无待领取奖励", "NO REWARD PENDING"));
    }
    label(s_schedule_rows[0],
          (s_game.pending_events || s_game.event_queue_count)
              ? tr("剧情事件", "STORY EVENT")
              : (s_game.notifications ? tr("任务结果", "TASK RESULTS")
                                      : tr("离线报告", "OFFLINE REPORT")),
          9, 4,
          ui_font_14(), COLOR_RED);
    label_one_line(s_schedule_rows[0], report_line, 9, 23, 198,
                   ui_font_14(), COLOR_INK);
    label(s_schedule_rows[1], tr("前台接待", "RECEPTION"), 9, 4,
          ui_font_14(), COLOR_RED);
    label(s_schedule_rows[1], app_i18n_pet(ui_language(), GAME_PET_MOMO), 9, 23,
          ui_font_14(), COLOR_INK);
    char stamina[128];
    game_job_score_t reception_score = game_calculate_job_score(
        &s_game, GAME_PET_MOMO, GAME_JOB_RECEPTION, GAME_PET_COUNT);
    snprintf(stamina, sizeof(stamina), tr("体%u 评分%d", "E%u SCORE %d"),
             s_game.momo.stamina, reception_score.score);
    label(s_schedule_rows[1], stamina, 128, 23,
          ui_font_14(), COLOR_MUTED);

    label(s_schedule_rows[2], tr("森林 / 30分钟", "FOREST  /  30 MIN"), 9, 4,
          ui_font_14(), COLOR_RED);
    char forest_line[128];
    game_job_score_t forest_score = game_calculate_job_score(
        &s_game, GAME_PET_AMAI, GAME_JOB_FOREST, GAME_PET_COUNT);
    snprintf(forest_line, sizeof(forest_line), tr("%s  评分%d", "%s  SCORE %d"),
             s_game.forest.active ? tr("探索中", "EXPLORING")
                                  : tr("按OK派遣", "OK SEND"),
             forest_score.score);
    label(s_schedule_rows[2], forest_line, 9, 23,
          ui_font_14(), COLOR_INK);
    label(s_schedule_rows[3], tr("厨房 / 热面包", "KITCHEN / HOT BREAD"), 9, 4,
          ui_font_14(), COLOR_RED);
    char kitchen_line[128];
    snprintf(kitchen_line, sizeof(kitchen_line), tr("%s  小麦%u", "%s  WHEAT %u"),
             s_game.kitchen.active ? tr("阿团烹饪中", "ATUAN COOKING")
                                   : tr("按OK烹饪", "OK TO COOK"),
             s_game.inventory_wheat);
    label(s_schedule_rows[3], kitchen_line, 9, 23,
          ui_font_14(), COLOR_INK);
    label(screen, tr("上下选择  OK操作", "UP/DOWN SELECT   OK ACTION"), 10, 252,
          ui_font_14(), COLOR_MUTED);
    label(screen, tr("长按OK：返回驿站", "HOLD OK: STATION"), 10, 282,
          ui_font_14(), COLOR_INK);
    refresh_schedule_selection();
    activate_screen();
}

static void build_farm(void)
{
    lv_obj_t *screen = new_screen(COLOR_GRASS);
    rect(screen, 0, 48, 240, 228, 0x679B3C);
    for (int y = 51; y < 276; y += 18) {
        rect(screen, 0, y, 240, 2, 0x7DAF4D);
    }
    rect(screen, 0, 0, 240, 48, COLOR_NIGHT);
    label(screen, tr("农田", "FARM"), 10, 10, ui_font_20(), COLOR_PAPER);
    char stock[128];
    snprintf(stock, sizeof(stock), tr("种子%u  小麦%u", "SEEDS %u  WHEAT %u"),
             s_game.inventory_wheat_seed, s_game.inventory_wheat);
    label(screen, stock, 78, 16, ui_font_14(), COLOR_GOLD);

    uint8_t plot_count = game_available_farm_plots(&s_game);
    for (int i = 0; i < plot_count; i++) {
        int y = 54 + i * 34;
        lv_obj_t *plot = rect(screen, 10, y, 220, 30, COLOR_PAPER);
        lv_obj_set_style_border_width(plot, i == s_farm_selection ? 3 : 2, 0);
        lv_obj_set_style_border_color(plot,
            lv_color_hex(i == s_farm_selection ? COLOR_GOLD : COLOR_WOOD_DARK), 0);
        lv_obj_set_style_radius(plot, 4, 0);
        if (i == s_farm_selection) {
            lv_obj_set_style_shadow_width(plot, 7, 0);
            lv_obj_set_style_shadow_color(plot, lv_color_hex(COLOR_INK), 0);
            lv_obj_set_style_shadow_opa(plot, LV_OPA_30, 0);
        }
        uint32_t crop_color = s_game.farm[i].active ? 0x95B84C : COLOR_MUTED;
        lv_obj_t *crop_mark = rect(plot, 7, 7, 12, 12, crop_color);
        lv_obj_set_style_radius(crop_mark, LV_RADIUS_CIRCLE, 0);
        char line[128];
        game_crop_t crop = s_game.farm[i].crop;
        snprintf(line, sizeof(line), tr("田地%d  %s", "PLOT %d  %s"), i + 1,
                 s_game.farm[i].active
                     ? app_i18n_crop(ui_language(), crop)
                     : tr("按OK查看", "OK FOR DETAILS"));
        label_one_line(plot, line, 25, 5, 185,
                       ui_font_14(), COLOR_INK);
    }
    label(screen, tr("长按OK：返回驿站", "HOLD OK: STATION"), 10, 276,
          ui_font_14(), COLOR_INK);
    activate_screen();
}

static void build_farm_detail(void)
{
    lv_obj_t *screen = new_screen(COLOR_GRASS);
    rect(screen, 0, 0, 240, 48, COLOR_NIGHT);
    char title[128];
    snprintf(title, sizeof(title), tr("田地 %d", "PLOT %d"), s_farm_selection + 1);
    label(screen, title, 10, 10, ui_font_20(), COLOR_PAPER);
    lv_obj_t *card = rect(screen, 10, 61, 220, 174, COLOR_PAPER);
    lv_obj_set_style_border_width(card, 3, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(COLOR_WOOD_DARK), 0);
    if (s_game.farm[s_farm_selection].active) {
        label(card, app_i18n_crop(ui_language(), s_game.farm[s_farm_selection].crop),
              10, 12, ui_font_20(), COLOR_RED);
        uint32_t remaining = s_game.farm[s_farm_selection].matures_at > s_now
            ? s_game.farm[s_farm_selection].matures_at - s_now : 0U;
        char line[128];
        snprintf(line, sizeof(line), tr("生长良好 / %lu时%02lu分", "HEALTHY / %luh %02lum"),
                 (unsigned long)(remaining / 3600U),
                 (unsigned long)((remaining / 60U) % 60U));
        label(card, line, 10, 55, ui_font_14(), COLOR_INK);
        label(card, tr("照料者：露露", "CARETAKER: LULU"), 10, 88, ui_font_14(), COLOR_INK);
    } else {
        game_crop_t crop = (game_crop_t)s_farm_crop_selection;
        const game_crop_definition_t *definition = game_crop_definition(crop);
        label(card, app_i18n_crop(ui_language(), crop), 10, 12, ui_font_20(), COLOR_RED);
        char line[128];
        snprintf(line, sizeof(line), tr("种子%u / 生长%lu小时", "SEEDS %u / GROW %luh"),
                 ui_seed_count(crop),
                 (unsigned long)(definition->grow_seconds / 3600U));
        label(card, line, 10, 55, ui_font_14(), COLOR_INK);
        label(card, tr("上下：选择作物", "UP/DOWN: CHOOSE CROP"), 10, 88,
              ui_font_14(), COLOR_MUTED);
        label(card, tr("OK：播种", "OK: PLANT"), 10, 120, ui_font_14(), COLOR_RED);
    }
    label(screen, tr("长按OK：返回", "HOLD OK: BACK"), 10, 276, ui_font_14(), COLOR_INK);
    activate_screen();
}

static void build_kitchen(void)
{
    lv_obj_t *screen = new_screen(COLOR_PAPER);
    rect(screen, 0, 0, 240, 48, COLOR_NIGHT);
    label(screen, tr("厨房", "KITCHEN"), 10, 10, ui_font_20(), COLOR_PAPER);
    game_recipe_t recipe = (game_recipe_t)s_recipe_selection;
    const game_recipe_definition_t *definition = game_recipe_definition(recipe);
    game_job_score_t kitchen_score = game_calculate_job_score(
        &s_game, GAME_PET_ATUAN, GAME_JOB_KITCHEN, GAME_PET_COUNT);
    lv_obj_t *card = rect(screen, 10, 62, 220, 178, COLOR_GOLD);
    lv_obj_set_style_border_width(card, 3, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(COLOR_INK), 0);
    label(card, app_i18n_recipe(ui_language(), recipe),
          10, 12, ui_font_20(), COLOR_RED);
    char line[128];
    bool unlocked = (s_game.unlocked_recipes & (1U << recipe)) != 0U;
    snprintf(line, sizeof(line), tr("%s / %lu分 / 评分%d", "%s / %luM / SCORE %d"),
             unlocked ? tr("已解锁", "UNLOCKED") : tr("未解锁", "LOCKED"),
             (unsigned long)(definition->cook_seconds / 60U),
             kitchen_score.score);
    label(card, line, 10, 52, ui_font_14(), COLOR_INK);
    snprintf(line, sizeof(line), tr("作物%u  浆果%u", "CROP %u  BERRIES %u"),
             ui_crop_count(definition->crop_a), s_game.inventory_berries);
    label(card, line, 10, 84, ui_font_14(), COLOR_INK);
    char action_line[128];
    if (s_game.kitchen.active) {
        snprintf(action_line, sizeof(action_line), "%s",
                 s_game.kitchen.kind == GAME_TASK_RECIPE_RESEARCH
                 ? tr("阿团研究中", "ATUAN IS RESEARCHING")
                 : tr("阿团烹饪中", "ATUAN IS COOKING"));
    } else if (unlocked) {
        snprintf(action_line, sizeof(action_line), "%s",
                 tr("OK：开始烹饪", "OK: START COOKING"));
    } else {
        snprintf(action_line, sizeof(action_line), tr("OK：研究 %u%%", "OK: RESEARCH %u%%"),
                 s_game.recipe_research[recipe]);
    }
    label(card, action_line,
          10, 124, ui_font_14(),
          s_game.kitchen.active ? COLOR_MUTED : COLOR_RED);
    label(screen, tr("上下选配方  长按OK返回", "UP/DOWN RECIPE  HOLD OK: BACK"), 10, 276,
          ui_font_14(), COLOR_INK);
    activate_screen();
}

static void build_forest(void)
{
    lv_obj_t *screen = new_screen(COLOR_GRASS);
    rect(screen, 0, 0, 240, 48, COLOR_NIGHT);
    label(screen, tr("森林探索", "FOREST EXPEDITION"), 10, 10,
          ui_font_20(), COLOR_PAPER);
    lv_obj_t *card = rect(screen, 10, 62, 220, 178, COLOR_PAPER);
    lv_obj_set_style_border_width(card, 3, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(COLOR_WOOD_DARK), 0);
    game_job_score_t score = game_calculate_job_score(
        &s_game, GAME_PET_AMAI, GAME_JOB_FOREST, GAME_PET_COUNT);
    label(card, s_forest_duration == 0 ? tr("快速搜寻", "QUICK SEARCH")
                                       : tr("长途探索", "LONG EXPEDITION"),
          10, 12, ui_font_20(), COLOR_RED);
    char line[128];
    snprintf(line, sizeof(line), tr("%s / 评分%d", "%s / SCORE %d"),
             s_forest_duration == 0 ? tr("30分钟", "30 MIN")
                                    : tr("2小时", "2 HOURS"), score.score);
    label(card, line, 10, 52, ui_font_14(), COLOR_INK);
    label(card, s_forest_duration == 0
          ? tr("稳妥：木材 + 浆果", "SAFE: WOOD + BERRIES")
          : tr("风险：天气 / 稀有发现", "RISK: WEATHER / RARE FINDS"),
          10, 84, ui_font_14(), COLOR_INK);
    label(card, s_game.forest.active ? tr("OK：取消并返回", "OK: CANCEL + RETURN")
                                     : tr("OK：派遣阿麦", "OK: SEND AMAI"),
          10, 124, ui_font_14(),
          s_game.forest.active ? COLOR_MUTED : COLOR_RED);
    label(screen, tr("上下选时长  长按返回", "UP/DOWN TIME  HOLD: BACK"), 10, 276,
          ui_font_14(), COLOR_INK);
    activate_screen();
}

static void build_travel(void)
{
    lv_obj_t *screen = new_screen(COLOR_HILL);
    rect(screen, 0, 0, 240, 48, COLOR_NIGHT);
    label(screen, tr("旅行", "TRAVEL"), 10, 10, ui_font_20(), COLOR_PAPER);
    lv_obj_t *card = rect(screen, 10, 60, 220, 172, COLOR_PAPER);
    lv_obj_set_style_border_width(card, 3, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(COLOR_WOOD_DARK), 0);
    label(card, tr("雾松林", "MISTPINE FOREST"), 10, 12, ui_font_20(), COLOR_RED);
    if (s_game.travel.active) {
        label(card, tr("阿麦 + 阿团探索中", "AMAI + ATUAN EXPLORING"), 10, 51,
              ui_font_14(), COLOR_INK);
        char eta[128];
        uint32_t remaining = s_game.travel.ends_at > s_now
            ? s_game.travel.ends_at - s_now : 0U;
        snprintf(eta, sizeof(eta), tr("%lu时%02lu分后返回", "RETURN IN %luh %02lum"),
                 (unsigned long)(remaining / 3600U),
                 (unsigned long)((remaining / 60U) % 60U));
        label(card, eta, 10, 78, ui_font_14(), COLOR_MUTED);
        label(card, tr("OK：取消并退还面包", "OK: CANCEL + REFUND BREAD"), 10, 103,
              ui_font_14(), COLOR_RED);
    } else if (s_game.spring_day < 8U) {
        label(card, tr("春季第8日解锁", "LOCKED UNTIL SPRING 8"), 10, 51,
              ui_font_14(), COLOR_MUTED);
    } else {
        const char *goals[GAME_TRAVEL_GOAL_COUNT] = {
            tr("材料", "MATERIALS"), tr("旧路", "OLD ROAD"),
            tr("风景", "SCENERY"),
        };
        label(card, tr("8小时 / 1个热面包", "8 HOURS / 1 HOT BREAD"), 10, 51,
              ui_font_14(), COLOR_INK);
        char goal[128];
        snprintf(goal, sizeof(goal), tr("目标：%s", "GOAL: %s"), goals[s_travel_goal]);
        label(card, goal, 10, 78, ui_font_14(), COLOR_RED);
        label(card, tr("OK派遣 / 上下选目标", "OK: SEND / UP-DOWN GOAL"), 10, 103,
              ui_font_14(), COLOR_RED);
    }
    char journal[128];
    snprintf(journal, sizeof(journal), tr("旅行日志 %u", "TRAVEL JOURNALS %u"),
             s_game.travel_journal_count);
    label(card, journal, 10, 137, ui_font_14(), COLOR_INK);
    label(screen, tr("长按OK：返回驿站", "HOLD OK: STATION"), 10, 276,
          ui_font_14(), COLOR_INK);
    activate_screen();
}

static void build_backpack(void)
{
    lv_obj_t *screen = new_screen(COLOR_PAPER);
    rect(screen, 0, 0, 240, 48, COLOR_NIGHT);
    label(screen, tr("背包", "BACKPACK"), 10, 10, ui_font_20(), COLOR_PAPER);
    const char *titles[] = {
        tr("物品", "ITEMS"), tr("伙伴", "PARTNERS"), tr("驿站", "STATION"),
        tr("任务", "TASKS"), tr("图鉴", "ALBUM"), tr("设置", "SETTINGS"),
    };
    for (int i = 0; i < 6; i++) {
        int x = 9 + (i % 2) * 113;
        int y = 57 + (i / 2) * 59;
        lv_obj_t *card = rect(screen, x, y, 109, 49,
                              i == s_backpack_selection ? COLOR_GOLD : COLOR_PAPER);
        lv_obj_set_style_border_width(card, 2, 0);
        lv_obj_set_style_border_color(card, lv_color_hex(COLOR_INK), 0);
        label(card, titles[i], 7, 5, ui_font_14(), COLOR_RED);
    }
    char inventory[128];
    snprintf(inventory, sizeof(inventory), "G%lu W%u B%u F%u M%u J%u",
             (unsigned long)s_game.coins, s_game.inventory_wood,
             s_game.inventory_berries, s_game.inventory_hot_bread,
             s_game.inventory_mushrooms, s_game.travel_journal_count);
    label(screen, inventory, 10, 236, ui_font_14(), COLOR_INK);
    char quest[128];
    snprintf(quest, sizeof(quest), tr("主线任务 %u/10%s", "MAIN QUEST %u/10%s"),
             s_game.quest_stage > 10U ? 10U : s_game.quest_stage,
             s_game.chapter_complete ? tr(" 已完成", " COMPLETE") : "");
    label(screen, quest, 10, 256, ui_font_14(), COLOR_RED);
    label(screen, tr("长按OK：返回驿站", "HOLD OK: STATION"), 10, 276,
          ui_font_14(), COLOR_INK);
    activate_screen();
}

static void build_buildings(void)
{
    lv_obj_t *screen = new_screen(COLOR_PAPER);
    rect(screen, 0, 0, 240, 48, COLOR_NIGHT);
    label(screen, tr("建筑", "BUILDINGS"), 10, 10, ui_font_20(), COLOR_PAPER);
    for (int i = 0; i < GAME_BUILD_COUNT; i++) {
        const game_building_definition_t *definition =
            game_building_definition((game_building_t)i);
        int y = 53 + i * 34;
        lv_obj_t *row = rect(screen, 8, y, 224, 30,
                             i == s_building_selection ? COLOR_GOLD : COLOR_PAPER);
        lv_obj_set_style_border_width(row, 2, 0);
        lv_obj_set_style_border_color(row, lv_color_hex(COLOR_INK), 0);
        char line[128];
        bool done = (s_game.completed_buildings & (1U << i)) != 0U;
        snprintf(line, sizeof(line), tr("%s  %s 木%u 金%u", "%s  %s W%u G%u"),
                 app_i18n_building(ui_language(), (game_building_t)i),
                 done ? tr("完成", "DONE") : "", definition->wood, definition->coins);
        label(row, line, 6, 6, ui_font_14(),
              done ? COLOR_MUTED : COLOR_INK);
    }
    label(screen, s_game.construction.active
          ? tr("OK取消建设  长按返回", "OK ACTIVE BUILD: CANCEL  HOLD: BACK")
          : tr("OK建设  长按返回", "OK BUILD  HOLD OK: BACK"), 10, 276,
          ui_font_14(), COLOR_INK);
    activate_screen();
}

static void build_backpack_detail(void)
{
    const char *titles[6] = {
        tr("物品", "ITEMS"), tr("伙伴", "PARTNERS"), tr("建筑", "BUILDINGS"),
        tr("任务", "TASKS"), tr("图鉴", "ALBUM"), tr("设置", "SETTINGS"),
    };
    lv_obj_t *screen = new_screen(COLOR_PAPER);
    rect(screen, 0, 0, 240, 48, COLOR_NIGHT);
    label(screen, titles[s_backpack_selection], 10, 10,
          ui_font_20(), COLOR_PAPER);
    lv_obj_t *card = rect(screen, 10, 61, 220, 190, COLOR_PAPER);
    lv_obj_set_style_border_width(card, 3, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(COLOR_INK), 0);
    char line[128];
    if (s_backpack_selection == 0) {
        snprintf(line, sizeof(line), tr("木%u  浆果%u  蘑菇%u", "WOOD%u  BERRY%u  MUSH%u"),
                 s_game.inventory_wood, s_game.inventory_berries,
                 s_game.inventory_mushrooms);
        label_one_line(card, line, 9, 12, 196,
                       ui_font_14(), COLOR_INK);
        snprintf(line, sizeof(line), tr("麦%u  胡萝卜%u  面包%u", "WHEAT%u  CARROT%u  BRD%u"),
                 s_game.inventory_wheat,
                 s_game.inventory_crops[GAME_CROP_CARROT],
                 s_game.inventory_hot_bread);
        label_one_line(card, line, 9, 48, 196,
                       ui_font_14(), COLOR_INK);
        snprintf(line, sizeof(line), tr("种子 麦%u 胡%u 莓%u 草%u", "SEED W%u C%u S%u H%u"),
                 ui_seed_count(GAME_CROP_WHEAT), ui_seed_count(GAME_CROP_CARROT),
                 ui_seed_count(GAME_CROP_STRAWBERRY), ui_seed_count(GAME_CROP_HERB));
        label_one_line(card, line, 9, 84, 196,
                       ui_font_14(), COLOR_INK);
        game_recipe_t recipe = (game_recipe_t)s_item_selection;
        const game_recipe_definition_t *dish = game_recipe_definition(recipe);
        uint16_t stock = recipe == GAME_RECIPE_HOT_BREAD
            ? s_game.inventory_hot_bread : s_game.inventory_dishes[recipe];
        uint16_t premium = recipe == GAME_RECIPE_HOT_BREAD
            ? s_game.inventory_premium_hot_bread
            : s_game.inventory_premium_dishes[recipe];
        snprintf(line, sizeof(line), tr("> %s x%u +%u优 / %u金", "> %s x%u +%uQ / %uG"),
                 app_i18n_recipe(ui_language(), recipe), stock, premium, dish->sell_price);
        label_one_line(card, line, 9, 126, 196,
                       ui_font_14(), COLOR_RED);
        label(card, tr("上下选料理  OK出售", "UP/DOWN DISH  OK: SELL"), 9, 157,
              ui_font_14(), COLOR_MUTED);
    } else if (s_backpack_selection == 1) {
        game_pet_id_t pet = (game_pet_id_t)s_partner_selection;
        const game_pet_definition_t *definition = game_pet_definition(pet);
        label(card, app_i18n_pet(ui_language(), pet), 9, 10, ui_font_20(), COLOR_RED);
        snprintf(line, sizeof(line), "%s / %s",
                 app_i18n_pet_species(ui_language(), pet),
                 app_i18n_pet_personality(ui_language(), pet));
        label(card, line, 9, 43, ui_font_14(), COLOR_INK);
        snprintf(line, sizeof(line), tr("体%u 敏%u 察%u 魅%u 专%u",
                                        "STA %u DEX %u PER %u CHA %u FOC %u"),
                 definition->stamina, definition->dexterity,
                 definition->perception, definition->charm, definition->focus);
        label(card, line, 9, 75, ui_font_14(), COLOR_INK);
        snprintf(line, sizeof(line), tr("亲密%u  今日互动%u", "AFFINITY %u  ACTIONS %u"),
                 s_game.player_affinity[pet], s_game.companion_actions);
        label(card, line, 9, 107, ui_font_14(), COLOR_INK);
        label(card, tr("上下选伙伴  OK交谈", "UP/DOWN PET  OK: TALK"), 9, 143,
              ui_font_14(), COLOR_RED);
    } else if (s_backpack_selection == 3) {
        snprintf(line, sizeof(line), tr("第一章  %u / 10", "FIRST CHAPTER  %u / 10"),
                 s_game.quest_stage > 10U ? 10U : s_game.quest_stage);
        label(card, line, 9, 12, ui_font_20(), COLOR_RED);
        label(card, s_game.chapter_complete ? tr("道路已重新点亮", "THE ROAD IS RELIT")
                                            : tr("下一目标进行中", "NEXT OBJECTIVE ACTIVE"),
              9, 55, ui_font_14(), COLOR_INK);
        snprintf(line, sizeof(line), tr("森林%u  收获%u  日志%u",
                                        "FOREST %u  HARVEST %u  JOURNALS %u"),
                 s_game.forest_runs, s_game.total_crops_harvested,
                 s_game.travel_journal_count);
        label(card, line, 9, 91, ui_font_14(), COLOR_INK);
        label(card, tr("近期事件", "RECENT EVENTS"), 9, 125, ui_font_14(), COLOR_RED);
        if (s_game.event_history_count > 0U) {
            uint8_t shown = s_game.event_history_count > 2U ? 2U
                : s_game.event_history_count;
            for (uint8_t i = 0U; i < shown; i++) {
                uint8_t history_index = (uint8_t)(s_game.event_history_count - 1U - i);
                uint8_t event_id = s_game.event_history[history_index];
                lv_obj_t *history = label(card, app_i18n_event(ui_language(), event_id),
                                          9, 148 + i * 19,
                                          ui_font_14(), COLOR_MUTED);
                lv_obj_set_width(history, 196);
                lv_label_set_long_mode(history, LV_LABEL_LONG_DOT);
            }
        }
    } else if (s_backpack_selection == 4) {
        snprintf(line, sizeof(line), tr("伙伴 4/4  配方 %u/5", "PETS 4/4  RECIPES %u/5"),
                 (unsigned)__builtin_popcount((unsigned)s_game.unlocked_recipes));
        label(card, line, 9, 12, ui_font_14(), COLOR_INK);
        snprintf(line, sizeof(line), tr("访客 6  事件 %u", "VISITORS 6  EVENTS %u"),
                 GAME_CONTENT_EVENT_COUNT);
        label(card, line, 9, 48, ui_font_14(), COLOR_INK);
        snprintf(line, sizeof(line), tr("旅行照片 %u/8", "TRAVEL PHOTOS %u/8"),
                 s_game.travel_journal_count > 8U ? 8U : s_game.travel_journal_count);
        label(card, line, 9, 84, ui_font_14(), COLOR_INK);
    } else if (s_backpack_selection == 5) {
        snprintf(line, sizeof(line), tr("%c 声音：%s", "%c SOUND: %s"),
                 s_partner_selection == 0 ? '>' : ' ',
                 s_game.sound_enabled ? tr("开", "ON") : tr("关", "OFF"));
        label(card, line, 9, 12, ui_font_14(), COLOR_INK);
        snprintf(line, sizeof(line), tr("%c 夜间静音：%s", "%c NIGHT MUTE: %s"),
                 s_partner_selection == 1 ? '>' : ' ',
                 s_game.night_mute_enabled ? tr("开", "ON") : tr("关", "OFF"));
        label(card, line, 9, 48, ui_font_14(), COLOR_INK);
        snprintf(line, sizeof(line), tr("%c 时钟：%s", "%c CLOCK: %s"),
                 s_partner_selection == 2 ? '>' : ' ',
                 s_game.clock_24_hour ? tr("24小时", "24 HOUR")
                                      : tr("12小时", "12 HOUR"));
        label(card, line, 9, 84, ui_font_14(), COLOR_INK);
        snprintf(line, sizeof(line), tr("%c 语言：%s", "%c LANGUAGE: %s"),
                 s_partner_selection == 3 ? '>' : ' ',
                 s_game.language_english ? "ENGLISH" : "简体中文");
        label(card, line, 9, 120, ui_font_14(), COLOR_INK);
        label(card, tr("存档：A/B CRC V15", "SAVE: A/B CRC V15"),
              9, 154, ui_font_14(), COLOR_MUTED);
    }
    label(screen, tr("长按OK：返回", "HOLD OK: BACK"), 10, 276, ui_font_14(), COLOR_INK);
    activate_screen();
}

static void build_event(void)
{
    lv_obj_t *screen = new_screen(COLOR_NIGHT);
    uint8_t event_id = s_game.event_queue[0].id;
    label(screen, tr("事件", "EVENT"), 10, 10, ui_font_20(), COLOR_PAPER);
    lv_obj_t *card = rect(screen, 9, 52, 222, 208, COLOR_PAPER);
    lv_obj_set_style_border_width(card, 3, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(COLOR_WOOD_DARK), 0);
    lv_obj_t *title = label(card, app_i18n_event(ui_language(), event_id), 9, 12,
                            ui_font_14(), COLOR_RED);
    lv_obj_set_width(title, 198);
    lv_label_set_long_mode(title, LV_LABEL_LONG_WRAP);
    label(card, tr("旅人正在等待你的回答。", "A traveler waits for your answer."), 9, 58,
          ui_font_14(), COLOR_INK);
    lv_obj_t *choice_a = rect(card, 8, 96, 200, 38,
                              s_event_choice == 0 ? COLOR_GOLD : COLOR_PAPER);
    lv_obj_t *choice_b = rect(card, 8, 143, 200, 38,
                              s_event_choice == 1 ? COLOR_GOLD : COLOR_PAPER);
    lv_obj_set_style_border_width(choice_a, 2, 0);
    lv_obj_set_style_border_width(choice_b, 2, 0);
    lv_obj_set_style_border_color(choice_a, lv_color_hex(COLOR_INK), 0);
    lv_obj_set_style_border_color(choice_b, lv_color_hex(COLOR_INK), 0);
    label(choice_a, tr("实际帮助 / 奖励", "PRACTICAL HELP / REWARD"), 7, 9,
          ui_font_14(), COLOR_INK);
    label(choice_b, tr("倾听 / 关系", "LISTEN / RELATIONSHIP"), 7, 9,
          ui_font_14(), COLOR_INK);
    label(screen, tr("上下选择  OK确认", "UP/DOWN CHOOSE  OK CONFIRM"), 10, 278,
          ui_font_14(), COLOR_PAPER);
    activate_screen();
}

static void build_notice(void)
{
    lv_obj_t *screen = new_screen(COLOR_NIGHT);
    label(screen, tr("任务结果", "TASK RESULTS"), 10, 10, ui_font_20(), COLOR_PAPER);
    lv_obj_t *card = rect(screen, 9, 52, 222, 202, COLOR_PAPER);
    lv_obj_set_style_border_width(card, 3, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(COLOR_WOOD_DARK), 0);
    int y = 12;
    if (s_game.notifications & GAME_NOTICE_TRAVEL) {
        label(card, tr("旅行队已经归来", "TRAVEL TEAM RETURNED"), 9, y, ui_font_14(), COLOR_RED);
        y += 29;
    }
    if (s_game.notifications & GAME_NOTICE_BUILDING) {
        label(card, tr("建筑已经完成", "BUILDING COMPLETED"), 9, y, ui_font_14(), COLOR_RED);
        y += 29;
    }
    if (s_game.notifications & GAME_NOTICE_RESEARCH) {
        label(card, tr("配方研究有进展", "RECIPE RESEARCH UPDATED"), 9, y, ui_font_14(), COLOR_RED);
        y += 29;
    }
    if (s_game.notifications & GAME_NOTICE_PREMIUM_DISH) {
        label(card, tr("优质料理已完成", "QUALITY DISH READY"), 9, y, ui_font_14(), COLOR_RED);
        y += 29;
    }
    if (s_game.notifications & GAME_NOTICE_FOREST) {
        label(card, tr("长途探索已完成", "LONG EXPEDITION COMPLETE"), 9, y, ui_font_14(), COLOR_RED);
    }
    label(screen, tr("OK确认  长按返回", "OK: ACKNOWLEDGE  HOLD: BACK"), 10, 276,
          ui_font_14(), COLOR_PAPER);
    activate_screen();
}

static void stop_heat_timer(void)
{
    if (s_heat_timer) {
        lv_timer_delete(s_heat_timer);
        s_heat_timer = NULL;
    }
    s_heat_marker = NULL;
}

static void finish_heat_game(uint8_t accuracy)
{
    stop_heat_timer();
    uint32_t now = s_game.last_settled_time;
    clock_service_now(&now);
    s_now = now;
    game_state_t candidate = s_game;
    if (now > candidate.last_settled_time) {
        game_reduce(&candidate, (game_action_t){
            .type = GAME_ACTION_SETTLE_TO_TIME, .now = now,
        });
    }
    if (game_reduce(&candidate, (game_action_t){
            .type = GAME_ACTION_FINISH_HEAT_GAME,
            .now = now,
            .option = accuracy,
        }) && app_persistence_store(&candidate)) {
        s_game = candidate;
    }
    s_page = PAGE_KITCHEN;
    build_kitchen();
}

static void heat_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (s_page != PAGE_COOK_ASSIST || !s_heat_marker) {
        stop_heat_timer();
        return;
    }
    uint32_t elapsed = lv_tick_get() - s_heat_started_at;
    uint32_t phase = (elapsed / 12U) % 332U;
    uint32_t position = phase <= 166U ? phase : 332U - phase;
    lv_obj_set_x(s_heat_marker, 18 + (int32_t)position);
    uint32_t distance = position > 83U ? position - 83U : 83U - position;
    s_heat_accuracy = distance >= 83U
        ? 0U : (uint8_t)(100U - distance * 100U / 83U);
    if (elapsed >= 25U * 1000U) finish_heat_game(s_heat_accuracy);
}

static void build_cook_assist(void)
{
    lv_obj_t *screen = new_screen(COLOR_NIGHT);
    label(screen, tr("火候控制", "FIRE CONTROL"), 10, 10, ui_font_20(), COLOR_PAPER);
    lv_obj_t *card = rect(screen, 15, 65, 210, 164, COLOR_PAPER);
    lv_obj_set_style_border_width(card, 3, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(COLOR_RED), 0);
    label(card, tr("保持火焰稳定", "KEEP THE FLAME STEADY"), 10, 15,
          ui_font_14(), COLOR_RED);
    rect(card, 18, 65, 174, 28, COLOR_WOOD_DARK);
    rect(card, 67, 68, 76, 22, COLOR_GOLD);
    s_heat_marker = rect(card, 18, 61, 8, 36, COLOR_RED);
    label(card, tr("指针进入金色区时按OK", "OK IN THE GOLD ZONE"), 18, 112,
          ui_font_14(), COLOR_INK);
    label(screen, tr("25秒 / 尽量停在中央", "25 SEC / STOP NEAR CENTER"), 23, 248,
          ui_font_14(), COLOR_GOLD);
    label(screen, tr("OK停止  长按OK返回", "OK STOP  HOLD OK BACK"), 10, 278,
          ui_font_14(), COLOR_PAPER);
    activate_screen();
    s_heat_started_at = lv_tick_get();
    s_heat_accuracy = 0U;
    stop_heat_timer();
    s_heat_marker = lv_obj_get_child(card, -1);
    s_heat_timer = lv_timer_create(heat_timer_cb, 50U, NULL);
}

static void cancel_active_task(uint8_t target)
{
    uint32_t now = s_game.last_settled_time;
    clock_service_now(&now);
    s_now = now;
    game_state_t candidate = s_game;
    if (now > candidate.last_settled_time) {
        game_reduce(&candidate, (game_action_t){
            .type = GAME_ACTION_SETTLE_TO_TIME, .now = now,
        });
    }
    if (game_reduce(&candidate, (game_action_t){
            .type = GAME_ACTION_CANCEL_TASK,
            .now = now,
            .target = target,
        }) && app_persistence_store(&candidate)) {
        s_game = candidate;
    } else if (candidate.commit_sequence != s_game.commit_sequence &&
               app_persistence_store(&candidate)) {
        s_game = candidate;
    }
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
    s_partner_selection = 0;
    s_event_choice = 0;
    s_item_selection = 0;
    s_travel_goal = GAME_TRAVEL_MATERIALS;
    s_forest_duration = 0;
    s_last_input_tick = lv_tick_get();
    s_menu_hidden = false;
    if (!s_menu_timer) s_menu_timer = lv_timer_create(menu_timer_cb, 250U, NULL);
    build_station();
}

void app_ui_handle_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    if (ev != BSP_BTN_CLICK && ev != BSP_BTN_LONG) return;
    s_last_input_tick = lv_tick_get();
    if (s_page == PAGE_STATION && s_menu_hidden) {
        s_menu_hidden = false;
        for (size_t i = 0; i < 5U; i++) {
            if (s_bottom_tabs[i]) lv_obj_remove_flag(s_bottom_tabs[i], LV_OBJ_FLAG_HIDDEN);
        }
        if (ev == BSP_BTN_CLICK && (btn == BSP_BTN_UP || btn == BSP_BTN_DOWN)) {
            return;
        }
    }

    if (s_page == PAGE_EVENT) {
        if (ev == BSP_BTN_CLICK && (btn == BSP_BTN_UP || btn == BSP_BTN_DOWN)) {
            s_event_choice = 1 - s_event_choice;
            build_event();
        } else if (ev == BSP_BTN_CLICK && btn == BSP_BTN_OK) {
            game_state_t candidate = s_game;
            if (game_reduce(&candidate, (game_action_t){
                    .type = GAME_ACTION_RESOLVE_CONTENT_EVENT,
                    .target = (uint8_t)s_event_choice,
                }) && app_persistence_store(&candidate)) {
                s_game = candidate;
                s_page = PAGE_SCHEDULE;
                build_schedule();
            }
        } else if (ev == BSP_BTN_LONG && btn == BSP_BTN_OK) {
            s_page = PAGE_SCHEDULE;
            build_schedule();
        }
        return;
    }

    if (s_page == PAGE_NOTICE) {
        if (btn == BSP_BTN_OK && ev == BSP_BTN_LONG) {
            s_page = PAGE_SCHEDULE;
            build_schedule();
        } else if (btn == BSP_BTN_OK && ev == BSP_BTN_CLICK) {
            game_state_t candidate = s_game;
            if (game_reduce(&candidate, (game_action_t){
                    .type = GAME_ACTION_CLEAR_NOTIFICATIONS,
                }) && app_persistence_store(&candidate)) {
                s_game = candidate;
            }
            s_page = PAGE_SCHEDULE;
            build_schedule();
        }
        return;
    }

    if (s_page == PAGE_COOK_ASSIST) {
        if (btn == BSP_BTN_OK && ev == BSP_BTN_LONG) {
            stop_heat_timer();
            s_page = PAGE_KITCHEN;
            build_kitchen();
        } else if (btn == BSP_BTN_OK && ev == BSP_BTN_CLICK) {
            finish_heat_game(s_heat_accuracy);
        }
        return;
    }

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
            bool unlocked = (candidate.unlocked_recipes &
                             (1U << s_recipe_selection)) != 0U;
            if (game_reduce(&candidate, (game_action_t){
                    .type = unlocked ? GAME_ACTION_START_RECIPE
                                     : GAME_ACTION_START_RESEARCH,
                    .now = now,
                    .target = (uint8_t)s_recipe_selection,
                }) && app_persistence_store(&candidate)) {
                s_game = candidate;
            }
            build_kitchen();
        } else if (ev == BSP_BTN_CLICK && btn == BSP_BTN_OK &&
                   s_game.kitchen.active &&
                   s_game.kitchen.kind == GAME_TASK_HOT_BREAD &&
                   s_game.kitchen.option == 0U) {
            s_page = PAGE_COOK_ASSIST;
            build_cook_assist();
        } else if (ev == BSP_BTN_CLICK && btn == BSP_BTN_OK &&
                   s_game.kitchen.active) {
            cancel_active_task(GAME_CANCEL_KITCHEN);
            build_kitchen();
        }
        return;
    }

    if (s_page == PAGE_FOREST) {
        if (btn == BSP_BTN_OK && ev == BSP_BTN_LONG) {
            s_page = PAGE_SCHEDULE;
            build_schedule();
        } else if (ev == BSP_BTN_CLICK &&
                   (btn == BSP_BTN_UP || btn == BSP_BTN_DOWN) &&
                   !s_game.forest.active) {
            s_forest_duration = 1 - s_forest_duration;
            build_forest();
        } else if (ev == BSP_BTN_CLICK && btn == BSP_BTN_OK &&
                   !s_game.forest.active) {
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
                    .type = s_forest_duration == 0
                        ? GAME_ACTION_START_AMAI_FOREST
                        : GAME_ACTION_START_FOREST_2H,
                    .now = now,
                }) && app_persistence_store(&candidate)) {
                s_game = candidate;
            }
            build_forest();
        } else if (ev == BSP_BTN_CLICK && btn == BSP_BTN_OK &&
                   s_game.forest.active) {
            cancel_active_task(GAME_CANCEL_FOREST);
            build_forest();
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
        } else if (ev == BSP_BTN_CLICK && btn == BSP_BTN_OK &&
                   s_game.construction.active &&
                   s_game.construction.building ==
                       (game_building_t)s_building_selection) {
            cancel_active_task(GAME_CANCEL_CONSTRUCTION);
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

    if (s_page == PAGE_BACKPACK_DETAIL) {
        if (btn == BSP_BTN_OK && ev == BSP_BTN_LONG) {
            s_page = PAGE_BACKPACK;
            build_backpack();
        } else if (s_backpack_selection == 0 && ev == BSP_BTN_CLICK &&
                   (btn == BSP_BTN_UP || btn == BSP_BTN_DOWN)) {
            int delta = btn == BSP_BTN_UP ? -1 : 1;
            s_item_selection = (s_item_selection + delta +
                                GAME_RECIPE_COUNT) % GAME_RECIPE_COUNT;
            build_backpack_detail();
        } else if (s_backpack_selection == 0 && ev == BSP_BTN_CLICK &&
                   btn == BSP_BTN_OK) {
            game_state_t candidate = s_game;
            if (game_reduce(&candidate, (game_action_t){
                    .type = GAME_ACTION_SELL_DISH,
                    .target = (uint8_t)s_item_selection,
                }) && app_persistence_store(&candidate)) {
                s_game = candidate;
            }
            build_backpack_detail();
        } else if (s_backpack_selection == 1 && ev == BSP_BTN_CLICK &&
                   (btn == BSP_BTN_UP || btn == BSP_BTN_DOWN)) {
            int delta = btn == BSP_BTN_UP ? -1 : 1;
            s_partner_selection = (s_partner_selection + delta +
                                   GAME_PET_COUNT) % GAME_PET_COUNT;
            build_backpack_detail();
        } else if (s_backpack_selection == 1 && ev == BSP_BTN_CLICK &&
                   btn == BSP_BTN_OK) {
            game_state_t candidate = s_game;
            if (game_reduce(&candidate, (game_action_t){
                    .type = GAME_ACTION_TALK_TO_PET,
                    .target = (uint8_t)s_partner_selection,
                }) && app_persistence_store(&candidate)) {
                s_game = candidate;
            }
            build_backpack_detail();
        } else if (s_backpack_selection == 5 && ev == BSP_BTN_CLICK &&
                   (btn == BSP_BTN_UP || btn == BSP_BTN_DOWN)) {
            int delta = btn == BSP_BTN_UP ? -1 : 1;
            s_partner_selection = (s_partner_selection + delta + 4) % 4;
            build_backpack_detail();
        } else if (s_backpack_selection == 5 && ev == BSP_BTN_CLICK &&
                   btn == BSP_BTN_OK) {
            game_state_t candidate = s_game;
            if (game_reduce(&candidate, (game_action_t){
                    .type = GAME_ACTION_TOGGLE_SETTING,
                    .target = (uint8_t)s_partner_selection,
                }) && app_persistence_store(&candidate)) {
                s_game = candidate;
            }
            build_backpack_detail();
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
        } else if (s_page == PAGE_BACKPACK && ev == BSP_BTN_CLICK &&
                   btn == BSP_BTN_OK) {
            s_page = PAGE_BACKPACK_DETAIL;
            s_partner_selection = 0;
            build_backpack_detail();
        } else if (s_page == PAGE_TRAVEL && ev == BSP_BTN_CLICK &&
                   (btn == BSP_BTN_UP || btn == BSP_BTN_DOWN) &&
                   !s_game.travel.active) {
            int delta = btn == BSP_BTN_UP ? -1 : 1;
            s_travel_goal = (s_travel_goal + delta +
                             GAME_TRAVEL_GOAL_COUNT) % GAME_TRAVEL_GOAL_COUNT;
            build_travel();
        } else if (s_page == PAGE_TRAVEL && ev == BSP_BTN_CLICK &&
                   btn == BSP_BTN_OK && s_game.travel.active) {
            cancel_active_task(GAME_CANCEL_TRAVEL);
            build_travel();
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
                    .option = (uint8_t)s_travel_goal,
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
            int count = game_available_farm_plots(&s_game);
            int delta = btn == BSP_BTN_UP ? -1 : 1;
            s_farm_selection = (s_farm_selection + delta + count) % count;
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
            int delta = btn == BSP_BTN_UP ? -1 : 1;
            s_schedule_selection = (s_schedule_selection + delta + 4) % 4;
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
                   s_schedule_selection == 0 && s_game.event_queue_count > 0U) {
            s_page = PAGE_EVENT;
            s_event_choice = 0;
            build_event();
        } else if (ev == BSP_BTN_CLICK && btn == BSP_BTN_OK &&
                   s_schedule_selection == 0 && s_game.notifications != 0U) {
            s_page = PAGE_NOTICE;
            build_notice();
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
                   s_schedule_selection == 2) {
            s_page = PAGE_FOREST;
            s_forest_duration = 0;
            build_forest();
        }
        return;
    }

    if (ev != BSP_BTN_CLICK) return;
    if (btn == BSP_BTN_UP || btn == BSP_BTN_DOWN) {
        int delta = btn == BSP_BTN_UP ? -1 : 1;
        s_top_selection = (s_top_selection + delta + 5) % 5;
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
