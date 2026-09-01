#include <assert.h>
#include <stdint.h>

#include "game/game_state.h"

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
    assert(state.pending.coins == 60U);
    assert(state.momo.stamina == 76U);

    assert(!game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_SETTLE_TO_TIME,
        .now = 1000U + 6U * 3600U,
    }));
    assert(state.pending.coins == 60U);

    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_CLAIM_REPORT,
    }));
    assert(state.coins == 60U);
    assert(!state.pending.available);
    assert(!game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_CLAIM_REPORT,
    }));
    assert(state.coins == 60U);
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
    assert(state.pending.coins == 80U);
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
    assert(state.inventory_wheat == 4U);
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

int main(void)
{
    test_six_hour_reception_loop();
    test_time_anomaly_and_eight_hour_income_cap();
    test_forest_task_completes_once();
    test_hot_bread_consumes_inputs_and_completes();
    test_parallel_tasks_require_settlement_boundary();
    return 0;
}
