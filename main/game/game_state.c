#include "game_state.h"
#include "game_content.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

_Static_assert(sizeof(game_state_t) <= 12U * 1024U,
               "game_state_t exceeds the MVP resident RAM budget");

static void refresh_quest_progress(game_state_t *state);
static game_pet_state_t *pet_state(game_state_t *state, game_pet_id_t pet);

uint8_t game_available_farm_plots(const game_state_t *state)
{
    return state && (state->completed_buildings & (1U << GAME_BUILD_SINK)) != 0U
        ? GAME_FARM_PLOT_COUNT : GAME_FARM_INITIAL_PLOT_COUNT;
}

static int relationship_index(game_pet_id_t left, game_pet_id_t right)
{
    if (left == right || left >= GAME_PET_COUNT || right >= GAME_PET_COUNT) return -1;
    if (left > right) {
        game_pet_id_t swap = left;
        left = right;
        right = swap;
    }
    static const int8_t indexes[GAME_PET_COUNT][GAME_PET_COUNT] = {
        { -1, 0, 1, 2 },
        { -1, -1, 3, 4 },
        { -1, -1, -1, 5 },
        { -1, -1, -1, -1 },
    };
    return indexes[left][right];
}

uint8_t game_relationship(const game_state_t *state, game_pet_id_t left,
                          game_pet_id_t right)
{
    int index = relationship_index(left, right);
    return state && index >= 0 ? state->relationships[index] : 0U;
}

game_job_score_t game_calculate_job_score(const game_state_t *state,
                                          game_pet_id_t pet, game_job_t job,
                                          game_pet_id_t partner)
{
    game_job_score_t result = { .score = 0, .yield_percent = 80U };
    const game_pet_definition_t *definition = game_pet_definition(pet);
    if (!state || !definition || job <= GAME_JOB_REST || job > GAME_JOB_FARM) {
        return result;
    }
    uint8_t primary = 0U;
    uint8_t secondary = 0U;
    switch (job) {
    case GAME_JOB_RECEPTION:
        primary = definition->charm;
        secondary = definition->perception;
        break;
    case GAME_JOB_KITCHEN:
        primary = definition->dexterity;
        secondary = definition->focus;
        break;
    case GAME_JOB_FARM:
        primary = definition->dexterity;
        secondary = definition->focus;
        break;
    case GAME_JOB_FOREST:
        primary = definition->perception;
        secondary = definition->stamina;
        break;
    case GAME_JOB_REST:
        break;
    }
    int score = 50 + primary * 5 + secondary * 3;
    if (definition->preferred_job == job) score += 25;
    game_pet_state_t *pet_status = pet_state((game_state_t *)state, pet);
    if (pet_status->mood >= 70U) score += 5;
    else if (pet_status->mood < 40U) score -= 10;
    uint16_t experience = state->job_experience[pet][job];
    if (experience >= 150U) score += 10;
    else if (experience >= 50U) score += 5;
    if (partner < GAME_PET_COUNT && partner != pet) {
        uint8_t relationship = game_relationship(state, pet, partner);
        if (relationship >= 80U) score += 10;
        else if (relationship >= 50U) score += 5;
    }
    if (job == GAME_JOB_RECEPTION) {
        if (state->weather == GAME_WEATHER_RAIN) score -= 10;
        else if (state->weather == GAME_WEATHER_STORM) score += 10;
    } else if (job == GAME_JOB_FOREST) {
        if (state->weather == GAME_WEATHER_CLOUDY) score += 10;
        else if (state->weather == GAME_WEATHER_STORM) score -= 10;
    } else if (job == GAME_JOB_FARM && state->weather >= GAME_WEATHER_RAIN) {
        score += 10;
    }
    if (score < 0) score = 0;
    if (score > INT16_MAX) score = INT16_MAX;
    result.score = (int16_t)score;
    if (score >= 110) {
        result.yield_percent = 140U;
        result.premium_chance = 25U;
    } else if (score >= 90) {
        result.yield_percent = 120U;
        result.premium_chance = 15U;
    } else if (score >= 70) {
        result.yield_percent = 100U;
        result.premium_chance = 5U;
    }
    return result;
}

