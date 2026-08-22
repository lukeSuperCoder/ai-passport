#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "app_i18n.h"

int main(void)
{
    assert(app_lang() == APP_LANG_EN);
    assert(strcmp(app_str(APP_STR_ON), "ON") == 0);
    assert(strcmp(app_str(APP_STR_HOME_CODES), "TOTP") == 0);
    assert(strcmp(app_str_onoff(false), "OFF") == 0);
    assert(strcmp(app_lang_name(APP_LANG_EN), "English") == 0);
    assert(strcmp(app_lang_html(), "en") == 0);

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

    app_lang_set((app_lang_t)99);
    assert(app_lang() == APP_LANG_EN);
    assert(app_str(APP_STR_COUNT)[0] == 0);
    puts("ok");
    return 0;
}
