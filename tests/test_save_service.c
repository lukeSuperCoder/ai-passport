#include "game/game_state.h"
#include "services/save_service.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    uint8_t bytes[SAVE_SLOT_SIZE * SAVE_SLOT_COUNT];
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

int main(void)
{
    memory_storage_t storage;
    memset(&storage, 0xFF, sizeof(storage));
    save_backend_t save = backend(&storage);
    game_state_t loaded;
    assert(!save_service_load(&save, &loaded));

    game_state_t first;
    game_state_init(&first, 100U);
    first.coins = 25U;
    assert(save_service_store(&save, &first));
    assert(save_service_load(&save, &loaded));
    assert(loaded.coins == 25U);

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

    /* Slot 1 is newest. Corrupt its payload and verify fallback to slot 0. */
    storage.bytes[SAVE_SLOT_SIZE + 28U] ^= 0x01U;
    assert(save_service_load(&save, &loaded));
    assert(loaded.coins == 25U);

    return 0;
}
