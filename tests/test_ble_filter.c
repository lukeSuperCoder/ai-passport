#include <assert.h>
#include <string.h>
#include "ble_filter.h"

int main(void)
{
    assert(ble_filter_is_sms("com.apple.MobileSMS"));
    assert(ble_filter_is_sms("com.apple.messages"));
    assert(!ble_filter_is_sms("com.tencent.xin"));
    assert(!ble_filter_is_sms(""));

    assert(ble_filter_has_keyword("Your code is 123456", NULL));
    assert(ble_filter_has_keyword("OTP: 8888", NULL));
    assert(ble_filter_has_keyword("CODE 9999", NULL));
    assert(ble_filter_has_keyword("\xE9\xAA\x8C\xE8\xAF\x81\xE7\xA0\x81 654321", NULL));
    assert(!ble_filter_has_keyword("hello world", NULL));
    assert(ble_filter_has_keyword("hello world", "hello"));

    char code[12];
    ble_filter_extract_code("Your code is 123456. Ignore 12", code, sizeof(code));
    assert(strcmp(code, "123456") == 0);
    ble_filter_extract_code("PIN 99887766 extra", code, sizeof(code));
    assert(strcmp(code, "99887766") == 0);
    ble_filter_extract_code("no digits here", code, sizeof(code));
    assert(code[0] == 0);
    ble_filter_pick_code("title", "", "code 654321", code, sizeof(code));
    assert(strcmp(code, "654321") == 0);
    ble_filter_pick_code("PIN 1111", "x", "", code, sizeof(code));
    assert(strcmp(code, "1111") == 0);

    char latin[32];
    ble_filter_to_latin("ABC\xE9\xAA\x8C""123", latin, sizeof(latin));
    assert(strcmp(latin, "ABC?123") == 0);
    ble_filter_to_latin("ASCII", latin, sizeof(latin));
    assert(strcmp(latin, "ASCII") == 0);
    return 0;
}
