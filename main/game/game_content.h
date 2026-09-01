#pragma once

#include <stdint.h>

#include "game_state.h"

typedef struct {
    const char *name;
    uint32_t grow_seconds;
    uint8_t yield;
    uint8_t seed_price;
    uint8_t sell_price;
} game_crop_definition_t;

typedef struct {
    const char *name;
    uint32_t cook_seconds;
    uint8_t sell_price;
    game_crop_t crop_a;
    uint8_t crop_a_count;
    game_crop_t crop_b;
    uint8_t crop_b_count;
    uint8_t berries;
} game_recipe_definition_t;

typedef struct {
    const char *name;
    uint16_t wood;
    uint16_t coins;
    uint8_t road_fragments;
    uint32_t build_seconds;
} game_building_definition_t;

const game_crop_definition_t *game_crop_definition(game_crop_t crop);
const game_recipe_definition_t *game_recipe_definition(game_recipe_t recipe);
const game_building_definition_t *game_building_definition(game_building_t building);
