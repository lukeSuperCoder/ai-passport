#include <assert.h>
#include <stdint.h>

#include "game/game_state.h"
#include "game/game_content.h"

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
    assert(state.pending.wood == 8U);
    assert(state.pending.berries == 3U);
    assert(state.pending.mushrooms == 1U);
    assert(state.travel_journal_count == 1U);
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_CLAIM_REPORT,
    }));
    assert(state.inventory_mushrooms == 1U);
}

static void test_partner_relationship_changes_travel_result(void)
{
    game_state_t state;
    game_state_init(&state, 0U);
    state.spring_day = 8U;
    state.completed_buildings |= (uint8_t)(1U << GAME_BUILD_SIGNPOST);
    state.inventory_hot_bread = 1U;
    state.relationships[5] = 50U;
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_START_TRAVEL, .now = 0U,
    }));
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_SETTLE_TO_TIME, .now = 8U * 60U * 60U,
    }));
    assert(state.pending.mushrooms == 2U);
    assert(game_relationship(&state, GAME_PET_AMAI, GAME_PET_ATUAN) == 55U);
}

static void test_companion_actions_restore_each_day(void)
{
    game_state_t state;
    game_state_init(&state, 100U);
    assert(state.companion_actions == 2U);
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_TALK_TO_PET, .target = GAME_PET_MOMO,
    }));
    assert(state.momo.mood == 90U);
    assert(state.player_affinity[GAME_PET_MOMO] == 5U);
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_TALK_TO_PET, .target = GAME_PET_LULU,
    }));
    assert(!game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_TALK_TO_PET, .target = GAME_PET_AMAI,
    }));
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_SETTLE_TO_TIME, .now = 100U + 24U * 60U * 60U,
    }));
    assert(state.companion_actions == 2U);
}

static void test_content_catalog_is_complete_and_valid(void)
{
    assert(game_content_validate());
    for (uint8_t i = 0U; i < GAME_CONTENT_EVENT_COUNT; i++) {
        assert(game_event_definition(i));
    }
    assert(!game_event_definition(GAME_CONTENT_EVENT_COUNT));
    for (game_pet_id_t pet = GAME_PET_MOMO; pet < GAME_PET_COUNT; pet++) {
        assert(game_pet_definition(pet));
    }
}

static void test_settings_toggle_deterministically(void)
{
    game_state_t state;
    game_state_init(&state, 0U);
    assert(state.sound_enabled && state.night_mute_enabled && state.clock_24_hour);
    for (uint8_t setting = 0U; setting < 3U; setting++) {
        assert(game_reduce(&state, (game_action_t){
            .type = GAME_ACTION_TOGGLE_SETTING, .target = setting,
        }));
    }
    assert(!state.sound_enabled && !state.night_mute_enabled && !state.clock_24_hour);
    assert(!game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_TOGGLE_SETTING, .target = 3U,
    }));
}

static void test_content_events_wait_for_player_choice(void)
{
    game_state_t state;
    game_state_init(&state, 0U);
    assert(state.event_queue_count == 1U);
    assert(state.event_queue[0].id == 53U);
    uint32_t coins = state.pending.coins;
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_RESOLVE_CONTENT_EVENT, .target = 0U,
    }));
    assert(state.event_queue_count == 0U);
    assert(state.pending.coins > coins);
    assert(state.visitor_stages[0] == 1U);
    assert(state.event_history_count == 1U);
    assert(!game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_RESOLVE_CONTENT_EVENT, .target = 0U,
    }));

    state.inventory_wheat = 2U;
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_START_RECIPE, .now = 0U,
        .target = GAME_RECIPE_HOT_BREAD,
    }));
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_SETTLE_TO_TIME, .now = 10U * 60U,
    }));
    assert(state.event_queue_count == 1U);
    uint8_t relation = state.relationships[state.event_queue[0].id %
                                           GAME_RELATION_COUNT];
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_RESOLVE_CONTENT_EVENT, .target = 1U,
    }));
    assert(state.relationships[state.event_history[1] % GAME_RELATION_COUNT] ==
           relation + 3U);
}

