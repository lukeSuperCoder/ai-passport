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

static const game_pet_definition_t s_pets[GAME_PET_COUNT] = {
    [GAME_PET_MOMO] = { "MOMO", "BLACK CAT", "TIDY", 2U, 4U, 4U, 3U, 4U,
                        GAME_JOB_RECEPTION },
    [GAME_PET_LULU] = { "LULU", "WHITE RABBIT", "ATTACHED", 3U, 4U, 3U, 4U, 3U,
                        GAME_JOB_FARM },
    [GAME_PET_AMAI] = { "AMAI", "YELLOW DOG", "ENTHUSIASTIC", 4U, 2U, 4U, 4U, 2U,
                        GAME_JOB_FOREST },
    [GAME_PET_ATUAN] = { "ATUAN", "BROWN BEAR", "HUNGRY", 5U, 3U, 2U, 2U, 3U,
                         GAME_JOB_KITCHEN },
};

#define EVENT(id_, type_, title_, repeat_) \
    { id_, type_, title_, repeat_, repeat_ ? 2U : 0U }

static const game_event_definition_t s_events[GAME_CONTENT_EVENT_COUNT] = {
    EVENT(0, GAME_EVENT_TYPE_MAIN, "RELIGHT THE LANTERN", false),
    EVENT(1, GAME_EVENT_TYPE_MAIN, "FIRST BREAKFAST", false),
    EVENT(2, GAME_EVENT_TYPE_MAIN, "ABANDONED YARD", false),
    EVENT(3, GAME_EVENT_TYPE_MAIN, "LETTER WITHOUT ADDRESS", false),
    EVENT(4, GAME_EVENT_TYPE_MAIN, "LEAKING GUEST ROOM", false),
    EVENT(5, GAME_EVENT_TYPE_MAIN, "FAILED STEW", false),
    EVENT(6, GAME_EVENT_TYPE_MAIN, "TASTE OF HOME", false),
    EVENT(7, GAME_EVENT_TYPE_MAIN, "ROADSIDE MARKET", false),
    EVENT(8, GAME_EVENT_TYPE_MAIN, "OLD SIGNPOST", false),
    EVENT(9, GAME_EVENT_TYPE_MAIN, "THE ROAD RELIT", false),
    EVENT(10, GAME_EVENT_TYPE_PET, "MOMO'S POLISHED BELL", false),
    EVENT(11, GAME_EVENT_TYPE_PET, "MOMO'S OLD MEMORY", false),
    EVENT(12, GAME_EVENT_TYPE_PET, "LULU'S FIRST BUD", false),
    EVENT(13, GAME_EVENT_TYPE_PET, "LULU'S FLOWER PROMISE", false),
    EVENT(14, GAME_EVENT_TYPE_PET, "AMAI'S LOST SCENT", false),
    EVENT(15, GAME_EVENT_TYPE_PET, "AMAI'S UNDELIVERED LETTER", false),
    EVENT(16, GAME_EVENT_TYPE_PET, "ATUAN'S SECRET SNACK", false),
    EVENT(17, GAME_EVENT_TYPE_PET, "ATUAN'S HOME DISH", false),
    EVENT(18, GAME_EVENT_TYPE_JOB, "RECEPTION: SHY GUEST", true),
    EVENT(19, GAME_EVENT_TYPE_JOB, "RECEPTION: WRONG CHANGE", true),
    EVENT(20, GAME_EVENT_TYPE_JOB, "RECEPTION: OLD REGULAR", true),
    EVENT(21, GAME_EVENT_TYPE_JOB, "RECEPTION: LATE ARRIVAL", true),
    EVENT(22, GAME_EVENT_TYPE_JOB, "FARM: DRY SOIL", true),
    EVENT(23, GAME_EVENT_TYPE_JOB, "FARM: TINY SPROUT", true),
    EVENT(24, GAME_EVENT_TYPE_JOB, "FARM: GARDEN PEST", true),
    EVENT(25, GAME_EVENT_TYPE_JOB, "FARM: GIANT HARVEST", true),
    EVENT(26, GAME_EVENT_TYPE_JOB, "KITCHEN: LOW FLAME", true),
    EVENT(27, GAME_EVENT_TYPE_JOB, "KITCHEN: EXTRA PORTION", true),
    EVENT(28, GAME_EVENT_TYPE_JOB, "KITCHEN: NEW AROMA", true),
    EVENT(29, GAME_EVENT_TYPE_JOB, "KITCHEN: BURNT EDGE", true),
    EVENT(30, GAME_EVENT_TYPE_JOB, "FOREST: FORKED PATH", true),
    EVENT(31, GAME_EVENT_TYPE_JOB, "FOREST: BERRY PATCH", true),
    EVENT(32, GAME_EVENT_TYPE_JOB, "FOREST: OLD MARK", true),
    EVENT(33, GAME_EVENT_TYPE_JOB, "FOREST: BLUE FEATHER", true),
    EVENT(34, GAME_EVENT_TYPE_WEATHER, "CLEAR MORNING", true),
    EVENT(35, GAME_EVENT_TYPE_WEATHER, "CLOUD SHADOW", true),
    EVENT(36, GAME_EVENT_TYPE_WEATHER, "RAIN AT THE EAVES", true),
    EVENT(37, GAME_EVENT_TYPE_WEATHER, "STORM SHELTER", true),
    EVENT(38, GAME_EVENT_TYPE_COMBINATION, "MOMO AND LULU TIDY UP", false),
    EVENT(39, GAME_EVENT_TYPE_COMBINATION, "MOMO AND AMAI GREET", false),
    EVENT(40, GAME_EVENT_TYPE_COMBINATION, "MOMO AND ATUAN TASTE", false),
    EVENT(41, GAME_EVENT_TYPE_COMBINATION, "LULU AND AMAI EXPLORE", false),
    EVENT(42, GAME_EVENT_TYPE_COMBINATION, "LULU AND ATUAN PREPARE", false),
    EVENT(43, GAME_EVENT_TYPE_COMBINATION, "AMAI AND ATUAN CAMP", false),
    EVENT(44, GAME_EVENT_TYPE_TRAVEL, "MISTPINE DEPARTURE", true),
    EVENT(45, GAME_EVENT_TYPE_TRAVEL, "MOSSY BRIDGE", true),
    EVENT(46, GAME_EVENT_TYPE_TRAVEL, "QUIET CLEARING", true),
    EVENT(47, GAME_EVENT_TYPE_TRAVEL, "ANIMAL TRACKS", true),
    EVENT(48, GAME_EVENT_TYPE_TRAVEL, "BROKEN MILESTONE", true),
    EVENT(49, GAME_EVENT_TYPE_TRAVEL, "FOREST RAIN", true),
    EVENT(50, GAME_EVENT_TYPE_TRAVEL, "DISTANT LANTERN", true),
    EVENT(51, GAME_EVENT_TYPE_TRAVEL, "STAR STAMP", false),
    EVENT(52, GAME_EVENT_TYPE_TRAVEL, "MISTPINE RETURN", true),
    EVENT(53, GAME_EVENT_TYPE_VISITOR, "QINGHE SEEKS SHELTER", false),
    EVENT(54, GAME_EVENT_TYPE_VISITOR, "QINGHE SENDS SEEDS", false),
    EVENT(55, GAME_EVENT_TYPE_VISITOR, "XIAOYU LOSES A CLUE", false),
    EVENT(56, GAME_EVENT_TYPE_VISITOR, "XIAOYU FINDS THE ROAD", false),
    EVENT(57, GAME_EVENT_TYPE_VISITOR, "UNCLE BAI'S CART", true),
    EVENT(58, GAME_EVENT_TYPE_VISITOR, "UNCLE BAI'S BARGAIN", true),
    EVENT(59, GAME_EVENT_TYPE_VISITOR, "ATUAN ASKS TO COOK", false),
    EVENT(60, GAME_EVENT_TYPE_VISITOR, "ATUAN STAYS", false),
    EVENT(61, GAME_EVENT_TYPE_VISITOR, "YAO PAINTS THE INN", false),
    EVENT(62, GAME_EVENT_TYPE_VISITOR, "YAO'S SECOND SKETCH", false),
    EVENT(63, GAME_EVENT_TYPE_VISITOR, "GRAYSHADOW AT THE DOOR", false),
    EVENT(64, GAME_EVENT_TYPE_VISITOR, "GRAYSHADOW'S STAMP", false),
};

