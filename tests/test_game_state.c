#include <assert.h>
#include <stdint.h>

#include "game/game_state.h"

static uint32_t reception_income(uint32_t base, game_weather_t weather)
{
    if (weather == GAME_WEATHER_RAIN) return base * 90U / 100U;
    if (weather == GAME_WEATHER_STORM) return base * 150U / 100U;
    return base;
}

static uint32_t hourly_income(game_weather_t weather)
{
    if (weather == GAME_WEATHER_RAIN) return 9U;
    if (weather == GAME_WEATHER_STORM) return 15U;
    return 10U;
}

static void assert_reception_duration(uint32_t seconds, uint32_t paid_hours)
{
    game_state_t state;
    game_state_init(&state, 1000U);
    game_weather_t initial_weather = state.weather;
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_ASSIGN_MOMO_RECEPTION, .now = 1000U,
    }));
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_SETTLE_TO_TIME, .now = 1000U + seconds,
    }));
    assert(state.pending.coins == paid_hours * hourly_income(initial_weather));
}

static void test_offline_acceptance_duration_matrix(void)
{
    assert_reception_duration(30U * 60U, 0U);
    assert_reception_duration(2U * 60U * 60U, 2U);
    assert_reception_duration(8U * 60U * 60U, 8U);
    assert_reception_duration(3U * 24U * 60U * 60U, 8U);

    game_state_t capped;
    game_state_init(&capped, 100U);
    assert(game_reduce(&capped, (game_action_t){
        .type = GAME_ACTION_ASSIGN_MOMO_RECEPTION, .now = 100U,
    }));
    assert(game_reduce(&capped, (game_action_t){
        .type = GAME_ACTION_SETTLE_TO_TIME,
        .now = 100U + 10U * 24U * 60U * 60U,
    }));
    assert(capped.pending.elapsed_seconds == GAME_OFFLINE_CAP_SECONDS);
    assert(capped.spring_day == 11U);
}

static void test_six_hour_reception_loop(void)
{
    game_state_t state;
    game_state_init(&state, 1000U);

    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_ASSIGN_MOMO_RECEPTION,
        .now = 1000U,
    }));
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_SETTLE_TO_TIME,
        .now = 1000U + 6U * 3600U,
    }));
    assert(state.pending.available);
    uint32_t expected = reception_income(60U, state.weather);
    assert(state.pending.coins == expected);
    assert(state.momo.stamina == 76U);

    assert(!game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_SETTLE_TO_TIME,
        .now = 1000U + 6U * 3600U,
    }));
    assert(state.pending.coins == expected);

    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_CLAIM_REPORT,
    }));
    assert(state.coins == expected);
    assert(!state.pending.available);
    assert(!game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_CLAIM_REPORT,
    }));
    assert(state.coins == expected);
}

static void test_time_anomaly_and_eight_hour_income_cap(void)
{
    game_state_t state;
    game_state_init(&state, 5000U);
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_ASSIGN_MOMO_RECEPTION,
        .now = 5000U,
    }));
    assert(!game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_SETTLE_TO_TIME,
        .now = 4999U,
    }));
    assert(state.time_anomaly);

    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_SETTLE_TO_TIME,
        .now = 5000U + 20U * 3600U,
    }));
    assert(state.pending.coins == reception_income(80U, state.weather));
    assert(state.momo.stamina == 68U);
}

static void test_forest_task_completes_once(void)
{
    game_state_t state;
    game_state_init(&state, 100U);
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_START_AMAI_FOREST,
        .now = 100U,
    }));
    assert(state.forest.active);
    assert(state.amai.job == GAME_JOB_FOREST);

    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_SETTLE_TO_TIME,
        .now = 100U + 29U * 60U,
    }));
    assert(state.forest.active);
    assert(!state.pending.available);

    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_SETTLE_TO_TIME,
        .now = 100U + 30U * 60U,
    }));
    assert(!state.forest.active);
    assert(state.amai.job == GAME_JOB_REST);
    assert(state.amai.stamina == 95U);
    assert(state.pending.wood == 3U);
    assert(state.pending.berries == 1U);

    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_CLAIM_REPORT,
    }));
    assert(state.inventory_wood == 3U);
    assert(state.inventory_berries == 1U);

    assert(!game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_SETTLE_TO_TIME,
        .now = 100U + 30U * 60U,
    }));
    assert(state.inventory_wood == 3U);
}

