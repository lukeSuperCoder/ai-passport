#include "app_i18n.h"

#include "game/game_content.h"

#include <stddef.h>

typedef struct {
    const char *zh_cn;
    const char *en;
} localized_text_t;

const char *app_i18n_pick(app_language_t language,
                          const char *zh_cn, const char *en)
{
    return language == APP_LANG_EN ? en : zh_cn;
}

static const localized_text_t s_weather[] = {
    { "晴", "CLEAR" }, { "多云", "CLOUDY" },
    { "雨", "RAIN" }, { "暴风雨", "STORM" },
};

static const localized_text_t s_crops[GAME_CROP_COUNT] = {
    { "无", "NONE" }, { "小麦", "WHEAT" }, { "胡萝卜", "CARROT" },
    { "草莓", "STRAWBERRY" }, { "香草", "HERB" },
};

static const localized_text_t s_recipes[GAME_RECIPE_COUNT] = {
    { "热面包", "HOT BREAD" }, { "胡萝卜炖菜", "CARROT STEW" },
    { "草莓果酱", "STRAWBERRY JAM" }, { "香草茶", "HERB TEA" },
    { "森林蛋糕", "FOREST CAKE" },
};

static const localized_text_t s_buildings[GAME_BUILD_COUNT] = {
    { "前台", "FRONT DESK" }, { "厨房", "KITCHEN" },
    { "农田", "FARM" }, { "客房", "GUEST ROOM" },
    { "水槽", "SINK" }, { "路牌", "SIGNPOST" },
};

static const localized_text_t s_pet_names[GAME_PET_COUNT] = {
    { "默默", "MOMO" }, { "露露", "LULU" },
    { "阿麦", "AMAI" }, { "阿团", "ATUAN" },
};

static const localized_text_t s_pet_species[GAME_PET_COUNT] = {
    { "黑猫", "BLACK CAT" }, { "白兔", "WHITE RABBIT" },
    { "黄狗", "YELLOW DOG" }, { "棕熊", "BROWN BEAR" },
};

static const localized_text_t s_pet_personality[GAME_PET_COUNT] = {
    { "爱整洁", "TIDY" }, { "黏人", "ATTACHED" },
    { "热情", "ENTHUSIASTIC" }, { "贪吃", "HUNGRY" },
};

static const char *s_event_zh[GAME_CONTENT_EVENT_COUNT] = {
    "重燃灯火", "第一顿早餐", "荒废的院子", "没有地址的信", "漏雨的客房",
    "失败的炖菜", "家的味道", "路边集市", "旧路牌", "道路重亮",
    "默默擦亮的铃铛", "默默的旧回忆", "露露的第一株嫩芽", "露露的花之约",
    "阿麦丢失的气味", "阿麦未送达的信", "阿团的秘密零食", "阿团的家乡菜",
    "前台：害羞的客人", "前台：找错零钱", "前台：老熟客", "前台：深夜来客",
    "农田：干燥土壤", "农田：小小嫩芽", "农田：花园害虫", "农田：大丰收",
    "厨房：火力太小", "厨房：额外一份", "厨房：新的香气", "厨房：焦糊边缘",
    "森林：岔路口", "森林：浆果丛", "森林：古老标记", "森林：蓝色羽毛",
    "晴朗清晨", "云影", "屋檐听雨", "暴风雨避难所",
    "默默和露露大扫除", "默默和阿麦迎客", "默默和阿团试味",
    "露露和阿麦探索", "露露和阿团备餐", "阿麦和阿团露营",
    "雾松林出发", "长满青苔的桥", "寂静空地", "动物足迹", "损坏的里程碑",
    "森林雨", "远方灯火", "星星印章", "雾松林归来",
    "青禾寻找住处", "青禾送来种子", "小雨丢了线索", "小雨找到道路",
    "白叔的推车", "白叔的交易", "阿团想下厨", "阿团留下了",
    "阿瑶画下驿站", "阿瑶的第二幅画", "灰影来到门前", "灰影的印章",
};

