#include "app_i18n.h"

#include <assert.h>
#include <string.h>

int main(void)
{
    assert(strcmp(app_i18n_weather(APP_LANG_ZH_CN, GAME_WEATHER_RAIN), "雨") == 0);
    assert(strcmp(app_i18n_weather(APP_LANG_EN, GAME_WEATHER_RAIN), "RAIN") == 0);
    assert(strcmp(app_i18n_crop(APP_LANG_ZH_CN, GAME_CROP_STRAWBERRY), "草莓") == 0);
    assert(strcmp(app_i18n_recipe(APP_LANG_EN, GAME_RECIPE_HERB_TEA), "HERB TEA") == 0);
    assert(strcmp(app_i18n_building(APP_LANG_ZH_CN, GAME_BUILD_SIGNPOST), "路牌") == 0);
    assert(strcmp(app_i18n_pet(APP_LANG_ZH_CN, GAME_PET_ATUAN), "阿团") == 0);
    for (uint8_t id = 0U; id < GAME_CONTENT_EVENT_COUNT; id++) {
        assert(app_i18n_event(APP_LANG_ZH_CN, id)[0] != '\0');
        assert(app_i18n_event(APP_LANG_EN, id)[0] != '\0');
    }
    for (uint8_t weather = GAME_WEATHER_CLEAR;
         weather <= GAME_WEATHER_STORM; weather++) {
        for (uint8_t period = 0U; period < 4U; period++) {
            assert(app_i18n_dialogue(APP_LANG_ZH_CN,
                                     (game_weather_t)weather,
                                     period, 0U)[0] != '\0');
        }
    }
    return 0;
}