static void test_hot_bread_consumes_inputs_and_completes(void)
{
    game_state_t state;
    game_state_init(&state, 200U);
    state.inventory_wheat = 4U;
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_START_ATUAN_HOT_BREAD,
        .now = 200U,
    }));
    assert(state.inventory_wheat == 2U);
    assert(state.kitchen.kind == GAME_TASK_HOT_BREAD);
    assert(state.atuan.job == GAME_JOB_KITCHEN);
    assert(!game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_START_ATUAN_HOT_BREAD,
        .now = 200U,
    }));

    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_SETTLE_TO_TIME,
        .now = 200U + 10U * 60U,
    }));
    assert(!state.kitchen.active);
    assert(state.pending.hot_bread == 1U);
    assert(state.atuan.stamina == 96U);
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_CLAIM_REPORT,
    }));
    assert(state.inventory_hot_bread == 1U);
}

static void test_parallel_tasks_require_settlement_boundary(void)
{
    game_state_t state;
    game_state_init(&state, 1000U);
    state.inventory_wheat = 2U;
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_START_AMAI_FOREST,
        .now = 1000U,
    }));
    assert(!game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_START_ATUAN_HOT_BREAD,
        .now = 1600U,
    }));
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_SETTLE_TO_TIME,
        .now = 1600U,
    }));
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_START_ATUAN_HOT_BREAD,
        .now = 1600U,
    }));
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_SETTLE_TO_TIME,
        .now = 2800U,
    }));
    assert(!state.forest.active);
    assert(!state.kitchen.active);
    assert(state.pending.wood == 3U);
    assert(state.pending.hot_bread == 1U);
}

static void test_wheat_farm_feeds_kitchen_inventory(void)
{
    game_state_t state;
    game_state_init(&state, 500U);
    assert(state.inventory_wheat_seed == 4U);
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_PLANT_WHEAT,
        .now = 500U,
        .target = 0U,
    }));
    assert(state.inventory_wheat_seed == 3U);
    assert(state.farm[0].active);
    assert(!game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_PLANT_WHEAT,
        .now = 500U,
        .target = 0U,
    }));

    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_SETTLE_TO_TIME,
        .now = 500U + 24U * 60U * 60U - 1U,
    }));
    assert(state.farm[0].active);
    assert(state.pending.wheat == 0U);
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_SETTLE_TO_TIME,
        .now = 500U + 24U * 60U * 60U,
    }));
    assert(!state.farm[0].active);
    assert(state.pending.wheat == 2U);
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_CLAIM_REPORT,
    }));
    assert(state.inventory_wheat == 2U);
}

static void test_spring_calendar_and_weather_are_deterministic(void)
{
    game_state_t left;
    game_state_t right;
    game_state_init(&left, 10000U);
    game_state_init(&right, 10000U);

    uint32_t day7 = 10000U + 6U * 24U * 60U * 60U;
    assert(game_reduce(&left, (game_action_t){
        .type = GAME_ACTION_SETTLE_TO_TIME,
        .now = day7,
    }));
    assert(game_reduce(&right, (game_action_t){
        .type = GAME_ACTION_SETTLE_TO_TIME,
        .now = day7,
    }));
    assert(left.spring_day == 7U);
    assert((left.calendar_milestones & 0x01U) != 0U);
    assert((left.pending_events & GAME_EVENT_MARKET) != 0U);
    assert(left.weather == right.weather);

    uint32_t day14 = 10000U + 13U * 24U * 60U * 60U;
    assert(game_reduce(&left, (game_action_t){
        .type = GAME_ACTION_SETTLE_TO_TIME,
        .now = day14,
    }));
    assert(left.spring_day == 14U);
    assert((left.calendar_milestones & 0x03U) == 0x03U);
    assert((left.pending_events & GAME_EVENT_FESTIVAL) != 0U);
    assert(game_reduce(&left, (game_action_t){
        .type = GAME_ACTION_SETTLE_TO_TIME,
        .now = day14 + 10U * 24U * 60U * 60U,
    }));
    assert(left.spring_day == 14U);
}


static void test_reception_is_split_by_daily_weather(void)
{
    game_state_t state;
    game_state_init(&state, 0U);
    state.weather_seed = 12345U;
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_ASSIGN_MOMO_RECEPTION,
        .now = 23U * 3600U,
    }));
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_SETTLE_TO_TIME,
        .now = 27U * 3600U,
    }));

    game_state_t day1;
    game_state_t day2;
    game_state_init(&day1, 0U);
    game_state_init(&day2, 0U);
    day1.weather_seed = state.weather_seed;
    day2.weather_seed = state.weather_seed;
    assert(game_reduce(&day2, (game_action_t){
        .type = GAME_ACTION_SETTLE_TO_TIME,
        .now = 24U * 3600U,
    }));
    uint32_t expected = hourly_income(day1.weather) + 3U * hourly_income(day2.weather);
    assert(state.pending.coins == expected);
}

