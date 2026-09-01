#include "game/game_state.h"
#include "services/save_service.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    uint8_t bytes[SAVE_SLOT_SIZE * SAVE_SLOT_COUNT];
    unsigned write_count;
    unsigned fail_write_at;
} memory_storage_t;

static bool memory_read(void *context, size_t offset, void *data, size_t size)
{
    memory_storage_t *storage = context;
    if (offset + size > sizeof(storage->bytes)) return false;
    memcpy(data, storage->bytes + offset, size);
    return true;
}

static bool memory_write(void *context, size_t offset, const void *data, size_t size)
{
    memory_storage_t *storage = context;
    if (storage->write_count++ == storage->fail_write_at) return false;
    if (offset + size > sizeof(storage->bytes)) return false;
    memcpy(storage->bytes + offset, data, size);
    return true;
}

static bool memory_erase(void *context, size_t offset, size_t size)
{
    memory_storage_t *storage = context;
    if (offset + size > sizeof(storage->bytes)) return false;
    memset(storage->bytes + offset, 0xFF, size);
    return true;
}

static save_backend_t backend(memory_storage_t *storage)
{
    return (save_backend_t){
        .context = storage,
        .read = memory_read,
        .write = memory_write,
        .erase = memory_erase,
    };
}

static void write_u16(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8U);
}

static void write_u32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8U);
    data[2] = (uint8_t)(value >> 16U);
    data[3] = (uint8_t)(value >> 24U);
}

static uint32_t test_crc32(const uint8_t *data, size_t size)
{
    uint32_t crc = 0xFFFFFFFFU;
    for (size_t i = 0; i < size; i++) {
        crc ^= data[i];
        for (unsigned bit = 0; bit < 8U; bit++) {
            uint32_t mask = (uint32_t)-(int32_t)(crc & 1U);
            crc = (crc >> 1U) ^ (0xEDB88320U & mask);
        }
    }
    return ~crc;
}

