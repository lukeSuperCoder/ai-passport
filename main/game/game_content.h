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

typedef struct {
    const char *name;
    const char *species;
    const char *personality;
    uint8_t stamina;
    uint8_t dexterity;
    uint8_t perception;
    uint8_t charm;
    uint8_t focus;
    game_job_t preferred_job;
} game_pet_definition_t;

typedef enum {
    GAME_EVENT_TYPE_MAIN = 0,
    GAME_EVENT_TYPE_PET,
    GAME_EVENT_TYPE_JOB,
    GAME_EVENT_TYPE_WEATHER,
    GAME_EVENT_TYPE_COMBINATION,
    GAME_EVENT_TYPE_TRAVEL,
    GAME_EVENT_TYPE_VISITOR,
    GAME_EVENT_TYPE_COUNT,
} game_event_type_t;

typedef struct {
    uint8_t id;
    game_event_type_t type;
    const char *title;
    bool repeatable;
    uint8_t cooldown_days;
} game_event_definition_t;

const game_crop_definition_t *game_crop_definition(game_crop_t crop);
const game_recipe_definition_t *game_recipe_definition(game_recipe_t recipe);
const game_building_definition_t *game_building_definition(game_building_t building);
const game_pet_definition_t *game_pet_definition(game_pet_id_t pet);
const game_event_definition_t *game_event_definition(uint8_t index);
const char *game_traveler_dialogue(game_weather_t weather, uint8_t period,
                                   uint32_t seed);
bool game_content_validate(void);
