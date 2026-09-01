#include "game_state.h"
#include "game_content.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

_Static_assert(sizeof(game_state_t) <= 12U * 1024U,
               "game_state_t exceeds the MVP resident RAM budget");

static void refresh_quest_progress(game_state_t *state);

static uint32_t saturating_add_u32(uint32_t left, uint32_t right)
{
    return UINT32_MAX - left < right ? UINT32_MAX : left + right;
}

static uint16_t saturating_add_u16(uint16_t left, uint16_t right)
{
    return UINT16_MAX - left < right ? UINT16_MAX : (uint16_t)(left + right);
}

static uint16_t crop_inventory(const game_state_t *state, game_crop_t crop)
{
    return crop == GAME_CROP_WHEAT
        ? state->inventory_wheat : state->inventory_crops[crop];
}

static uint16_t seed_inventory(const game_state_t *state, game_crop_t crop)
{
    return crop == GAME_CROP_WHEAT
        ? state->inventory_wheat_seed : state->inventory_seeds[crop];
}

static void set_crop_inventory(game_state_t *state, game_crop_t crop, uint16_t value)
{
    if (crop == GAME_CROP_WHEAT) state->inventory_wheat = value;
    else state->inventory_crops[crop] = value;
}

static void set_seed_inventory(game_state_t *state, game_crop_t crop, uint16_t value)
{
    if (crop == GAME_CROP_WHEAT) state->inventory_wheat_seed = value;
    else state->inventory_seeds[crop] = value;
}

static void add_pending_crop(game_state_t *state, game_crop_t crop, uint16_t count)
{
    if (crop == GAME_CROP_WHEAT) {
        state->pending.wheat = saturating_add_u16(state->pending.wheat, count);
    } else {
        state->pending_crops[crop] = saturating_add_u16(
            state->pending_crops[crop], count);
    }
}

static void add_pending_dish(game_state_t *state, game_recipe_t recipe,
                             uint16_t count)
{
    if (recipe == GAME_RECIPE_HOT_BREAD) {
        state->pending.hot_bread = saturating_add_u16(
            state->pending.hot_bread, count);
    } else {
        state->pending_dishes[recipe] = saturating_add_u16(
            state->pending_dishes[recipe], count);
    }
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

static uint8_t day_for_time(const game_state_t *state, uint32_t now)
{
    if (now < state->season_started_at) return 1U;
    uint32_t elapsed_days = (now - state->season_started_at) / (24U * 60U * 60U);
    return elapsed_days >= GAME_SPRING_DAY_COUNT
        ? GAME_SPRING_DAY_COUNT : (uint8_t)(elapsed_days + 1U);
}

static void update_calendar(game_state_t *state, uint32_t now)
{
    uint8_t previous = state->spring_day;
    uint8_t day = day_for_time(state, now);
    state->spring_day = day;
    state->weather = weather_for_day(state->weather_seed, day);
    if (previous < 7U && day >= 7U) {
        state->calendar_milestones |= GAME_EVENT_MARKET;
        if ((state->completed_events & GAME_EVENT_MARKET) == 0U) {
            state->pending_events |= GAME_EVENT_MARKET;
        }
    }
    if (previous < 14U && day >= 14U) {
        state->calendar_milestones |= GAME_EVENT_FESTIVAL;
        if ((state->completed_events & GAME_EVENT_FESTIVAL) == 0U) {
            state->pending_events |= GAME_EVENT_FESTIVAL;
        }
    }
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
        if (state->forest_runs < UINT8_MAX) state->forest_runs++;
        if (state->road_fragments < 9U) state->road_fragments++;
        break;
    case GAME_TASK_HOT_BREAD:
        add_pending_dish(state, task->recipe, 1U);
        if (state->cooked_counts[task->recipe] < UINT16_MAX) {
            state->cooked_counts[task->recipe]++;
        }
        state->atuan.job = GAME_JOB_REST;
        state->atuan.stamina = state->atuan.stamina > 4U
            ? (uint8_t)(state->atuan.stamina - 4U) : 0U;
        break;
    case GAME_TASK_TRAVEL_8H:
        state->pending.wood = saturating_add_u16(state->pending.wood, 5U);
        state->pending.berries = saturating_add_u16(state->pending.berries, 3U);
        state->pending.mushrooms = saturating_add_u16(
            state->pending.mushrooms, 1U);
        state->travel_journal_count = state->travel_journal_count == UINT8_MAX
            ? UINT8_MAX : (uint8_t)(state->travel_journal_count + 1U);
        state->amai.job = GAME_JOB_REST;
        state->atuan.job = GAME_JOB_REST;
        state->amai.stamina = state->amai.stamina > 15U
            ? (uint8_t)(state->amai.stamina - 15U) : 0U;
        state->atuan.stamina = state->atuan.stamina > 15U
            ? (uint8_t)(state->atuan.stamina - 15U) : 0U;
        break;
    case GAME_TASK_BUILDING:
        state->completed_buildings |= (uint8_t)(1U << task->building);
        task->active = false;
        task->kind = GAME_TASK_NONE;
        refresh_quest_progress(state);
        return;
    case GAME_TASK_NONE:
        return;
    }
    state->pending.available = true;
    task->active = false;
    task->kind = GAME_TASK_NONE;
    task->actor = GAME_ACTOR_NONE;
    refresh_quest_progress(state);
}