static game_pet_state_t *pet_state(game_state_t *state, game_pet_id_t pet)
{
    switch (pet) {
    case GAME_PET_MOMO: return &state->momo;
    case GAME_PET_LULU: return &state->lulu;
    case GAME_PET_AMAI: return &state->amai;
    case GAME_PET_ATUAN: return &state->atuan;
    }
    return NULL;
}

static bool event_seen(const game_state_t *state, uint8_t id)
{
    return (state->event_seen[id / 8U] & (1U << (id % 8U))) != 0U;
}

static bool enqueue_event(game_state_t *state, uint8_t id)
{
    const game_event_definition_t *definition = game_event_definition(id);
    if (!definition || state->event_queue_count >= GAME_EVENT_QUEUE_SIZE ||
        (!definition->repeatable && event_seen(state, id))) {
        return false;
    }
    for (uint8_t i = 0U; i < state->event_queue_count; i++) {
        if (state->event_queue[i].id == id) return false;
    }
    uint8_t last_day = state->event_last_day[id];
    if (definition->repeatable && last_day != 0U &&
        state->spring_day < (uint8_t)(last_day + definition->cooldown_days)) {
        return false;
    }
    state->event_queue[state->event_queue_count++] = (game_queued_event_t){
        .id = id,
        .queued_day = state->spring_day,
    };
    return true;
}

static void record_event_history(game_state_t *state, uint8_t id)
{
    if (state->event_history_count < GAME_EVENT_HISTORY_SIZE) {
        state->event_history[state->event_history_count++] = id;
        return;
    }
    memmove(state->event_history, state->event_history + 1,
            GAME_EVENT_HISTORY_SIZE - 1U);
    state->event_history[GAME_EVENT_HISTORY_SIZE - 1U] = id;
}

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

