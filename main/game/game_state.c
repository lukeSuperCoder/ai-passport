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

static game_weather_t weather_for_day(uint32_t seed, uint8_t day)
{
    uint32_t value = seed ^ ((uint32_t)day * 0x9E3779B9U);
    value ^= value >> 16U;
    value *= 0x7FEB352DU;
    value ^= value >> 15U;
    uint32_t roll = value % 100U;
    if (roll < 45U) return GAME_WEATHER_CLEAR;
    if (roll < 70U) return GAME_WEATHER_CLOUDY;
    if (roll < 95U) return GAME_WEATHER_RAIN;
    return GAME_WEATHER_STORM;
}

static void update_calendar(game_state_t *state, uint32_t now)
{
    if (now < state->season_started_at) return;
    uint32_t elapsed_days = (now - state->season_started_at) / (24U * 60U * 60U);
    uint8_t day = elapsed_days >= GAME_SPRING_DAY_COUNT
        ? GAME_SPRING_DAY_COUNT : (uint8_t)(elapsed_days + 1U);
    state->spring_day = day;
    state->weather = weather_for_day(state->weather_seed, day);
    if (day >= 7U) state->calendar_milestones |= 0x01U;
    if (day >= 14U) state->calendar_milestones |= 0x02U;
}

const char *game_weather_name(game_weather_t weather)
{
    switch (weather) {
    case GAME_WEATHER_CLEAR: return "CLEAR";
    case GAME_WEATHER_CLOUDY: return "CLOUDY";
    case GAME_WEATHER_RAIN: return "RAIN";
    case GAME_WEATHER_STORM: return "STORM";
    }
    return "UNKNOWN";
}

static void complete_timed_task(game_state_t *state, game_timed_task_t *task)
{
    switch (task->kind) {
    case GAME_TASK_FOREST_30M:
        state->pending.wood = saturating_add_u16(state->pending.wood, 3U);
        state->pending.berries = saturating_add_u16(state->pending.berries, 1U);
        state->amai.job = GAME_JOB_REST;
        state->amai.stamina = state->amai.stamina > 5U
            ? (uint8_t)(state->amai.stamina - 5U) : 0U;
        break;
    case GAME_TASK_HOT_BREAD:
        state->pending.hot_bread = saturating_add_u16(state->pending.hot_bread, 1U);
        state->atuan.job = GAME_JOB_REST;
        state->atuan.stamina = state->atuan.stamina > 4U
            ? (uint8_t)(state->atuan.stamina - 4U) : 0U;
        break;
    case GAME_TASK_NONE:
        return;
    }
    state->pending.available = true;
    task->active = false;
    task->kind = GAME_TASK_NONE;
    task->actor = GAME_ACTOR_NONE;
}

