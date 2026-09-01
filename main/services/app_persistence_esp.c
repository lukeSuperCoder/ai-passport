#include "app_persistence.h"
#include "save_service.h"

#include "esp_partition.h"

static const esp_partition_t *s_partition;

static bool partition_read(void *context, size_t offset, void *data, size_t size)
{
    const esp_partition_t *partition = context;
    return esp_partition_read(partition, offset, data, size) == ESP_OK;
}

static bool partition_write(void *context, size_t offset, const void *data, size_t size)
{
    const esp_partition_t *partition = context;
    return esp_partition_write(partition, offset, data, size) == ESP_OK;
}

static bool partition_erase(void *context, size_t offset, size_t size)
{
    const esp_partition_t *partition = context;
    return esp_partition_erase_range(partition, offset, size) == ESP_OK;
}

static bool backend(save_backend_t *result)
{
    if (!s_partition) {
        s_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, 0x41,
                                               "game_save");
    }
    if (!s_partition || s_partition->size < SAVE_SLOT_SIZE * SAVE_SLOT_COUNT) {
        return false;
    }
    *result = (save_backend_t){
        .context = (void *)s_partition,
        .read = partition_read,
        .write = partition_write,
        .erase = partition_erase,
    };
    return true;
}

bool app_persistence_load(game_state_t *state)
{
    save_backend_t storage;
    return backend(&storage) && save_service_load(&storage, state);
}

bool app_persistence_store(const game_state_t *state)
{
    save_backend_t storage;
    return backend(&storage) && save_service_store(&storage, state);
}