static const localized_text_t s_quest_objectives[10] = {
    { "前往计划，确认事件", "OPEN SCHEDULE AND CONFIRM" },
    { "制作1份热面包", "MAKE 1 HOT BREAD" },
    { "收获2份作物", "HARVEST 2 CROPS" },
    { "完成森林探索", "COMPLETE 1 FOREST RUN" },
    { "完成客房建设", "REPAIR THE GUEST ROOM" },
    { "制作1份胡萝卜炖菜", "MAKE 1 CARROT STEW" },
    { "完成3份料理", "COMPLETE 3 DISHES" },
    { "春7日参加路边集市", "VISIT THE MARKET ON SPRING 7" },
    { "完成旧路牌建设", "REPAIR THE OLD SIGNPOST" },
    { "完成旅行，参加灯会", "TRAVEL ONCE AND JOIN THE FESTIVAL" },
};

static const char *s_dialogue_zh[4][6] = {
    {
        "朝阳把旧屋顶照得发亮。", "走这段路正适合带个热面包。",
        "今天森林小路开放吗？", "多年前我来过这间驿站。",
        "山坡上也能看见这里的灯。", "晴朗夜晚很适合赶路。",
    },
    {
        "云让田野泛起银光。", "去下一座村庄前，我想歇歇。",
        "蘑菇喜欢这样的天气。", "灰色天空下，道路很安静。",
        "你的路牌照亮了路口。", "也许晚些时候能看见星星。",
    },
    {
        "能让我在门外抖掉雨水吗？", "比起再走一里路，我更想喝茶。",
        "庄稼一定很欢迎这场雨。", "我在湿松林外听见了铃声。",
        "是窗边灯光引我来到这里。", "谢谢你一直为旅人留着门。",
    },
    {
        "风差点把我的地图卷走！", "能让我住到风暴过去吗？",
        "老树都被风吹弯了。", "路上的每个人都需要避雨处。",
        "这盏灯比雷声更勇敢。", "等河水平静，我再出发。",
    },
};

static const char *localized_at(const localized_text_t *items, size_t count,
                                size_t index, app_language_t language)
{
    if (index >= count) return language == APP_LANG_EN ? "UNKNOWN" : "未知";
    return app_i18n_pick(language, items[index].zh_cn, items[index].en);
}

const char *app_i18n_weather(app_language_t language, game_weather_t weather)
{
    return localized_at(s_weather, sizeof(s_weather) / sizeof(s_weather[0]),
                        weather, language);
}

const char *app_i18n_crop(app_language_t language, game_crop_t crop)
{
    return localized_at(s_crops, GAME_CROP_COUNT, crop, language);
}

const char *app_i18n_recipe(app_language_t language, game_recipe_t recipe)
{
    return localized_at(s_recipes, GAME_RECIPE_COUNT, recipe, language);
}

const char *app_i18n_building(app_language_t language, game_building_t building)
{
    return localized_at(s_buildings, GAME_BUILD_COUNT, building, language);
}

const char *app_i18n_pet(app_language_t language, game_pet_id_t pet)
{
    return localized_at(s_pet_names, GAME_PET_COUNT, pet, language);
}

const char *app_i18n_pet_species(app_language_t language, game_pet_id_t pet)
{
    return localized_at(s_pet_species, GAME_PET_COUNT, pet, language);
}

const char *app_i18n_pet_personality(app_language_t language, game_pet_id_t pet)
{
    return localized_at(s_pet_personality, GAME_PET_COUNT, pet, language);
}

const char *app_i18n_event(app_language_t language, uint8_t event_id)
{
    const game_event_definition_t *event = game_event_definition(event_id);
    if (!event) return language == APP_LANG_EN ? "UNKNOWN EVENT" : "未知事件";
    return language == APP_LANG_EN ? event->title : s_event_zh[event_id];
}

const char *app_i18n_quest_objective(app_language_t language,
                                     uint8_t quest_stage)
{
    if (quest_stage < 1U || quest_stage > 10U) {
        return language == APP_LANG_EN ? "CHAPTER COMPLETE" : "第一章已完成";
    }
    return localized_at(s_quest_objectives, 10U, quest_stage - 1U, language);
}

const char *app_i18n_dialogue(app_language_t language, game_weather_t weather,
                              uint8_t period, uint32_t seed)
{
    if (language == APP_LANG_EN) {
        return game_traveler_dialogue(weather, period, seed);
    }
    if (weather > GAME_WEATHER_STORM) weather = GAME_WEATHER_CLEAR;
    uint8_t base = (uint8_t)((period % 4U) * 2U);
    uint8_t variant = (uint8_t)(seed & 1U);
    return s_dialogue_zh[weather][(base + variant) % 6U];
}
