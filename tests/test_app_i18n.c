#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "app_i18n.h"

int main(void)
{
    assert(app_lang() == APP_LANG_ZH);
    assert(strcmp(app_str(APP_STR_ON), "开") == 0);
    assert(strcmp(app_str(APP_STR_HOME_CODES), "验证器") == 0);
    assert(strcmp(app_str_onoff(false), "关") == 0);
    assert(strcmp(app_lang_name(APP_LANG_EN), "English") == 0);
    assert(strcmp(app_lang_html(), "zh-CN") == 0);

    app_lang_set(APP_LANG_ZH);
    assert(app_lang() == APP_LANG_ZH);
    assert(strcmp(app_str(APP_STR_ON), "开") == 0);
    assert(strcmp(app_str(APP_STR_HOME_SETTINGS), "设置") == 0);
    assert(strstr(app_lang_name(APP_LANG_ZH), "中文") != NULL);
    assert(strcmp(app_lang_html(), "zh-CN") == 0);
    assert(strchr(app_str(APP_STR_CONNECTING), '%') != NULL);
    assert(strchr(app_str(APP_STR_WEB_BUSY), '%') != NULL);
    assert(strcmp(app_str(APP_STR_LOG), "调试日志") == 0);
    assert(strcmp(app_str(APP_STR_LOG_NONE), "暂无日志") == 0);
    assert(strcmp(app_str(APP_STR_CAT_SOCIAL), "社交") == 0);
    assert(strcmp(app_str(APP_STR_HOME_CODES), "验证器") == 0);
    assert(strcmp(app_str(APP_STR_HOME_WALKIE), "对讲机") == 0);
    assert(strcmp(app_str(APP_STR_WX_TODAY), "今天") == 0);
    assert(strcmp(app_str(APP_STR_WX_TOMORROW), "明天") == 0);
    assert(strcmp(app_str(APP_STR_HINT_OPEN), "确定进入  长按返回") == 0);
    assert(strcmp(app_str(APP_STR_HOLD_DELETE), "长按上键删除") == 0);
    assert(strcmp(app_str(APP_STR_WALKIE_START), "开始") == 0);
    assert(strchr(app_str(APP_STR_TOTP_CONFIRM), '%') != NULL);
    assert(strcmp(app_str(APP_STR_TAMA_DARK), "灯关了") == 0);
    assert(strcmp(app_str(APP_STR_TAMA_LIT), "灯开了") == 0);
    assert(strcmp(app_str(APP_STR_TAMA_FEED), "喂食") == 0);
    assert(strcmp(app_str(APP_STR_TAMA_PLAY), "玩耍") == 0);
    assert(strcmp(app_str(APP_STR_TAMA_DRINK), "喝水") == 0);
    assert(strcmp(app_str(APP_STR_TAMA_BACK), "返回") == 0);
    assert(strcmp(app_str(APP_STR_TAMA_CAT_FOOD), "吃喝") == 0);
    assert(strcmp(app_str(APP_STR_TAMA_WORLD), "世界") == 0);
    assert(strcmp(app_str(APP_STR_TAMA_VISIT), "拜访") == 0);
    assert(strcmp(app_str(APP_STR_TAMA_FIGHT), "对战") == 0);
    assert(strcmp(app_str(APP_STR_TAMA_NEED_BT), "需要蓝牙") == 0);
    assert(strcmp(app_str(APP_STR_TAMA_BAG), "背包") == 0);
    assert(strcmp(app_str(APP_STR_TAMA_TAB_SHOP), "背包") == 0);
    assert(strcmp(app_str(APP_STR_TAMA_IT_SOAP), "沐浴露") == 0);
    assert(strcmp(app_str(APP_STR_TAMA_WEAR), "药品") == 0);
    assert(strcmp(app_str(APP_STR_TAMA_GEAR), "道具") == 0);
    assert(strcmp(app_str(APP_STR_TAMA_GD0), "汉堡") == 0);
    assert(strcmp(app_str(APP_STR_TAMA_GD4), "矿泉水") == 0);
    assert(strcmp(app_str(APP_STR_TAMA_GD14), "金铲铲") == 0);
    assert(strcmp(app_str(APP_STR_TAMA_EMPTY), "没有材料") == 0);
    assert(strcmp(app_str(APP_STR_TAMA_WARN), "预警") == 0);
    assert(strcmp(app_str(APP_STR_TAMA_HINT), "上下翻页  确定进入") == 0);
    assert(strcmp(app_str(APP_STR_TAMA_TAB_HOME), "主页") == 0);
    assert(strcmp(app_str(APP_STR_TAMA_TAB_SET), "设置") == 0);
    assert(strcmp(app_str(APP_STR_TAMA_STAT_FOOD), "饱食") == 0);
    assert(strcmp(app_str(APP_STR_TAMA_STAT_HEALTH), "健康") == 0);
    assert(strcmp(app_str(APP_STR_TAMA_SP1), "团团") == 0);
    assert(strcmp(app_str(APP_STR_TAMA_BEAT), "节奏大师") == 0);
    assert(strcmp(app_str(APP_STR_TAMA_FISH), "接住小鱼") == 0);
    assert(strcmp(app_str(APP_STR_TAMA_RESULT), "结算") == 0);
    assert(strcmp(app_str(APP_STR_TAMA_CONFIRM), "确定") == 0);
    assert(strcmp(app_str(APP_STR_TAMA_MISS), "没中") == 0);
    assert(strcmp(app_str(APP_STR_TAMA_DEX_PET), "宠物") == 0);
    assert(strcmp(app_str(APP_STR_TAMA_DEX_ITM), "道具") == 0);
    assert(strcmp(app_str(APP_STR_TAMA_DEX_COL), "收藏") == 0);
    assert(strcmp(app_str(APP_STR_TAMA_AH0), "破壳") == 0);
    assert(strcmp(app_str(APP_STR_TAMA_ACH), "成就") == 0);
    assert(strcmp(app_str(APP_STR_UPDATE), "更新") == 0);
    assert(strcmp(app_str(APP_STR_OTA_INSTALL), "安装") == 0);
    assert(strchr(app_str(APP_STR_OTA_APPLYING), '%') != NULL);
    assert(strcmp(app_str(APP_STR_TAMA_TRIP), "背包旅行") == 0);
    assert(strcmp(app_str(APP_STR_TAMA_TRIP_BACK), "回来了") == 0);
    assert(strcmp(app_str(APP_STR_TAMA_SV0), "贝壳") == 0);
    assert(strcmp(app_str(APP_STR_TAMA_USED), "用 %s x%d") == 0);
    assert(strcmp(app_str(APP_STR_TAMA_NAME), "名字") == 0);
    assert(strcmp(app_str(APP_STR_TAMA_KIT), "小猫快跑") == 0);
    assert(strcmp(app_str(APP_STR_TAMA_MAT), "三消") == 0);
    assert(strcmp(app_str(APP_STR_TAMA_HOLD_BACK), "长按确定全局返回") == 0);
    assert(strcmp(app_str(APP_STR_TAMA_WIPE), "重置游戏") == 0);
    assert(strstr(app_str(APP_STR_TAMA_DX_EGG), "还在里面打呼噜") != NULL);
    assert(strstr(app_str(APP_STR_TAMA_DX_GD0), "油滋滋，吃完去遛弯") != NULL);
    assert(strstr(app_str(APP_STR_TAMA_DX_SV0), "海里寄来的明信片") != NULL);
    assert(strchr(app_str(APP_STR_TAMA_DX_EGG), '\n') != NULL);

    app_lang_set(APP_LANG_EN);
    assert(strcmp(app_str(APP_STR_TAMA_DEX_ITM), "Items") == 0);
    assert(strcmp(app_str(APP_STR_UPDATE), "Update") == 0);
    assert(strcmp(app_str(APP_STR_TAMA), "Pet") == 0);
    assert(strcmp(app_str(APP_STR_TAMA_HINT), "UP/DOWN pages  OK open") == 0);
    assert(strcmp(app_str(APP_STR_TAMA_SUB_HINT), "OK do  hold OK back") == 0);
    assert(strcmp(app_str(APP_STR_TAMA_USED), "Used %s x%d") == 0);
    assert(strcmp(app_str(APP_STR_TAMA_KIT), "Kitten Run") == 0);
    assert(strcmp(app_str(APP_STR_TAMA_MAT), "Match 3") == 0);
    assert(strstr(app_str(APP_STR_TAMA_DX_EGG), "Still napping inside") != NULL);
    assert(strstr(app_str(APP_STR_TAMA_DX_GD14), "Dig once, luck twice") != NULL);
    assert(strcmp(app_str(APP_STR_TAMA_DARK), "lights off") == 0);
    assert(strcmp(app_str(APP_STR_TAMA_LIT), "lights on") == 0);

    app_lang_set((app_lang_t)99);
    assert(app_lang() == APP_LANG_EN);
    assert(app_str(APP_STR_COUNT)[0] == 0);
    puts("ok");
    return 0;
}