static void settle_reception(game_state_t *state, uint32_t elapsed)
{
    uint32_t productive = elapsed;
    if (productive > GAME_RECEPTION_CAP_SECONDS) {
        productive = GAME_RECEPTION_CAP_SECONDS;
    }

    uint32_t hours = productive / 3600U;
    if (hours == 0) return;

    uint32_t income = hours * 10U;
    if (state->weather == GAME_WEATHER_RAIN) income = income * 90U / 100U;
    if (state->weather == GAME_WEATHER_STORM) income = income * 150U / 100U;
    state->pending.coins = saturating_add_u32(state->pending.coins, income);
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
    state->season_started_at = now;
    state->weather_seed = now ^ 0x54494D45U;
    state->spring_day = 1U;
    state->weather = weather_for_day(state->weather_seed, 1U);
    state->momo.job = GAME_JOB_REST;
    state->momo.stamina = 100U;
    state->momo.mood = 80U;
    state->amai.job = GAME_JOB_REST;
    state->amai.stamina = 100U;
    state->amai.mood = 90U;
    state->atuan.job = GAME_JOB_REST;
    state->atuan.stamina = 100U;
    state->atuan.mood = 70U;
    state->lulu.job = GAME_JOB_REST;
    state->lulu.stamina = 100U;
    state->lulu.mood = 90U;
    state->inventory_wheat_seed = 4U;
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
        update_calendar(state, action.now);
        if (state->momo.job == GAME_JOB_RECEPTION) {
            settle_reception(state, elapsed);
        }
        if (state->forest.active && state->forest.ends_at <= action.now) {
            complete_timed_task(state, &state->forest);
        }
        if (state->kitchen.active && state->kitchen.ends_at <= action.now) {
            complete_timed_task(state, &state->kitchen);
        }
        for (size_t i = 0; i < GAME_FARM_PLOT_COUNT; i++) {
            game_farm_plot_t *plot = &state->farm[i];
            if (plot->active && plot->matures_at <= action.now) {
                if (plot->crop == GAME_CROP_WHEAT) {
                    state->pending.wheat = saturating_add_u16(
                        state->pending.wheat, 2U);
                    state->pending.available = true;
                }
                memset(plot, 0, sizeof(*plot));
            }
        }
        bool farm_active = false;
        for (size_t i = 0; i < GAME_FARM_PLOT_COUNT; i++) {
            farm_active = farm_active || state->farm[i].active;
        }
        if (!farm_active && state->lulu.job == GAME_JOB_FARM) {
            state->lulu.job = GAME_JOB_REST;
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
            action.now != state->last_settled_time) {
            return false;
        }
        state->forest.active = true;
        state->forest.kind = GAME_TASK_FOREST_30M;
        state->forest.actor = GAME_ACTOR_AMAI;
        state->forest.task_id = state->commit_sequence + 1U;
        state->forest.started_at = action.now;
        state->forest.ends_at = action.now + 30U * 60U;
        state->amai.job = GAME_JOB_FOREST;
        state->amai.job_started_at = action.now;
        state->last_trusted_time = action.now;
        state->last_settled_time = action.now;
        state->commit_sequence++;
        return true;

    case GAME_ACTION_START_ATUAN_HOT_BREAD:
        if (state->kitchen.active || state->atuan.job != GAME_JOB_REST ||
            state->inventory_wheat < 2U ||
            action.now > UINT32_MAX - 10U * 60U ||
            action.now != state->last_settled_time) {
            return false;
        }
        state->inventory_wheat -= 2U;
        state->kitchen.active = true;
        state->kitchen.kind = GAME_TASK_HOT_BREAD;
        state->kitchen.actor = GAME_ACTOR_ATUAN;
        state->kitchen.task_id = state->commit_sequence + 1U;
        state->kitchen.started_at = action.now;
        state->kitchen.ends_at = action.now + 10U * 60U;
        state->atuan.job = GAME_JOB_KITCHEN;
        state->atuan.job_started_at = action.now;
        state->last_trusted_time = action.now;
        state->last_settled_time = action.now;
        state->commit_sequence++;
        return true;
    }

    case GAME_ACTION_CLAIM_REPORT:
        if (!state->pending.available) return false;
        state->coins = saturating_add_u32(state->coins, state->pending.coins);
        state->inventory_wood = saturating_add_u16(
            state->inventory_wood, state->pending.wood);
        state->inventory_berries = saturating_add_u16(
            state->inventory_berries, state->pending.berries);
        state->inventory_hot_bread = saturating_add_u16(
            state->inventory_hot_bread, state->pending.hot_bread);
        state->inventory_wheat = saturating_add_u16(
            state->inventory_wheat, state->pending.wheat);
        memset(&state->pending, 0, sizeof(state->pending));
        state->commit_sequence++;
        return true;

    case GAME_ACTION_PLANT_WHEAT:
        if (action.target >= GAME_FARM_PLOT_COUNT ||
            state->farm[action.target].active ||
            state->inventory_wheat_seed == 0U ||
            action.now > UINT32_MAX - 24U * 60U * 60U ||
            action.now != state->last_settled_time) {
            return false;
        }
        state->inventory_wheat_seed--;
        state->farm[action.target].active = true;
        state->farm[action.target].crop = GAME_CROP_WHEAT;
        state->farm[action.target].planted_at = action.now;
        state->farm[action.target].matures_at = action.now + 24U * 60U * 60U;
        state->lulu.job = GAME_JOB_FARM;
        state->lulu.job_started_at = action.now;
        state->commit_sequence++;
        return true;
    }

    return false;
}
