#pragma once

#include "game/game_state.h"

#include <stdbool.h>

bool app_persistence_load(game_state_t *state);
bool app_persistence_store(const game_state_t *state);