static void refresh_quest_progress(game_state_t *state)
{
    bool advanced;
    do {
        advanced = false;
        switch (state->quest_stage) {
        case 2U:
            if (state->cooked_counts[GAME_RECIPE_HOT_BREAD] > 0U) {
                state->quest_stage = 3U;
                state->coins = saturating_add_u32(state->coins, 100U);
                state->inventory_seeds[GAME_CROP_CARROT] = saturating_add_u16(
                    state->inventory_seeds[GAME_CROP_CARROT], 2U);
                advanced = true;
            }
            break;
        case 3U:
            if (state->total_crops_harvested >= 2U) {
                state->quest_stage = 4U;
                advanced = true;
            }
            break;
        case 4U:
            if (state->forest_runs > 0U) {
                state->quest_stage = 5U;
                advanced = true;
            }
            break;
        case 5U:
            if ((state->completed_buildings & (1U << GAME_BUILD_GUEST_ROOM)) != 0U) {
                state->quest_stage = 6U;
                state->unlocked_recipes |= (uint8_t)(
                    (1U << GAME_RECIPE_CARROT_STEW) |
                    (1U << GAME_RECIPE_HERB_TEA));
                state->inventory_seeds[GAME_CROP_HERB] = saturating_add_u16(
                    state->inventory_seeds[GAME_CROP_HERB], 2U);
                advanced = true;
            }
            break;
        case 6U:
            if (state->cooked_counts[GAME_RECIPE_CARROT_STEW] > 0U) {
                state->quest_stage = 7U;
                advanced = true;
            }
            break;
        case 7U: {
            uint32_t cooked = 0U;
            for (size_t i = 0; i < GAME_RECIPE_COUNT; i++) {
                cooked += state->cooked_counts[i];
            }
            if (cooked >= 3U) {
                state->quest_stage = 8U;
                state->unlocked_recipes |= (uint8_t)(1U << GAME_RECIPE_FOREST_CAKE);
                advanced = true;
            }
            break;
        }
        case 8U:
            if ((state->completed_events & GAME_EVENT_MARKET) != 0U) {
                state->quest_stage = 9U;
                state->unlocked_recipes |= (uint8_t)(1U << GAME_RECIPE_STRAWBERRY_JAM);
                state->inventory_seeds[GAME_CROP_STRAWBERRY] = saturating_add_u16(
                    state->inventory_seeds[GAME_CROP_STRAWBERRY], 2U);
                advanced = true;
            }
            break;
        case 9U:
            if ((state->completed_buildings & (1U << GAME_BUILD_SIGNPOST)) != 0U) {
                state->quest_stage = 10U;
                advanced = true;
            }
            break;
        case 10U:
            if (state->travel_journal_count > 0U &&
                (state->completed_events & GAME_EVENT_FESTIVAL) != 0U) {
                state->quest_stage = 11U;
                state->chapter_complete = true;
                advanced = true;
            }
            break;
        default:
            break;
        }
    } while (advanced);
}

