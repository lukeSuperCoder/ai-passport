#pragma once

#include <stdbool.h>
#include <stdint.h>

#define GAME_OFFLINE_CAP_SECONDS (7U * 24U * 60U * 60U)
#define GAME_RECEPTION_CAP_SECONDS (8U * 60U * 60U)
#define GAME_FARM_INITIAL_PLOT_COUNT 4U
#define GAME_FARM_PLOT_COUNT 6U
#define GAME_SPRING_DAY_COUNT 14U
#define GAME_EVENT_MARKET 0x01U
#define GAME_EVENT_FESTIVAL 0x02U
#define GAME_PET_COUNT 4U
#define GAME_RELATION_COUNT 6U
#define GAME_EVENT_QUEUE_SIZE 3U
#define GAME_EVENT_HISTORY_SIZE 10U
#define GAME_VISITOR_COUNT 6U
#define GAME_CONTENT_EVENT_COUNT 65U
#define GAME_NOTICE_TRAVEL       0x01U
#define GAME_NOTICE_BUILDING     0x02U
#define GAME_NOTICE_RESEARCH     0x04U
#define GAME_NOTICE_PREMIUM_DISH 0x08U
#define GAME_NOTICE_FOREST       0x10U

typedef enum {
    GAME_JOB_REST = 0,
    GAME_JOB_RECEPTION,
    GAME_JOB_FOREST,
    GAME_JOB_KITCHEN,
    GAME_JOB_FARM,
} game_job_t;

typedef enum {
    GAME_TASK_NONE = 0,
    GAME_TASK_FOREST_30M,
    GAME_TASK_HOT_BREAD,
    GAME_TASK_TRAVEL_8H,
    GAME_TASK_BUILDING,
    GAME_TASK_FOREST_2H,
    GAME_TASK_RECIPE_RESEARCH,
} game_task_kind_t;

typedef enum {
    GAME_ACTOR_NONE = 0,
    GAME_ACTOR_AMAI,
    GAME_ACTOR_ATUAN,
} game_actor_id_t;

typedef enum {
    GAME_PET_MOMO = 0,
    GAME_PET_LULU,
    GAME_PET_AMAI,
    GAME_PET_ATUAN,
} game_pet_id_t;

typedef enum {
    GAME_CROP_NONE = 0,
    GAME_CROP_WHEAT,
    GAME_CROP_CARROT,
    GAME_CROP_STRAWBERRY,
    GAME_CROP_HERB,
    GAME_CROP_COUNT,
} game_crop_t;

typedef enum {
    GAME_RECIPE_HOT_BREAD = 0,
    GAME_RECIPE_CARROT_STEW,
    GAME_RECIPE_STRAWBERRY_JAM,
    GAME_RECIPE_HERB_TEA,
    GAME_RECIPE_FOREST_CAKE,
    GAME_RECIPE_COUNT,
} game_recipe_t;

typedef enum {
    GAME_BUILD_FRONT_DESK = 0,
    GAME_BUILD_KITCHEN,
    GAME_BUILD_FARM,
    GAME_BUILD_GUEST_ROOM,
    GAME_BUILD_SINK,
    GAME_BUILD_SIGNPOST,
    GAME_BUILD_COUNT,
} game_building_t;

typedef enum {
    GAME_TRAVEL_MATERIALS = 0,
    GAME_TRAVEL_OLD_ROAD,
    GAME_TRAVEL_SCENERY,
    GAME_TRAVEL_GOAL_COUNT,
} game_travel_goal_t;

typedef enum {
    GAME_WEATHER_CLEAR = 0,
    GAME_WEATHER_CLOUDY,
    GAME_WEATHER_RAIN,
    GAME_WEATHER_STORM,
} game_weather_t;

typedef struct {
    game_job_t job;
    uint8_t stamina;
    uint8_t mood;
    uint32_t job_started_at;
} game_pet_state_t;

typedef struct {
    bool active;
    game_task_kind_t kind;
    game_actor_id_t actor;
    uint32_t task_id;
    uint32_t started_at;
    uint32_t ends_at;
    game_recipe_t recipe;
    game_building_t building;
    uint8_t option;
} game_timed_task_t;

typedef struct {
    bool active;
    game_crop_t crop;
    uint32_t planted_at;
    uint32_t matures_at;
} game_farm_plot_t;

typedef struct {
    uint32_t coins;
    uint16_t wood;
    uint16_t berries;
    uint16_t hot_bread;
    uint16_t wheat;
    uint16_t mushrooms;
    uint32_t elapsed_seconds;
    bool available;
} game_pending_report_t;

typedef struct {
    uint8_t id;
    uint8_t queued_day;
} game_queued_event_t;

typedef struct {
    int16_t score;
    uint8_t yield_percent;
    uint8_t premium_chance;
} game_job_score_t;

