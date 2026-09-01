#pragma once

#include "game/game_state.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SAVE_SLOT_SIZE 0x10000U
#define SAVE_SLOT_COUNT 2U

typedef bool (*save_read_fn)(void *context, size_t offset, void *data, size_t size);
typedef bool (*save_write_fn)(void *context, size_t offset, const void *data, size_t size);
typedef bool (*save_erase_fn)(void *context, size_t offset, size_t size);

typedef struct {
    void *context;
    save_read_fn read;
    save_write_fn write;
    save_erase_fn erase;
} save_backend_t;

bool save_service_load(const save_backend_t *backend, game_state_t *state);
bool save_service_store(const save_backend_t *backend, const game_state_t *state);