static void settle_reception(game_state_t *state, uint32_t from, uint32_t elapsed)
{
    uint32_t productive = elapsed;
    if (productive > GAME_RECEPTION_CAP_SECONDS) {
        productive = GAME_RECEPTION_CAP_SECONDS;
    }

    uint32_t hours = productive / 3600U;
    if (hours == 0U) return;

    uint32_t income = 0U;
    for (uint32_t hour = 0U; hour < hours; hour++) {
        uint32_t sample = from + hour * 3600U;
        game_weather_t weather = weather_for_day(
            state->weather_seed, day_for_time(state, sample));
        uint32_t hourly = 10U;
        if (weather == GAME_WEATHER_RAIN) hourly = 9U;
        if (weather == GAME_WEATHER_STORM) hourly = 15U;
        income = saturating_add_u32(income, hourly);
    }
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
    state->unlocked_recipes = (uint8_t)(1U << GAME_RECIPE_HOT_BREAD);
    state->completed_buildings = (uint8_t)(
        (1U << GAME_BUILD_FRONT_DESK) |
        (1U << GAME_BUILD_KITCHEN) |
        (1U << GAME_BUILD_FARM));
    state->quest_stage = 2U;
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
        uint32_t settlement_end = state->last_settled_time + elapsed;
        if (state->momo.job == GAME_JOB_RECEPTION) {
            settle_reception(state, state->last_settled_time, elapsed);
        }
        if (state->forest.active && state->forest.ends_at <= settlement_end) {
            complete_timed_task(state, &state->forest);
        }
        if (state->kitchen.active && state->kitchen.ends_at <= settlement_end) {
            complete_timed_task(state, &state->kitchen);
        }
        if (state->travel.active && state->travel.ends_at <= settlement_end) {
            complete_timed_task(state, &state->travel);
        }
        if (state->construction.active &&
            state->construction.ends_at <= settlement_end) {
            complete_timed_task(state, &state->construction);
        }
        for (size_t i = 0; i < GAME_FARM_PLOT_COUNT; i++) {
            game_farm_plot_t *plot = &state->farm[i];
            if (plot->active && plot->matures_at <= settlement_end) {
                const game_crop_definition_t *definition =
                    game_crop_definition(plot->crop);
                if (definition) {
                    add_pending_crop(state, plot->crop, definition->yield);
                    state->total_crops_harvested = saturating_add_u16(
                        state->total_crops_harvested, definition->yield);
                    state->pending.available = true;
                    refresh_quest_progress(state);
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
        /* Calendar follows trusted wall time even when economic simulation is capped. */
        update_calendar(state, action.now);
        state->last_settled_time = action.now;
        state->last_trusted_time = action.now;
        state->commit_sequence++;
        return true;
    }

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
    case GAME_ACTION_START_RECIPE: {
        game_recipe_t recipe = action.type == GAME_ACTION_START_ATUAN_HOT_BREAD
            ? GAME_RECIPE_HOT_BREAD : (game_recipe_t)action.target;
        const game_recipe_definition_t *definition = game_recipe_definition(recipe);
        if (!definition || (state->unlocked_recipes & (1U << recipe)) == 0U ||
            state->kitchen.active || state->atuan.job != GAME_JOB_REST ||
            crop_inventory(state, definition->crop_a) < definition->crop_a_count ||
            (definition->crop_b != GAME_CROP_NONE &&
             crop_inventory(state, definition->crop_b) < definition->crop_b_count) ||
            state->inventory_berries < definition->berries ||
            action.now > UINT32_MAX - definition->cook_seconds ||
            action.now != state->last_settled_time) {
            return false;
        }
        set_crop_inventory(state, definition->crop_a,
            (uint16_t)(crop_inventory(state, definition->crop_a) -
                       definition->crop_a_count));
        if (definition->crop_b != GAME_CROP_NONE) {
            set_crop_inventory(state, definition->crop_b,
                (uint16_t)(crop_inventory(state, definition->crop_b) -
                           definition->crop_b_count));
        }
        state->inventory_berries = (uint16_t)(state->inventory_berries -
                                               definition->berries);
        state->kitchen.active = true;
        state->kitchen.kind = GAME_TASK_HOT_BREAD;
        state->kitchen.recipe = recipe;
        state->kitchen.actor = GAME_ACTOR_ATUAN;
        state->kitchen.task_id = state->commit_sequence + 1U;
        state->kitchen.started_at = action.now;
        state->kitchen.ends_at = action.now + definition->cook_seconds;
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
        state->inventory_mushrooms = saturating_add_u16(
            state->inventory_mushrooms, state->pending.mushrooms);
        for (game_crop_t crop = GAME_CROP_CARROT;
             crop < GAME_CROP_COUNT; crop++) {
            state->inventory_crops[crop] = saturating_add_u16(
                state->inventory_crops[crop], state->pending_crops[crop]);
        }
        for (game_recipe_t recipe = GAME_RECIPE_CARROT_STEW;
             recipe < GAME_RECIPE_COUNT; recipe++) {
            state->inventory_dishes[recipe] = saturating_add_u16(
                state->inventory_dishes[recipe], state->pending_dishes[recipe]);
        }
        memset(&state->pending, 0, sizeof(state->pending));
        memset(state->pending_crops, 0, sizeof(state->pending_crops));
        memset(state->pending_dishes, 0, sizeof(state->pending_dishes));
        state->commit_sequence++;
        return true;

    case GAME_ACTION_PLANT_WHEAT:
    case GAME_ACTION_PLANT_CROP: {
        game_crop_t crop = action.type == GAME_ACTION_PLANT_WHEAT
            ? GAME_CROP_WHEAT : (game_crop_t)action.option;
        const game_crop_definition_t *definition = game_crop_definition(crop);
        if (!definition || action.target >= GAME_FARM_PLOT_COUNT ||
            state->farm[action.target].active ||
            seed_inventory(state, crop) == 0U ||
            action.now > UINT32_MAX - definition->grow_seconds ||
            action.now != state->last_settled_time) {
            return false;
        }
        set_seed_inventory(state, crop,
                           (uint16_t)(seed_inventory(state, crop) - 1U));
        state->farm[action.target].active = true;
        state->farm[action.target].crop = crop;
        state->farm[action.target].planted_at = action.now;
        state->farm[action.target].matures_at = action.now + definition->grow_seconds;
        state->lulu.job = GAME_JOB_FARM;
        state->lulu.job_started_at = action.now;
        state->commit_sequence++;
        return true;
    }

    case GAME_ACTION_RESOLVE_EVENT: {
        uint8_t event = action.target;
        if ((event != GAME_EVENT_MARKET && event != GAME_EVENT_FESTIVAL) ||
            (state->pending_events & event) == 0U) {
            return false;
        }
        state->pending_events &= (uint8_t)~event;
        state->completed_events |= event;
        if (event == GAME_EVENT_MARKET) {
            state->pending.coins = saturating_add_u32(state->pending.coins, 20U);
            state->pending.available = true;
        } else {
            state->pending.coins = saturating_add_u32(state->pending.coins, 50U);
            state->inventory_wheat_seed = saturating_add_u16(
                state->inventory_wheat_seed, 2U);
            state->pending.available = true;
        }
        refresh_quest_progress(state);
        state->commit_sequence++;
        return true;
    }

    case GAME_ACTION_START_TRAVEL:
        if (state->spring_day < 8U ||
            (state->completed_buildings & (1U << GAME_BUILD_SIGNPOST)) == 0U ||
            state->travel.active ||
            state->amai.job != GAME_JOB_REST ||
            state->atuan.job != GAME_JOB_REST ||
            state->amai.stamina < 40U || state->atuan.stamina < 40U ||
            state->inventory_hot_bread == 0U ||
            action.now > UINT32_MAX - 8U * 60U * 60U ||
            action.now != state->last_settled_time) {
            return false;
        }
        state->inventory_hot_bread--;
        state->travel.active = true;
        state->travel.kind = GAME_TASK_TRAVEL_8H;
        state->travel.task_id = state->commit_sequence + 1U;
        state->travel.started_at = action.now;
        state->travel.ends_at = action.now + 8U * 60U * 60U;
        state->amai.job = GAME_JOB_FOREST;
        state->atuan.job = GAME_JOB_FOREST;
        state->amai.job_started_at = action.now;
        state->atuan.job_started_at = action.now;
        state->commit_sequence++;
        return true;

    case GAME_ACTION_START_BUILDING: {
        game_building_t building = (game_building_t)action.target;
        const game_building_definition_t *definition =
            game_building_definition(building);
        if (!definition || state->construction.active ||
            (state->completed_buildings & (1U << building)) != 0U ||
            state->inventory_wood < definition->wood ||
            state->coins < definition->coins ||
            state->road_fragments < definition->road_fragments ||
            action.now > UINT32_MAX - definition->build_seconds ||
            action.now != state->last_settled_time) {
            return false;
        }
        state->inventory_wood = (uint16_t)(state->inventory_wood - definition->wood);
        state->coins -= definition->coins;
        state->road_fragments = (uint8_t)(state->road_fragments -
                                           definition->road_fragments);
        if (definition->build_seconds == 0U) {
            state->completed_buildings |= (uint8_t)(1U << building);
            refresh_quest_progress(state);
        } else {
            state->construction.active = true;
            state->construction.kind = GAME_TASK_BUILDING;
            state->construction.building = building;
            state->construction.task_id = state->commit_sequence + 1U;
            state->construction.started_at = action.now;
            state->construction.ends_at = action.now + definition->build_seconds;
        }
        state->commit_sequence++;
        return true;
    }
    }

    return false;
}
