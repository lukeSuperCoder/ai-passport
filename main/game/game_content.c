#include "game_content.h"

#include <stddef.h>

static const game_crop_definition_t s_crops[GAME_CROP_COUNT] = {
    [GAME_CROP_WHEAT] = { "WHEAT", 24U * 60U * 60U, 2U, 8U, 8U },
    [GAME_CROP_CARROT] = { "CARROT", 24U * 60U * 60U, 1U, 10U, 18U },
    [GAME_CROP_STRAWBERRY] = { "STRAWBERRY", 48U * 60U * 60U, 2U, 15U, 20U },
    [GAME_CROP_HERB] = { "HERB", 36U * 60U * 60U, 2U, 12U, 12U },
};

static const game_recipe_definition_t s_recipes[GAME_RECIPE_COUNT] = {
    [GAME_RECIPE_HOT_BREAD] = {
        "HOT BREAD", 10U * 60U, 28U, GAME_CROP_WHEAT, 2U,
        GAME_CROP_NONE, 0U, 0U,
    },
    [GAME_RECIPE_CARROT_STEW] = {
        "CARROT STEW", 30U * 60U, 45U, GAME_CROP_CARROT, 1U,
        GAME_CROP_HERB, 1U, 0U,
    },
    [GAME_RECIPE_STRAWBERRY_JAM] = {
        "STRAWBERRY JAM", 60U * 60U, 55U, GAME_CROP_STRAWBERRY, 2U,
        GAME_CROP_NONE, 0U, 0U,
    },
    [GAME_RECIPE_HERB_TEA] = {
        "HERB TEA", 10U * 60U, 32U, GAME_CROP_HERB, 2U,
        GAME_CROP_NONE, 0U, 0U,
    },
    [GAME_RECIPE_FOREST_CAKE] = {
        "FOREST CAKE", 30U * 60U, 42U, GAME_CROP_WHEAT, 1U,
        GAME_CROP_NONE, 0U, 2U,
    },
};

static const game_building_definition_t s_buildings[GAME_BUILD_COUNT] = {
    [GAME_BUILD_FRONT_DESK] = { "FRONT DESK", 2U, 0U, 0U, 0U },
    [GAME_BUILD_KITCHEN] = { "KITCHEN", 2U, 30U, 0U, 0U },
    [GAME_BUILD_FARM] = { "FARM", 2U, 0U, 0U, 10U * 60U },
    [GAME_BUILD_GUEST_ROOM] = { "GUEST ROOM", 8U, 150U, 0U, 2U * 60U * 60U },
    [GAME_BUILD_SINK] = { "SINK", 6U, 100U, 0U, 2U * 60U * 60U },
    [GAME_BUILD_SIGNPOST] = { "SIGNPOST", 10U, 0U, 3U, 4U * 60U * 60U },
};

const game_crop_definition_t *game_crop_definition(game_crop_t crop)
{
    if (crop <= GAME_CROP_NONE || crop >= GAME_CROP_COUNT) return NULL;
    return &s_crops[crop];
}

const game_recipe_definition_t *game_recipe_definition(game_recipe_t recipe)
{
    if (recipe >= GAME_RECIPE_COUNT) return NULL;
    return &s_recipes[recipe];
}

const game_building_definition_t *game_building_definition(game_building_t building)
{
    if (building >= GAME_BUILD_COUNT) return NULL;
    return &s_buildings[building];
}
