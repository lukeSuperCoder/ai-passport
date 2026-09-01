#include "game_state.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

_Static_assert(sizeof(game_state_t) <= 12U * 1024U,
               "game_state_t exceeds the MVP resident RAM budget");

static uint32_t saturating_add_u32(uint32_t left, uint32_t right)
{
    return UINT32_MAX - left < right ? UINT32_MAX : left + right;
}

static uint16_t saturating_add_u16(uint16_t left, uint16_t right)
{
    return UINT16_MAX - left < right ? UINT16_MAX : (uint16_t)(left + right);
}

static void settle_reception(game_state_t *state, uint32_t elapsed)
{
    uint32_t productive = elapsed;
    if (productive > GAME_RECEPTION_CAP_SECONDS) {
        productive = GAME_RECEPTION_CAP_SECONDS;
    }

    uint32_t hours = productive / 3600U;
    if (hours == 0) return;

    state->pending.coins = saturating_add_u32(state->pending.coins, hours * 10U);
    state->pending.available = true;

    uint32_t stamina_cost = hours * 4U;
    state->momo.stamina = stamina_cost >= state->momo.stamina
        ? 0U : (uint8_t)(state->momo.stamina - stamina_cost);
    if (state->momo.stamina < 20U) {
        state->momo.job = GAME_JOB_REST;
    }
}

void game_state_init(game_state_t *state, uint32_t now)
{
    if (!state) return;
    memset(state, 0, sizeof(*state));
    state->last_trusted_time = now;
    state->last_settled_time = now;
    state->momo.job = GAME_JOB_REST;
    state->momo.stamina = 100U;
    state->momo.mood = 80U;
    state->amai.job = GAME_JOB_REST;
    state->amai.stamina = 100U;
    state->amai.mood = 90U;
}

bool game_reduce(game_state_t *state, game_action_t action)
{
    if (!state) return false;

    switch (action.type) {
    case GAME_ACTION_ASSIGN_MOMO_RECEPTION:
        if (state->momo.job == GAME_JOB_RECEPTION ||
            action.now < state->last_trusted_time) {
            return false;
        }
        state->momo.job = GAME_JOB_RECEPTION;
        state->momo.job_started_at = action.now;
        state->last_trusted_time = action.now;
        state->last_settled_time = action.now;
        state->commit_sequence++;
        return true;

    case GAME_ACTION_SETTLE_TO_TIME: {
        if (action.now < state->last_trusted_time) {
            state->time_anomaly = true;
            return false;
        }
        if (action.now <= state->last_settled_time) return false;

        uint32_t elapsed = action.now - state->last_settled_time;
        if (elapsed > GAME_OFFLINE_CAP_SECONDS) {
            elapsed = GAME_OFFLINE_CAP_SECONDS;
        }
        if (state->momo.job == GAME_JOB_RECEPTION) {
            settle_reception(state, elapsed);
        }
        if (state->forest.active && state->forest.ends_at <= action.now) {
            state->pending.wood = saturating_add_u16(state->pending.wood, 3U);
            state->pending.berries = saturating_add_u16(state->pending.berries, 1U);
            state->pending.available = true;
            state->forest.active = false;
            state->amai.job = GAME_JOB_REST;
            state->amai.stamina = state->amai.stamina > 5U
                ? (uint8_t)(state->amai.stamina - 5U) : 0U;
        }
        if (state->pending.available) {
            state->pending.elapsed_seconds = saturating_add_u32(
                state->pending.elapsed_seconds, elapsed);
        }
        state->last_settled_time = action.now;
        state->last_trusted_time = action.now;
        state->commit_sequence++;
        return true;

    case GAME_ACTION_START_AMAI_FOREST:
        if (state->forest.active || state->amai.job != GAME_JOB_REST ||
            action.now > UINT32_MAX - 30U * 60U ||
            action.now < state->last_trusted_time) {
            return false;
        }
        state->forest.active = true;
        state->forest.task_id = state->commit_sequence + 1U;
        state->forest.started_at = action.now;
        state->forest.ends_at = action.now + 30U * 60U;
        state->amai.job = GAME_JOB_FOREST;
        state->amai.job_started_at = action.now;
        state->last_trusted_time = action.now;
        state->last_settled_time = action.now;
        state->commit_sequence++;
        return true;
    }

    case GAME_ACTION_CLAIM_REPORT:
        if (!state->pending.available) return false;
        state->coins = saturating_add_u32(state->coins, state->pending.coins);
        memset(&state->pending, 0, sizeof(state->pending));
        state->commit_sequence++;
        return true;
    }

    return false;
}