int main(void)
{
    memory_storage_t storage;
    memset(&storage, 0xFF, sizeof(storage));
    storage.write_count = 0U;
    storage.fail_write_at = UINT32_MAX;
    save_backend_t save = backend(&storage);
    game_state_t loaded;
    assert(!save_service_load(&save, &loaded));

    game_state_t first;
    game_state_init(&first, 100U);
    first.coins = 25U;
    assert(save_service_store(&save, &first));
    assert(save_service_load(&save, &loaded));
    assert(loaded.coins == 25U);

    /* Every interrupted write stage must preserve the previous committed slot. */
    memory_storage_t baseline = storage;
    for (unsigned failure = 0U; failure < 3U; failure++) {
        memory_storage_t interrupted = baseline;
        interrupted.write_count = 0U;
        interrupted.fail_write_at = failure;
        save_backend_t interrupted_save = backend(&interrupted);
        game_state_t replacement = first;
        replacement.coins = 999U;
        assert(!save_service_store(&interrupted_save, &replacement));
        interrupted.fail_write_at = UINT32_MAX;
        assert(save_service_load(&interrupted_save, &loaded));
        assert(loaded.coins == 25U);
    }

    game_state_t second = first;
    second.coins = 80U;
    second.momo.stamina = 71U;
    second.amai.stamina = 95U;
    second.forest.active = true;
    second.forest.task_id = 7U;
    second.forest.started_at = 1000U;
    second.forest.ends_at = 2800U;
    second.pending.wood = 3U;
    second.pending.hot_bread = 1U;
    second.atuan.stamina = 96U;
    second.kitchen.active = true;
    second.kitchen.kind = GAME_TASK_HOT_BREAD;
    second.kitchen.actor = GAME_ACTOR_ATUAN;
    second.kitchen.task_id = 8U;
    second.kitchen.started_at = 2000U;
    second.kitchen.ends_at = 2600U;
    second.inventory_wheat = 2U;
    second.inventory_wheat_seed = 3U;
    second.pending.wheat = 2U;
    second.lulu.job = GAME_JOB_FARM;
    second.farm[0].active = true;
    second.farm[0].crop = GAME_CROP_WHEAT;
    second.farm[0].planted_at = 3000U;
    second.farm[0].matures_at = 89400U;
    second.season_started_at = 1234U;
    second.weather_seed = 5678U;
    second.spring_day = 7U;
    second.weather = GAME_WEATHER_RAIN;
    second.calendar_milestones = 0x01U;
    second.pending_events = GAME_EVENT_MARKET;
    second.completed_events = GAME_EVENT_FESTIVAL;
    second.pending.mushrooms = 1U;
    second.inventory_mushrooms = 2U;
    second.travel_journal_count = 3U;
    second.travel.active = true;
    second.travel.kind = GAME_TASK_TRAVEL_8H;
    second.travel.task_id = 9U;
    second.travel.started_at = 4000U;
    second.travel.ends_at = 32800U;
    second.inventory_crops[GAME_CROP_CARROT] = 4U;
    second.inventory_seeds[GAME_CROP_STRAWBERRY] = 5U;
    second.inventory_dishes[GAME_RECIPE_HERB_TEA] = 2U;
    second.pending_crops[GAME_CROP_HERB] = 3U;
    second.pending_dishes[GAME_RECIPE_FOREST_CAKE] = 1U;
    second.unlocked_recipes = 0x1FU;
    second.kitchen.recipe = GAME_RECIPE_CARROT_STEW;
    second.quest_stage = 9U;
    second.completed_buildings = 0x1FU;
    second.reputation = 17U;
    second.forest_runs = 4U;
    second.road_fragments = 3U;
    second.total_crops_harvested = 12U;
    second.cooked_counts[GAME_RECIPE_HERB_TEA] = 2U;
    second.construction.active = true;
    second.construction.kind = GAME_TASK_BUILDING;
    second.construction.building = GAME_BUILD_SIGNPOST;
    second.construction.started_at = 5000U;
    second.construction.ends_at = 19400U;
    second.relationships[5] = 73U;
    second.player_affinity[GAME_PET_MOMO] = 35U;
    second.job_experience[GAME_PET_ATUAN][GAME_JOB_KITCHEN] = 155U;
    second.companion_actions = 1U;
    second.companion_actions_day = 7U;
    second.sound_enabled = false;
    second.night_mute_enabled = false;
    second.clock_24_hour = false;
    second.event_queue_count = 2U;
    second.event_queue[0] = (game_queued_event_t){ .id = 18U, .queued_day = 7U };
    second.event_queue[1] = (game_queued_event_t){ .id = 34U, .queued_day = 7U };
    second.event_last_day[18] = 5U;
    second.event_seen[53U / 8U] |= (uint8_t)(1U << (53U % 8U));
    second.event_history_count = 2U;
    second.event_history[0] = 53U;
    second.event_history[1] = 18U;
    second.visitor_stages[0] = 1U;
    second.farm[4].active = true;
    second.farm[4].crop = GAME_CROP_HERB;
    second.farm[4].planted_at = 6000U;
    second.farm[4].matures_at = 135600U;
    second.inventory_premium_hot_bread = 2U;
    second.pending_premium_dishes[GAME_RECIPE_HERB_TEA] = 3U;
    second.recipe_research[GAME_RECIPE_FOREST_CAKE] = 50U;
    second.last_forest_result = 2U;
    second.last_travel_goal = GAME_TRAVEL_OLD_ROAD;
    second.notifications = 3U;
    second.travel.option = GAME_TRAVEL_OLD_ROAD;
    assert(save_service_store(&save, &second));
    assert(save_service_load(&save, &loaded));
    assert(loaded.coins == 80U);
    assert(loaded.momo.stamina == 71U);
    assert(loaded.amai.stamina == 95U);
    assert(loaded.forest.active);
    assert(loaded.forest.ends_at == 2800U);
    assert(loaded.pending.wood == 3U);
    assert(loaded.pending.hot_bread == 1U);
    assert(loaded.kitchen.active);
    assert(loaded.kitchen.kind == GAME_TASK_HOT_BREAD);
    assert(loaded.inventory_wheat == 2U);
    assert(loaded.inventory_wheat_seed == 3U);
    assert(loaded.pending.wheat == 2U);
    assert(loaded.farm[0].active);
    assert(loaded.farm[0].matures_at == 89400U);
    assert(loaded.season_started_at == 1234U);
    assert(loaded.weather_seed == 5678U);
    assert(loaded.spring_day == 7U);
    assert(loaded.weather == GAME_WEATHER_RAIN);
    assert(loaded.pending_events == GAME_EVENT_MARKET);
    assert(loaded.completed_events == GAME_EVENT_FESTIVAL);
    assert(loaded.pending.mushrooms == 1U);
    assert(loaded.inventory_mushrooms == 2U);
    assert(loaded.travel_journal_count == 3U);
    assert(loaded.travel.active);
    assert(loaded.travel.ends_at == 32800U);
    assert(loaded.inventory_crops[GAME_CROP_CARROT] == 4U);
    assert(loaded.inventory_seeds[GAME_CROP_STRAWBERRY] == 5U);
    assert(loaded.inventory_dishes[GAME_RECIPE_HERB_TEA] == 2U);
    assert(loaded.pending_crops[GAME_CROP_HERB] == 3U);
    assert(loaded.pending_dishes[GAME_RECIPE_FOREST_CAKE] == 1U);
    assert(loaded.unlocked_recipes == 0x1FU);
    assert(loaded.kitchen.recipe == GAME_RECIPE_CARROT_STEW);
    assert(loaded.quest_stage == 9U);
    assert(loaded.completed_buildings == 0x1FU);
    assert(loaded.reputation == 17U);
    assert(loaded.forest_runs == 4U);
    assert(loaded.road_fragments == 3U);
    assert(loaded.total_crops_harvested == 12U);
    assert(loaded.cooked_counts[GAME_RECIPE_HERB_TEA] == 2U);
    assert(loaded.construction.active);
    assert(loaded.construction.building == GAME_BUILD_SIGNPOST);
    assert(loaded.relationships[5] == 73U);
    assert(loaded.player_affinity[GAME_PET_MOMO] == 35U);
    assert(loaded.job_experience[GAME_PET_ATUAN][GAME_JOB_KITCHEN] == 155U);
    assert(loaded.companion_actions == 1U);
    assert(loaded.companion_actions_day == 7U);
    assert(!loaded.sound_enabled);
    assert(!loaded.night_mute_enabled);
    assert(!loaded.clock_24_hour);
    assert(loaded.event_queue_count == 2U);
    assert(loaded.event_queue[0].id == 18U);
    assert(loaded.event_last_day[18] == 5U);
    assert(loaded.event_seen[53U / 8U] & (1U << (53U % 8U)));
    assert(loaded.event_history_count == 2U);
    assert(loaded.event_history[1] == 18U);
    assert(loaded.visitor_stages[0] == 1U);
    assert(loaded.farm[4].active);
    assert(loaded.farm[4].crop == GAME_CROP_HERB);
    assert(loaded.farm[4].matures_at == 135600U);
    assert(loaded.inventory_premium_hot_bread == 2U);
    assert(loaded.pending_premium_dishes[GAME_RECIPE_HERB_TEA] == 3U);
    assert(loaded.recipe_research[GAME_RECIPE_FOREST_CAKE] == 50U);
    assert(loaded.last_forest_result == 2U);
    assert(loaded.last_travel_goal == GAME_TRAVEL_OLD_ROAD);
    assert(loaded.notifications == 3U);
    assert(loaded.travel.option == GAME_TRAVEL_OLD_ROAD);

    /* A valid v13 slot migrates with every v14-only field at its safe default. */
    memory_storage_t legacy = storage;
    uint8_t *legacy_header = legacy.bytes + SAVE_SLOT_SIZE;
    uint8_t *legacy_payload = legacy_header + 28U;
    write_u16(legacy_header + 4U, 13U);
    write_u32(legacy_header + 12U, 472U);
    write_u32(legacy_header + 16U, test_crc32(legacy_payload, 472U));
    write_u32(legacy_header + 20U, test_crc32(legacy_header, 20U));
    save_backend_t legacy_save = backend(&legacy);
    assert(save_service_load(&legacy_save, &loaded));
    assert(loaded.coins == 80U);
    assert(loaded.inventory_premium_hot_bread == 0U);
    assert(loaded.recipe_research[GAME_RECIPE_FOREST_CAKE] == 0U);
    assert(loaded.notifications == 0U);
    assert(loaded.last_travel_goal == GAME_TRAVEL_MATERIALS);
    assert(loaded.travel.option == GAME_TRAVEL_MATERIALS);

    /* Slot 1 is newest. Corrupt its payload and verify fallback to slot 0. */
    storage.bytes[SAVE_SLOT_SIZE + 28U] ^= 0x01U;
    assert(save_service_load(&save, &loaded));
    assert(loaded.coins == 25U);

    return 0;
}