static void add_pending_premium_dish(game_state_t *state,
                                     game_recipe_t recipe, uint16_t count)
{
    if (recipe == GAME_RECIPE_HOT_BREAD) {
        state->pending_premium_hot_bread = saturating_add_u16(
            state->pending_premium_hot_bread, count);
    } else {
        state->pending_premium_dishes[recipe] = saturating_add_u16(
            state->pending_premium_dishes[recipe], count);
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
    if (day != state->companion_actions_day) {
        state->companion_actions_day = day;
        state->companion_actions = 2U;
    }
    if (previous < 7U && day >= 7U) {
        state->calendar_milestones |= GAME_EVENT_MARKET;
        if ((state->completed_events & GAME_EVENT_MARKET) == 0U) {
            state->pending_events |= GAME_EVENT_MARKET;
        }
    }
    if (previous != day) {
        enqueue_event(state, (uint8_t)(34U + state->weather));
        if (day >= 3U && state->visitor_stages[1] == 0U) enqueue_event(state, 55U);
        if (day >= 4U && state->visitor_stages[2] == 0U) enqueue_event(state, 57U);
        if (day >= 5U && state->visitor_stages[3] == 0U) enqueue_event(state, 59U);
        if (day >= 10U && state->visitor_stages[5] == 0U) enqueue_event(state, 63U);
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
        state->job_experience[GAME_PET_AMAI][GAME_JOB_FOREST] =
            saturating_add_u16(
                state->job_experience[GAME_PET_AMAI][GAME_JOB_FOREST], 10U);
        enqueue_event(state, (uint8_t)(30U + task->task_id % 4U));
        break;
    case GAME_TASK_FOREST_2H: {
        game_job_score_t score = game_calculate_job_score(
            state, GAME_PET_AMAI, GAME_JOB_FOREST, GAME_PET_COUNT);
        uint16_t wood = (uint16_t)((4U + task->task_id % 3U) *
                                   score.yield_percent / 100U);
        uint16_t berries = (uint16_t)((2U + (task->task_id / 3U) % 3U) *
                                      score.yield_percent / 100U);
        state->pending.wood = saturating_add_u16(state->pending.wood, wood);
        state->pending.berries = saturating_add_u16(state->pending.berries, berries);
        if ((task->task_id & 1U) == 0U) {
            state->pending.mushrooms = saturating_add_u16(
                state->pending.mushrooms, 1U);
        }
        state->last_forest_result = state->weather == GAME_WEATHER_STORM
            ? 2U : (uint8_t)(task->task_id % 2U);
        state->amai.job = GAME_JOB_REST;
        uint8_t cost = state->last_forest_result == 2U ? 20U : 15U;
        state->amai.stamina = state->amai.stamina > cost
            ? (uint8_t)(state->amai.stamina - cost) : 0U;
        if (state->forest_runs < UINT8_MAX) state->forest_runs++;
        if (state->road_fragments < 9U) state->road_fragments++;
        state->job_experience[GAME_PET_AMAI][GAME_JOB_FOREST] =
            saturating_add_u16(
                state->job_experience[GAME_PET_AMAI][GAME_JOB_FOREST], 25U);
        enqueue_event(state, (uint8_t)(34U + task->task_id % 6U));
        break;
    }
    case GAME_TASK_HOT_BREAD:
        {
            game_job_score_t score = game_calculate_job_score(
                state, GAME_PET_ATUAN, GAME_JOB_KITCHEN, GAME_PET_COUNT);
            uint8_t roll = (uint8_t)((task->task_id * 37U +
                                      state->weather_seed) % 100U);
            if (roll < score.premium_chance) {
                add_pending_premium_dish(state, task->recipe, 1U);
            } else {
                add_pending_dish(state, task->recipe, 1U);
            }
        }
        if (state->cooked_counts[task->recipe] < UINT16_MAX) {
            state->cooked_counts[task->recipe]++;
        }
        state->job_experience[GAME_PET_ATUAN][GAME_JOB_KITCHEN] =
            saturating_add_u16(
                state->job_experience[GAME_PET_ATUAN][GAME_JOB_KITCHEN], 10U);
        enqueue_event(state, (uint8_t)(26U + task->task_id % 4U));
        state->atuan.job = GAME_JOB_REST;
        state->atuan.stamina = state->atuan.stamina > 4U
            ? (uint8_t)(state->atuan.stamina - 4U) : 0U;
        break;
    case GAME_TASK_RECIPE_RESEARCH:
        state->recipe_research[task->recipe] =
            state->recipe_research[task->recipe] >= 50U
            ? 100U : (uint8_t)(state->recipe_research[task->recipe] + 50U);
        if (state->recipe_research[task->recipe] >= 100U) {
            state->unlocked_recipes |= (uint8_t)(1U << task->recipe);
        }
        state->atuan.job = GAME_JOB_REST;
        state->atuan.stamina = state->atuan.stamina > 6U
            ? (uint8_t)(state->atuan.stamina - 6U) : 0U;
        enqueue_event(state, (uint8_t)(26U + task->task_id % 4U));
        break;
    case GAME_TASK_TRAVEL_8H:
        state->last_travel_goal = (game_travel_goal_t)task->option;
        state->pending.wood = saturating_add_u16(
            state->pending.wood,
            task->option == GAME_TRAVEL_MATERIALS ? 8U : 4U);
        state->pending.berries = saturating_add_u16(
            state->pending.berries,
            task->option == GAME_TRAVEL_SCENERY ? 5U : 3U);
        state->pending.mushrooms = saturating_add_u16(
            state->pending.mushrooms, 1U);
        if (game_relationship(state, GAME_PET_AMAI, GAME_PET_ATUAN) >= 50U) {
            state->pending.mushrooms = saturating_add_u16(
                state->pending.mushrooms, 1U);
        }
        if (task->option == GAME_TRAVEL_OLD_ROAD && state->road_fragments < 9U) {
            state->road_fragments++;
        }
        if (task->option == GAME_TRAVEL_SCENERY && state->reputation < 100U) {
            state->reputation++;
        }
        int relation = relationship_index(GAME_PET_AMAI, GAME_PET_ATUAN);
        if (relation >= 0) {
            uint16_t improved = (uint16_t)state->relationships[relation] + 5U;
            state->relationships[relation] = improved > 100U ? 100U : (uint8_t)improved;
        }
        enqueue_event(state, (uint8_t)(44U + task->task_id % 9U));
        state->travel_journal_count = state->travel_journal_count == UINT8_MAX
            ? UINT8_MAX : (uint8_t)(state->travel_journal_count + 1U);
        state->amai.job = GAME_JOB_REST;
        state->atuan.job = GAME_JOB_REST;
        state->amai.stamina = state->amai.stamina > 15U
            ? (uint8_t)(state->amai.stamina - 15U) : 0U;
        state->atuan.stamina = state->atuan.stamina > 15U
            ? (uint8_t)(state->atuan.stamina - 15U) : 0U;
        state->notifications |= 0x01U;
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

static void recover_resting_pet(game_pet_state_t *pet, game_job_t initial_job,
                                uint32_t elapsed)
{
    if (initial_job != GAME_JOB_REST || pet->stamina >= 100U) return;
    uint32_t hours = elapsed / 3600U;
    if (hours > 8U) hours = 8U;
    uint32_t recovered = hours * 8U;
    pet->stamina = recovered >= 100U - pet->stamina
        ? 100U : (uint8_t)(pet->stamina + recovered);
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
    for (size_t i = 0; i < GAME_RELATION_COUNT; i++) {
        state->relationships[i] = 20U;
    }
    state->companion_actions = 2U;
    state->companion_actions_day = 1U;
    state->sound_enabled = true;
    state->night_mute_enabled = true;
    state->clock_24_hour = true;
    enqueue_event(state, 53U);
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
        game_job_t initial_momo_job = state->momo.job;
        game_job_t initial_lulu_job = state->lulu.job;
        game_job_t initial_amai_job = state->amai.job;
        game_job_t initial_atuan_job = state->atuan.job;
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
                    enqueue_event(state, (uint8_t)(22U + i));
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
        recover_resting_pet(&state->momo, initial_momo_job, elapsed);
        recover_resting_pet(&state->lulu, initial_lulu_job, elapsed);
        recover_resting_pet(&state->amai, initial_amai_job, elapsed);
        recover_resting_pet(&state->atuan, initial_atuan_job, elapsed);
        /* Calendar follows trusted wall time even when economic simulation is capped. */
        update_calendar(state, action.now);
        state->last_settled_time = action.now;
        state->last_trusted_time = action.now;
        state->commit_sequence++;
        return true;
    }

    case GAME_ACTION_START_AMAI_FOREST:
    case GAME_ACTION_START_FOREST_2H: {
        uint32_t duration = action.type == GAME_ACTION_START_FOREST_2H
            ? 2U * 60U * 60U : 30U * 60U;
        if (state->forest.active || state->amai.job != GAME_JOB_REST ||
            action.now > UINT32_MAX - duration ||
            action.now != state->last_settled_time) {
            return false;
        }
        state->forest.active = true;
        state->forest.kind = action.type == GAME_ACTION_START_FOREST_2H
            ? GAME_TASK_FOREST_2H : GAME_TASK_FOREST_30M;
        state->forest.actor = GAME_ACTOR_AMAI;
        state->forest.task_id = state->commit_sequence + 1U;
        state->forest.started_at = action.now;
        state->forest.ends_at = action.now + duration;
        state->amai.job = GAME_JOB_FOREST;
        state->amai.job_started_at = action.now;
        state->last_trusted_time = action.now;
        state->last_settled_time = action.now;
        state->commit_sequence++;
        return true;
    }

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
        state->inventory_premium_hot_bread = saturating_add_u16(
            state->inventory_premium_hot_bread,
            state->pending_premium_hot_bread);
        for (game_recipe_t recipe = GAME_RECIPE_CARROT_STEW;
             recipe < GAME_RECIPE_COUNT; recipe++) {
            state->inventory_premium_dishes[recipe] = saturating_add_u16(
                state->inventory_premium_dishes[recipe],
                state->pending_premium_dishes[recipe]);
        }
        memset(&state->pending, 0, sizeof(state->pending));
        memset(state->pending_crops, 0, sizeof(state->pending_crops));
        memset(state->pending_dishes, 0, sizeof(state->pending_dishes));
        memset(state->pending_premium_dishes, 0,
               sizeof(state->pending_premium_dishes));
        state->pending_premium_hot_bread = 0U;
        state->commit_sequence++;
        return true;

    case GAME_ACTION_PLANT_WHEAT:
    case GAME_ACTION_PLANT_CROP: {
        game_crop_t crop = action.type == GAME_ACTION_PLANT_WHEAT
            ? GAME_CROP_WHEAT : (game_crop_t)action.option;
        const game_crop_definition_t *definition = game_crop_definition(crop);
        if (!definition || action.target >= game_available_farm_plots(state) ||
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
            (state->inventory_hot_bread == 0U &&
             state->inventory_premium_hot_bread == 0U) ||
            action.now > UINT32_MAX - 8U * 60U * 60U ||
            action.now != state->last_settled_time ||
            action.option >= GAME_TRAVEL_GOAL_COUNT) {
            return false;
        }
        if (state->inventory_hot_bread > 0U) state->inventory_hot_bread--;
        else state->inventory_premium_hot_bread--;
        state->travel.active = true;
        state->travel.kind = GAME_TASK_TRAVEL_8H;
        state->travel.task_id = state->commit_sequence + 1U;
        state->travel.started_at = action.now;
        state->travel.ends_at = action.now + 8U * 60U * 60U;
        state->travel.option = action.option;
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

    case GAME_ACTION_TALK_TO_PET: {
        game_pet_id_t pet = (game_pet_id_t)action.target;
        game_pet_state_t *partner = pet_state(state, pet);
        if (!partner || state->companion_actions == 0U) return false;
        state->companion_actions--;
        partner->mood = partner->mood > 90U ? 100U : (uint8_t)(partner->mood + 10U);
        state->player_affinity[pet] = state->player_affinity[pet] > 95U
            ? 100U : (uint8_t)(state->player_affinity[pet] + 5U);
        state->commit_sequence++;
        return true;
    }

    case GAME_ACTION_TOGGLE_SETTING:
        switch (action.target) {
        case 0U: state->sound_enabled = !state->sound_enabled; break;
        case 1U: state->night_mute_enabled = !state->night_mute_enabled; break;
        case 2U: state->clock_24_hour = !state->clock_24_hour; break;
        default: return false;
        }
        state->commit_sequence++;
        return true;

    case GAME_ACTION_RESOLVE_CONTENT_EVENT: {
        if (state->event_queue_count == 0U || action.target > 1U) return false;
        uint8_t id = state->event_queue[0].id;
        const game_event_definition_t *definition = game_event_definition(id);
        if (!definition) return false;
        if (action.target == 0U) {
            state->pending.coins = saturating_add_u32(
                state->pending.coins, (uint32_t)(10U + definition->type * 2U));
            state->pending.available = true;
        } else {
            int relation = id % GAME_RELATION_COUNT;
            uint16_t improved = (uint16_t)state->relationships[relation] + 3U;
            state->relationships[relation] = improved > 100U ? 100U : (uint8_t)improved;
            game_pet_id_t pet = (game_pet_id_t)(id % GAME_PET_COUNT);
            state->player_affinity[pet] = state->player_affinity[pet] > 98U
                ? 100U : (uint8_t)(state->player_affinity[pet] + 2U);
        }
        state->event_last_day[id] = state->spring_day;
        if (!definition->repeatable) {
            state->event_seen[id / 8U] |= (uint8_t)(1U << (id % 8U));
        }
        if (definition->type == GAME_EVENT_TYPE_VISITOR) {
            uint8_t visitor = (uint8_t)((id - 53U) / 2U);
            if (visitor < GAME_VISITOR_COUNT && state->visitor_stages[visitor] < 3U) {
                state->visitor_stages[visitor]++;
            }
        }
        record_event_history(state, id);
        state->event_queue_count--;
        memmove(state->event_queue, state->event_queue + 1,
                state->event_queue_count * sizeof(state->event_queue[0]));
        memset(&state->event_queue[state->event_queue_count], 0,
               sizeof(state->event_queue[0]));
        state->commit_sequence++;
        return true;
    }

    case GAME_ACTION_SELL_DISH: {
        game_recipe_t recipe = (game_recipe_t)action.target;
        const game_recipe_definition_t *definition = game_recipe_definition(recipe);
        if (!definition) return false;
        uint16_t *stock = recipe == GAME_RECIPE_HOT_BREAD
            ? &state->inventory_hot_bread : &state->inventory_dishes[recipe];
        bool premium = false;
        if (*stock == 0U) {
            stock = recipe == GAME_RECIPE_HOT_BREAD
                ? &state->inventory_premium_hot_bread
                : &state->inventory_premium_dishes[recipe];
            premium = true;
        }
        if (*stock == 0U) return false;
        (*stock)--;
        uint32_t price = premium
            ? (uint32_t)definition->sell_price * 3U / 2U
            : definition->sell_price;
        state->coins = saturating_add_u32(state->coins, price);
        if (state->reputation < 100U) state->reputation++;
        state->commit_sequence++;
        return true;
    }

    case GAME_ACTION_START_RESEARCH: {
        game_recipe_t recipe = (game_recipe_t)action.target;
        const game_recipe_definition_t *definition = game_recipe_definition(recipe);
        if (!definition || recipe == GAME_RECIPE_HOT_BREAD ||
            (state->unlocked_recipes & (1U << recipe)) != 0U ||
            state->recipe_research[recipe] >= 100U || state->kitchen.active ||
            state->atuan.job != GAME_JOB_REST || state->inventory_berries == 0U ||
            action.now > UINT32_MAX - 60U * 60U ||
            action.now != state->last_settled_time) {
            return false;
        }
        state->inventory_berries--;
        state->kitchen.active = true;
        state->kitchen.kind = GAME_TASK_RECIPE_RESEARCH;
        state->kitchen.recipe = recipe;
        state->kitchen.actor = GAME_ACTOR_ATUAN;
        state->kitchen.task_id = state->commit_sequence + 1U;
        state->kitchen.started_at = action.now;
        state->kitchen.ends_at = action.now + 60U * 60U;
        state->atuan.job = GAME_JOB_KITCHEN;
        state->atuan.job_started_at = action.now;
        state->commit_sequence++;
        return true;
    }

    case GAME_ACTION_ASSIST_KITCHEN:
        if (!state->kitchen.active || state->companion_actions == 0U ||
            action.now != state->last_settled_time ||
            state->kitchen.ends_at <= action.now) {
            return false;
        }
        state->companion_actions--;
        state->kitchen.ends_at = action.now +
            (state->kitchen.ends_at - action.now) / 2U;
        state->player_affinity[GAME_PET_ATUAN] =
            state->player_affinity[GAME_PET_ATUAN] > 97U ? 100U :
            (uint8_t)(state->player_affinity[GAME_PET_ATUAN] + 3U);
        state->atuan.mood = state->atuan.mood > 90U ? 100U :
            (uint8_t)(state->atuan.mood + 10U);
        state->commit_sequence++;
        return true;
    }

    return false;
}