static void test_milestone_events_are_player_resolved_once(void)
{
    game_state_t state;
    game_state_init(&state, 100U);
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_SETTLE_TO_TIME,
        .now = 100U + 13U * 24U * 60U * 60U,
    }));
    assert(state.pending_events == (GAME_EVENT_MARKET | GAME_EVENT_FESTIVAL));
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_RESOLVE_EVENT,
        .target = GAME_EVENT_MARKET,
    }));
    assert((state.completed_events & GAME_EVENT_MARKET) != 0U);
    assert((state.pending_events & GAME_EVENT_MARKET) == 0U);
    assert(state.pending.coins == 20U);
    assert(!game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_RESOLVE_EVENT,
        .target = GAME_EVENT_MARKET,
    }));
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_RESOLVE_EVENT,
        .target = GAME_EVENT_FESTIVAL,
    }));
    assert(state.pending.coins == 70U);
    assert(state.inventory_wheat_seed == 6U);
}

static void test_travel_requires_unlock_and_completes_offline(void)
{
    game_state_t state;
    game_state_init(&state, 1000U);
    state.inventory_hot_bread = 1U;
    assert(!game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_START_TRAVEL,
        .now = 1000U,
    }));
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_SETTLE_TO_TIME,
        .now = 1000U + 7U * 24U * 60U * 60U,
    }));
    uint32_t departure = state.last_settled_time;
    assert(state.spring_day == 8U);
    state.completed_buildings |= (uint8_t)(1U << GAME_BUILD_SIGNPOST);
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_START_TRAVEL,
        .now = departure,
    }));
    assert(state.inventory_hot_bread == 0U);
    assert(state.travel.active);
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_SETTLE_TO_TIME,
        .now = departure + 8U * 60U * 60U,
    }));
    assert(!state.travel.active);
    assert(state.pending.wood == 5U);
    assert(state.pending.berries == 3U);
    assert(state.pending.mushrooms == 1U);
    assert(state.travel_journal_count == 1U);
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_CLAIM_REPORT,
    }));
    assert(state.inventory_mushrooms == 1U);
}

static void test_main_story_can_reach_spring_14_without_deadlock(void)
{
    game_state_t state;
    game_state_init(&state, 0U);
    uint32_t now = 0U;
    assert(state.quest_stage == 2U);
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_ASSIGN_MOMO_RECEPTION, .now = now,
    }));
    now += 8U * 60U * 60U;
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_SETTLE_TO_TIME, .now = now,
    }));
    assert(game_reduce(&state, (game_action_t){ .type = GAME_ACTION_CLAIM_REPORT }));

    state.inventory_wheat = 2U;
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_START_RECIPE, .now = now,
        .target = GAME_RECIPE_HOT_BREAD,
    }));
    now += 10U * 60U;
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_SETTLE_TO_TIME, .now = now,
    }));
    assert(state.quest_stage == 3U);

    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_PLANT_CROP, .now = now, .target = 0U,
        .option = GAME_CROP_WHEAT,
    }));
    now += 24U * 60U * 60U;
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_SETTLE_TO_TIME, .now = now,
    }));
    assert(state.quest_stage == 4U);

    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_START_AMAI_FOREST, .now = now,
    }));
    now += 30U * 60U;
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_SETTLE_TO_TIME, .now = now,
    }));
    assert(state.quest_stage == 5U);
    assert(game_reduce(&state, (game_action_t){ .type = GAME_ACTION_CLAIM_REPORT }));
    state.inventory_wood = 8U;
    assert(state.coins >= 150U);
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_START_BUILDING, .now = now,
        .target = GAME_BUILD_GUEST_ROOM,
    }));
    now += 2U * 60U * 60U;
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_SETTLE_TO_TIME, .now = now,
    }));
    assert(state.quest_stage == 6U);

    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_PLANT_CROP, .now = now, .target = 0U,
        .option = GAME_CROP_CARROT,
    }));
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_PLANT_CROP, .now = now, .target = 1U,
        .option = GAME_CROP_HERB,
    }));
    now += 36U * 60U * 60U;
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_SETTLE_TO_TIME, .now = now,
    }));
    assert(game_reduce(&state, (game_action_t){ .type = GAME_ACTION_CLAIM_REPORT }));
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_START_RECIPE, .now = now,
        .target = GAME_RECIPE_CARROT_STEW,
    }));
    now += 30U * 60U;
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_SETTLE_TO_TIME, .now = now,
    }));
    assert(state.quest_stage == 7U);
    state.inventory_crops[GAME_CROP_HERB] += 2U;
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_START_RECIPE, .now = now,
        .target = GAME_RECIPE_HERB_TEA,
    }));
    now += 10U * 60U;
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_SETTLE_TO_TIME, .now = now,
    }));
    assert(state.quest_stage == 8U);

    now = 6U * 24U * 60U * 60U;
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_SETTLE_TO_TIME, .now = now,
    }));
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_RESOLVE_EVENT, .target = GAME_EVENT_MARKET,
    }));
    assert(state.quest_stage == 9U);
    for (int i = state.forest_runs; i < 5; i++) {
        assert(game_reduce(&state, (game_action_t){
            .type = GAME_ACTION_START_AMAI_FOREST, .now = now,
        }));
        now += 30U * 60U;
        assert(game_reduce(&state, (game_action_t){
            .type = GAME_ACTION_SETTLE_TO_TIME, .now = now,
        }));
    }
    assert(game_reduce(&state, (game_action_t){ .type = GAME_ACTION_CLAIM_REPORT }));
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_START_BUILDING, .now = now,
        .target = GAME_BUILD_SIGNPOST,
    }));
    now += 4U * 60U * 60U;
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_SETTLE_TO_TIME, .now = now,
    }));
    assert(state.quest_stage == 10U);
    assert(state.inventory_hot_bread > 0U);
    now = 7U * 24U * 60U * 60U;
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_SETTLE_TO_TIME, .now = now,
    }));
    assert(state.spring_day == 8U);
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_START_TRAVEL, .now = now,
    }));
    now += 8U * 60U * 60U;
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_SETTLE_TO_TIME, .now = now,
    }));
    now = 13U * 24U * 60U * 60U;
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_SETTLE_TO_TIME, .now = now,
    }));
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_RESOLVE_EVENT, .target = GAME_EVENT_FESTIVAL,
    }));
    assert(state.quest_stage == 11U);
    assert(state.chapter_complete);
}

