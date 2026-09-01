#include <assert.h>
#include <stdint.h>

#include "game/game_state.h"

static uint32_t reception_income(uint32_t base, game_weather_t weather)
{
    if (weather == GAME_WEATHER_RAIN) return base * 90U / 100U;
    if (weather == GAME_WEATHER_STORM) return base * 150U / 100U;
    return base;
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
    assert(left.weather == right.weather);

    uint32_t day14 = 10000U + 13U * 24U * 60U * 60U;
    assert(game_reduce(&left, (game_action_t){
        .type = GAME_ACTION_SETTLE_TO_TIME,
        .now = day14,
    }));
    assert(left.spring_day == 14U);
    assert((left.calendar_milestones & 0x03U) == 0x03U);
    assert(game_reduce(&left, (game_action_t){
        .type = GAME_ACTION_SETTLE_TO_TIME,
        .now = day14 + 10U * 24U * 60U * 60U,
    }));
    assert(left.spring_day == 14U);
}

int main(void)
{
    test_six_hour_reception_loop();
    test_time_anomaly_and_eight_hour_income_cap();
    test_forest_task_completes_once();
    test_hot_bread_consumes_inputs_and_completes();
    test_parallel_tasks_require_settlement_boundary();
    test_wheat_farm_feeds_kitchen_inventory();
    test_spring_calendar_and_weather_are_deterministic();
    return 0;
}