static void test_job_scores_include_strength_mood_relationship_and_weather(void)
{
    game_state_t state;
    game_state_init(&state, 0U);
    state.weather = GAME_WEATHER_CLEAR;
    game_job_score_t momo = game_calculate_job_score(
        &state, GAME_PET_MOMO, GAME_JOB_RECEPTION, GAME_PET_COUNT);
    assert(momo.score == 107);
    assert(momo.yield_percent == 120U);
    assert(momo.premium_chance == 15U);

    game_job_score_t amai = game_calculate_job_score(
        &state, GAME_PET_AMAI, GAME_JOB_FOREST, GAME_PET_ATUAN);
    assert(amai.score == 112);
    state.relationships[5] = 50U;
    state.weather = GAME_WEATHER_CLOUDY;
    amai = game_calculate_job_score(
        &state, GAME_PET_AMAI, GAME_JOB_FOREST, GAME_PET_ATUAN);
    assert(amai.score == 127);
    assert(amai.yield_percent == 140U);
    state.amai.mood = 20U;
    state.job_experience[GAME_PET_AMAI][GAME_JOB_FOREST] = 150U;
    amai = game_calculate_job_score(
        &state, GAME_PET_AMAI, GAME_JOB_FOREST, GAME_PET_ATUAN);
    assert(amai.score == 122);
}

static void test_rest_recovers_eight_per_hour_with_eight_hour_cap(void)
{
    game_state_t state;
    game_state_init(&state, 0U);
    state.momo.stamina = 10U;
    state.lulu.stamina = 50U;
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_SETTLE_TO_TIME, .now = 2U * 60U * 60U,
    }));
    assert(state.momo.stamina == 26U);
    assert(state.lulu.stamina == 66U);
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_SETTLE_TO_TIME, .now = 20U * 60U * 60U,
    }));
    assert(state.momo.stamina == 90U);
    assert(state.lulu.stamina == 100U);
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
    assert(state.inventory_hot_bread > 0U ||
           state.inventory_premium_hot_bread > 0U);
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
    for (uint8_t plot = 0U; plot < GAME_FARM_INITIAL_PLOT_COUNT; plot++) {
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

static void test_sink_building_unlocks_two_additional_plots(void)
{
    game_state_t state;
    game_state_init(&state, 0U);
    assert(game_available_farm_plots(&state) == 4U);
    assert(!game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_PLANT_CROP, .now = 0U,
        .target = 4U, .option = GAME_CROP_WHEAT,
    }));
    state.completed_buildings |= (uint8_t)(1U << GAME_BUILD_SINK);
    assert(game_available_farm_plots(&state) == 6U);
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_PLANT_CROP, .now = 0U,
        .target = 4U, .option = GAME_CROP_WHEAT,
    }));
    assert(state.farm[4].active);
}

static void test_cooking_assist_and_sale_complete_economic_loop(void)
{
    game_state_t state;
    game_state_init(&state, 0U);
    state.inventory_wheat = 2U;
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_START_RECIPE, .now = 0U,
        .target = GAME_RECIPE_HOT_BREAD,
    }));
    assert(state.kitchen.ends_at == 10U * 60U);
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_ASSIST_KITCHEN, .now = 0U,
    }));
    assert(state.kitchen.ends_at == 5U * 60U);
    assert(state.companion_actions == 1U);
    assert(state.player_affinity[GAME_PET_ATUAN] == 3U);
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_SETTLE_TO_TIME, .now = 5U * 60U,
    }));
    assert(game_reduce(&state, (game_action_t){ .type = GAME_ACTION_CLAIM_REPORT }));
    assert(state.inventory_hot_bread == 1U);
    uint32_t coins = state.coins;
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_SELL_DISH, .target = GAME_RECIPE_HOT_BREAD,
    }));
    assert(state.inventory_hot_bread == 0U);
    assert(state.coins == coins + 28U);
    assert(state.reputation == 1U);
    assert(!game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_SELL_DISH, .target = GAME_RECIPE_HOT_BREAD,
    }));
}

static void test_recipe_research_unlocks_after_two_sessions(void)
{
    game_state_t state;
    game_state_init(&state, 0U);
    state.inventory_berries = 2U;
    for (uint32_t session = 0U; session < 2U; session++) {
        uint32_t now = session * 60U * 60U;
        assert(game_reduce(&state, (game_action_t){
            .type = GAME_ACTION_START_RESEARCH, .now = now,
            .target = GAME_RECIPE_CARROT_STEW,
        }));
        assert(game_reduce(&state, (game_action_t){
            .type = GAME_ACTION_SETTLE_TO_TIME, .now = now + 60U * 60U,
        }));
    }
    assert(state.recipe_research[GAME_RECIPE_CARROT_STEW] == 100U);
    assert(state.unlocked_recipes & (1U << GAME_RECIPE_CARROT_STEW));
    assert(state.inventory_berries == 0U);
}