typedef struct {
    uint32_t coins;
    uint32_t last_trusted_time;
    uint32_t last_settled_time;
    uint32_t commit_sequence;
    uint32_t season_started_at;
    uint32_t weather_seed;
    uint8_t spring_day;
    game_weather_t weather;
    uint8_t calendar_milestones;
    uint8_t pending_events;
    uint8_t completed_events;
    game_pet_state_t momo;
    game_pet_state_t amai;
    game_pet_state_t atuan;
    game_pet_state_t lulu;
    game_timed_task_t forest;
    game_timed_task_t kitchen;
    game_timed_task_t travel;
    uint16_t inventory_wheat;
    uint16_t inventory_wood;
    uint16_t inventory_berries;
    uint16_t inventory_hot_bread;
    uint16_t inventory_wheat_seed;
    uint16_t inventory_mushrooms;
    uint8_t travel_journal_count;
    uint16_t inventory_crops[GAME_CROP_COUNT];
    uint16_t inventory_seeds[GAME_CROP_COUNT];
    uint16_t inventory_dishes[GAME_RECIPE_COUNT];
    uint16_t pending_crops[GAME_CROP_COUNT];
    uint16_t pending_dishes[GAME_RECIPE_COUNT];
    uint16_t inventory_premium_dishes[GAME_RECIPE_COUNT];
    uint16_t pending_premium_dishes[GAME_RECIPE_COUNT];
    uint16_t inventory_premium_hot_bread;
    uint16_t pending_premium_hot_bread;
    uint8_t recipe_research[GAME_RECIPE_COUNT];
    uint8_t unlocked_recipes;
    uint8_t quest_stage;
    uint8_t completed_buildings;
    uint8_t reputation;
    uint8_t forest_runs;
    uint8_t road_fragments;
    uint16_t total_crops_harvested;
    uint16_t cooked_counts[GAME_RECIPE_COUNT];
    bool chapter_complete;
    game_timed_task_t construction;
    uint8_t relationships[GAME_RELATION_COUNT];
    uint8_t player_affinity[GAME_PET_COUNT];
    uint16_t job_experience[GAME_PET_COUNT][5];
    uint8_t companion_actions;
    uint8_t companion_actions_day;
    bool sound_enabled;
    bool night_mute_enabled;
    bool clock_24_hour;
    game_queued_event_t event_queue[GAME_EVENT_QUEUE_SIZE];
    uint8_t event_queue_count;
    uint8_t event_last_day[GAME_CONTENT_EVENT_COUNT];
    uint8_t event_seen[(GAME_CONTENT_EVENT_COUNT + 7U) / 8U];
    uint8_t event_history[GAME_EVENT_HISTORY_SIZE];
    uint8_t event_history_count;
    uint8_t visitor_stages[GAME_VISITOR_COUNT];
    uint8_t last_forest_result;
    game_travel_goal_t last_travel_goal;
    uint8_t notifications;
    game_farm_plot_t farm[GAME_FARM_PLOT_COUNT];
    game_pending_report_t pending;
    bool time_anomaly;
} game_state_t;

typedef enum {
    GAME_ACTION_ASSIGN_MOMO_RECEPTION = 0,
    GAME_ACTION_SETTLE_TO_TIME,
    GAME_ACTION_CLAIM_REPORT,
    GAME_ACTION_START_AMAI_FOREST,
    GAME_ACTION_START_ATUAN_HOT_BREAD,
    GAME_ACTION_PLANT_WHEAT,
    GAME_ACTION_RESOLVE_EVENT,
    GAME_ACTION_START_TRAVEL,
    GAME_ACTION_START_RECIPE,
    GAME_ACTION_PLANT_CROP,
    GAME_ACTION_START_BUILDING,
    GAME_ACTION_TALK_TO_PET,
    GAME_ACTION_TOGGLE_SETTING,
    GAME_ACTION_RESOLVE_CONTENT_EVENT,
    GAME_ACTION_SELL_DISH,
    GAME_ACTION_ASSIST_KITCHEN,
    GAME_ACTION_START_FOREST_2H,
    GAME_ACTION_START_RESEARCH,
    GAME_ACTION_CLEAR_NOTIFICATIONS,
    GAME_ACTION_FINISH_HEAT_GAME,
} game_action_type_t;

typedef struct {
    game_action_type_t type;
    uint32_t now;
    uint8_t target;
    uint8_t option;
} game_action_t;

void game_state_init(game_state_t *state, uint32_t now);
bool game_reduce(game_state_t *state, game_action_t action);
const char *game_weather_name(game_weather_t weather);
uint8_t game_relationship(const game_state_t *state, game_pet_id_t left,
                          game_pet_id_t right);
game_job_score_t game_calculate_job_score(const game_state_t *state,
                                          game_pet_id_t pet, game_job_t job,
                                          game_pet_id_t partner);
uint8_t game_available_farm_plots(const game_state_t *state);