static void test_all_crops_and_recipes_form_production_chains(void)
{
    game_state_t state;
    game_state_init(&state, 100U);
    state.inventory_seeds[GAME_CROP_CARROT] = 1U;
    state.inventory_seeds[GAME_CROP_STRAWBERRY] = 1U;
    state.inventory_seeds[GAME_CROP_HERB] = 1U;
    for (uint8_t plot = 0U; plot < GAME_FARM_PLOT_COUNT; plot++) {
        game_crop_t crop = (game_crop_t)(plot + 1U);
        assert(game_reduce(&state, (game_action_t){
            .type = GAME_ACTION_PLANT_CROP,
            .now = 100U,
            .target = plot,
            .option = (uint8_t)crop,
        }));
    }
    uint32_t now = 100U + 48U * 60U * 60U;
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_SETTLE_TO_TIME, .now = now,
    }));
    assert(state.pending.wheat == 2U);
    assert(state.pending_crops[GAME_CROP_CARROT] == 1U);
    assert(state.pending_crops[GAME_CROP_STRAWBERRY] == 2U);
    assert(state.pending_crops[GAME_CROP_HERB] == 2U);
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_CLAIM_REPORT,
    }));
    assert(state.inventory_wheat == 2U);
    assert(state.inventory_crops[GAME_CROP_CARROT] == 1U);
    assert(state.inventory_crops[GAME_CROP_STRAWBERRY] == 2U);
    assert(state.inventory_crops[GAME_CROP_HERB] == 2U);

    state.unlocked_recipes = (uint8_t)((1U << GAME_RECIPE_COUNT) - 1U);
    state.inventory_wheat += 2U;
    state.inventory_crops[GAME_CROP_HERB] += 3U;
    state.inventory_berries = 2U;
    const uint32_t durations[GAME_RECIPE_COUNT] = {
        10U * 60U, 30U * 60U, 60U * 60U, 10U * 60U, 30U * 60U,
    };
    for (game_recipe_t recipe = GAME_RECIPE_HOT_BREAD;
         recipe < GAME_RECIPE_COUNT; recipe++) {
        assert(game_reduce(&state, (game_action_t){
            .type = GAME_ACTION_START_RECIPE,
            .now = now,
            .target = (uint8_t)recipe,
        }));
        now += durations[recipe];
        assert(game_reduce(&state, (game_action_t){
            .type = GAME_ACTION_SETTLE_TO_TIME, .now = now,
        }));
    }
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_CLAIM_REPORT,
    }));
    assert(state.inventory_hot_bread == 1U);
    for (game_recipe_t recipe = GAME_RECIPE_CARROT_STEW;
         recipe < GAME_RECIPE_COUNT; recipe++) {
        assert(state.inventory_dishes[recipe] == 1U);
    }
}

int main(void)
{
    test_offline_acceptance_duration_matrix();
    test_six_hour_reception_loop();
    test_time_anomaly_and_eight_hour_income_cap();
    test_forest_task_completes_once();
    test_hot_bread_consumes_inputs_and_completes();
    test_parallel_tasks_require_settlement_boundary();
    test_wheat_farm_feeds_kitchen_inventory();
    test_spring_calendar_and_weather_are_deterministic();
    test_reception_is_split_by_daily_weather();
    test_milestone_events_are_player_resolved_once();
    test_travel_requires_unlock_and_completes_offline();
    test_all_crops_and_recipes_form_production_chains();
    test_main_story_can_reach_spring_14_without_deadlock();
    return 0;
}
