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
    assert(save_service_store(&save, &second));
    assert(save_service_load(&save, &loaded));
    assert(loaded.coins == 80U);
    assert(loaded.momo.stamina == 71U);

    /* Slot 1 is newest. Corrupt its payload and verify fallback to slot 0. */
    storage.bytes[SAVE_SLOT_SIZE + 28U] ^= 0x01U;
    assert(save_service_load(&save, &loaded));
    assert(loaded.coins == 25U);

    return 0;
}

