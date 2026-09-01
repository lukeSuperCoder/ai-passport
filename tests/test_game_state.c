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

int main(void)
{
    test_six_hour_reception_loop();
    test_time_anomaly_and_eight_hour_income_cap();
    return 0;
}