static void test_heat_game_is_single_use_and_boosts_quality(void)
{
    game_state_t state;
    game_state_init(&state, 0U);
    state.inventory_wheat = 2U;
    state.weather_seed = 95U;
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_START_RECIPE,
        .target = GAME_RECIPE_HOT_BREAD,
    }));
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_FINISH_HEAT_GAME,
        .option = 100U,
    }));
    assert(state.kitchen.option == 41U);
    assert(state.kitchen.ends_at == 5U * 60U);
    assert(!game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_FINISH_HEAT_GAME,
        .option = 100U,
    }));
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_SETTLE_TO_TIME, .now = 5U * 60U,
    }));
    assert(state.pending_premium_hot_bread == 1U);
}

static void test_premium_dish_is_claimed_and_sells_for_bonus(void)
{
    game_state_t state;
    game_state_init(&state, 0U);
    state.inventory_wheat = 2U;
    state.weather_seed = 70U;
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_START_RECIPE, .target = GAME_RECIPE_HOT_BREAD,
    }));
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_SETTLE_TO_TIME, .now = 10U * 60U,
    }));
    assert(state.pending_premium_hot_bread == 1U);
    assert(game_reduce(&state, (game_action_t){ .type = GAME_ACTION_CLAIM_REPORT }));
    assert(state.inventory_premium_hot_bread == 1U);
    uint32_t coins = state.coins;
    assert(game_reduce(&state, (game_action_t){
        .type = GAME_ACTION_SELL_DISH, .target = GAME_RECIPE_HOT_BREAD,
    }));
    assert(state.coins == coins + 42U);
}

static void test_two_hour_forest_and_travel_goals_have_distinct_results(void)
{
    game_state_t forest;
    game_state_init(&forest, 0U);
    assert(game_reduce(&forest, (game_action_t){
        .type = GAME_ACTION_START_FOREST_2H,
    }));
    assert(forest.forest.ends_at == 2U * 60U * 60U);
    assert(game_reduce(&forest, (game_action_t){
        .type = GAME_ACTION_SETTLE_TO_TIME, .now = 2U * 60U * 60U,
    }));
    assert(forest.pending.wood >= 4U);
    assert(forest.job_experience[GAME_PET_AMAI][GAME_JOB_FOREST] == 25U);

    game_state_t road;
    game_state_init(&road, 0U);
    road.spring_day = 8U;
    road.completed_buildings |= (uint8_t)(1U << GAME_BUILD_SIGNPOST);
    road.inventory_hot_bread = 1U;
    assert(game_reduce(&road, (game_action_t){
        .type = GAME_ACTION_START_TRAVEL, .option = GAME_TRAVEL_OLD_ROAD,
    }));
    assert(game_reduce(&road, (game_action_t){
        .type = GAME_ACTION_SETTLE_TO_TIME, .now = 8U * 60U * 60U,
    }));
    assert(road.last_travel_goal == GAME_TRAVEL_OLD_ROAD);
    assert(road.road_fragments == 1U);
    assert(road.notifications & GAME_NOTICE_TRAVEL);
    assert(game_reduce(&road, (game_action_t){
        .type = GAME_ACTION_CLEAR_NOTIFICATIONS,
    }));
    assert(road.notifications == 0U);
    assert(!game_reduce(&road, (game_action_t){
        .type = GAME_ACTION_CLEAR_NOTIFICATIONS,
    }));
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
    test_partner_relationship_changes_travel_result();
    test_companion_actions_restore_each_day();
    test_content_catalog_is_complete_and_valid();
    test_settings_toggle_deterministically();
    test_content_events_wait_for_player_choice();
    test_job_scores_include_strength_mood_relationship_and_weather();
    test_rest_recovers_eight_per_hour_with_eight_hour_cap();
    test_sink_building_unlocks_two_additional_plots();
    test_cooking_assist_and_sale_complete_economic_loop();
    test_recipe_research_unlocks_after_two_sessions();
    test_heat_game_is_single_use_and_boosts_quality();
    test_premium_dish_is_claimed_and_sells_for_bonus();
    test_two_hour_forest_and_travel_goals_have_distinct_results();
    test_all_crops_and_recipes_form_production_chains();
    test_main_story_can_reach_spring_14_without_deadlock();
    return 0;
}