#undef EVENT

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

const game_pet_definition_t *game_pet_definition(game_pet_id_t pet)
{
    if (pet >= GAME_PET_COUNT) return NULL;
    return &s_pets[pet];
}

const game_event_definition_t *game_event_definition(uint8_t index)
{
    if (index >= GAME_CONTENT_EVENT_COUNT) return NULL;
    return &s_events[index];
}

bool game_content_validate(void)
{
    uint8_t counts[GAME_EVENT_TYPE_COUNT] = {0};
    for (uint8_t i = 0U; i < GAME_CONTENT_EVENT_COUNT; i++) {
        const game_event_definition_t *event = &s_events[i];
        if (event->id != i || event->type >= GAME_EVENT_TYPE_COUNT ||
            !event->title || event->title[0] == '\0' ||
            (event->repeatable && event->cooldown_days < 2U) ||
            (!event->repeatable && event->cooldown_days != 0U)) {
            return false;
        }
        counts[event->type]++;
    }
    return counts[GAME_EVENT_TYPE_MAIN] == 10U &&
           counts[GAME_EVENT_TYPE_PET] == 8U &&
           counts[GAME_EVENT_TYPE_JOB] == 16U &&
           counts[GAME_EVENT_TYPE_WEATHER] == 4U &&
           counts[GAME_EVENT_TYPE_COMBINATION] == 6U &&
           counts[GAME_EVENT_TYPE_TRAVEL] == 9U &&
           counts[GAME_EVENT_TYPE_VISITOR] == 12U;
}
