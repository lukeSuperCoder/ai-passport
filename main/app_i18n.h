#pragma once

#include "game/game_state.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    APP_LANG_ZH_CN = 0,
    APP_LANG_EN,
} app_language_t;

const char *app_i18n_pick(app_language_t language,
                          const char *zh_cn, const char *en);
const char *app_i18n_weather(app_language_t language, game_weather_t weather);
const char *app_i18n_crop(app_language_t language, game_crop_t crop);
const char *app_i18n_recipe(app_language_t language, game_recipe_t recipe);
const char *app_i18n_building(app_language_t language, game_building_t building);
const char *app_i18n_pet(app_language_t language, game_pet_id_t pet);
const char *app_i18n_pet_species(app_language_t language, game_pet_id_t pet);
const char *app_i18n_pet_personality(app_language_t language, game_pet_id_t pet);
const char *app_i18n_event(app_language_t language, uint8_t event_id);
const char *app_i18n_dialogue(app_language_t language, game_weather_t weather,
                              uint8_t period, uint32_t seed);

