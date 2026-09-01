#include "services/app_persistence.h"
#include "services/save_service.h"

#include <stdio.h>
#include <string.h>

#define SAVE_FILE "simulator/build/time_station.save"
#define SAVE_TOTAL_SIZE (SAVE_SLOT_SIZE * SAVE_SLOT_COUNT)

static bool file_read(void *context, size_t offset, void *data, size_t size)
{
    (void)context;
    FILE *file = fopen(SAVE_FILE, "rb");
    if (!file) return false;
    bool ok = fseek(file, (long)offset, SEEK_SET) == 0 &&
              fread(data, 1, size, file) == size;
    fclose(file);
    return ok;
}

static bool ensure_file(void)
{
    FILE *file = fopen(SAVE_FILE, "rb");
    if (file) {
        bool ok = fseek(file, 0, SEEK_END) == 0 &&
                  ftell(file) == (long)SAVE_TOTAL_SIZE;
        fclose(file);
        if (ok) return true;
    }

    file = fopen(SAVE_FILE, "wb");
    if (!file) return false;
    uint8_t erased[256];
    memset(erased, 0xFF, sizeof(erased));
    for (size_t written = 0; written < SAVE_TOTAL_SIZE; written += sizeof(erased)) {
        if (fwrite(erased, 1, sizeof(erased), file) != sizeof(erased)) {
            fclose(file);
            return false;
        }
    }
    return fclose(file) == 0;
}

static bool file_write(void *context, size_t offset, const void *data, size_t size)
{
    (void)context;
    if (!ensure_file()) return false;
    FILE *file = fopen(SAVE_FILE, "r+b");
    if (!file) return false;
    bool ok = fseek(file, (long)offset, SEEK_SET) == 0 &&
              fwrite(data, 1, size, file) == size && fflush(file) == 0;
    fclose(file);
    return ok;
}

static bool file_erase(void *context, size_t offset, size_t size)
{
    (void)context;
    if (!ensure_file()) return false;
    FILE *file = fopen(SAVE_FILE, "r+b");
    if (!file || fseek(file, (long)offset, SEEK_SET) != 0) {
        if (file) fclose(file);
        return false;
    }
    uint8_t erased[256];
    memset(erased, 0xFF, sizeof(erased));
    bool ok = true;
    while (size > 0U) {
        size_t chunk = size < sizeof(erased) ? size : sizeof(erased);
        if (fwrite(erased, 1, chunk, file) != chunk) {
            ok = false;
            break;
        }
        size -= chunk;
    }
    ok = ok && fflush(file) == 0;
    fclose(file);
    return ok;
}

static save_backend_t backend(void)
{
    return (save_backend_t){
        .read = file_read,
        .write = file_write,
        .erase = file_erase,
    };
}

bool app_persistence_load(game_state_t *state)
{
    save_backend_t storage = backend();
    return save_service_load(&storage, state);
}

bool app_persistence_store(const game_state_t *state)
{
    save_backend_t storage = backend();
    return save_service_store(&storage, state);
}

