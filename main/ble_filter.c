// 纯逻辑:短信 App 识别、关键字匹配、验证码抽取。不依赖 ESP-IDF,可主机测试。
#include "ble_filter.h"

#include <string.h>

static const char *const SMS_IDS[] = {
    "com.apple.MobileSMS",
    "com.apple.messages",
};

static const char *const KEYWORDS[] = {
    "code",
    "otp",
    "verify",
    "verification",
    "pin",
    "password",
    "passwd",
    "\xE9\xAA\x8C\xE8\xAF\x81\xE7\xA0\x81",           // 验证码
    "\xE6\xA0\xA1\xE9\xAA\x8C\xE7\xA0\x81",           // 校验码
    "\xE5\x8A\xA8\xE6\x80\x81\xE7\xA0\x81",           // 动态码
};

static char ascii_lower(char c) {
    if (c >= 'A' && c <= 'Z') return (char)(c - 'A' + 'a');
    return c;
}

static bool has_non_ascii(const char *s) {
    for (; *s; s++) {
        if ((unsigned char)*s >= 0x80) return true;
    }
    return false;
}

static bool contains(const char *hay, const char *needle) {
    if (!hay || !needle || !needle[0]) return false;
    if (has_non_ascii(needle)) return strstr(hay, needle) != NULL;

    size_t nlen = strlen(needle);
    for (const char *p = hay; *p; p++) {
        size_t i = 0;
        while (i < nlen && p[i] && ascii_lower(p[i]) == ascii_lower(needle[i])) i++;
        if (i == nlen) return true;
    }
    return false;
}

bool ble_filter_is_sms(const char *app_id) {
    if (!app_id || !app_id[0]) return false;
    for (size_t i = 0; i < sizeof(SMS_IDS) / sizeof(SMS_IDS[0]); i++) {
        if (strcmp(app_id, SMS_IDS[i]) == 0) return true;
    }
    return strstr(app_id, "MobileSMS") != NULL;
}

bool ble_filter_has_keyword(const char *text, const char *extra_kw) {
    if (!text || !text[0]) return false;
    for (size_t i = 0; i < sizeof(KEYWORDS) / sizeof(KEYWORDS[0]); i++) {
        if (contains(text, KEYWORDS[i])) return true;
    }
    if (extra_kw && extra_kw[0] && contains(text, extra_kw)) return true;
    return false;
}

void ble_filter_extract_code(const char *msg, char *out, size_t n) {
    if (!out || n == 0) return;
    out[0] = 0;
    if (!msg) return;

    const char *best = NULL;
    int best_len = 0;
    const char *p = msg;
    while (*p) {
        if (*p >= '0' && *p <= '9') {
            const char *start = p;
            int len = 0;
            while (p[len] >= '0' && p[len] <= '9') len++;
            if (len >= 4 && len <= 8) {
                int score = (len == 6) ? 100 : len;
                int best_score = (best_len == 6) ? 100 : best_len;
                if (!best || score > best_score) {
                    best = start;
                    best_len = len;
                }
            }
            p += len;
        } else {
            p++;
        }
    }
    if (!best || (size_t)best_len + 1 > n) return;
    memcpy(out, best, (size_t)best_len);
    out[best_len] = 0;
}

void ble_filter_pick_code(const char *title, const char *subtitle,
                         const char *message, char *out, size_t n)
{
    if (!out || n == 0) return;
    out[0] = 0;
    if (message && message[0]) ble_filter_extract_code(message, out, n);
    if (!out[0] && title && title[0]) ble_filter_extract_code(title, out, n);
    if (!out[0] && subtitle && subtitle[0]) ble_filter_extract_code(subtitle, out, n);
}

void ble_filter_to_latin(const char *in, char *out, size_t n) {
    if (!out || n == 0) return;
    out[0] = 0;
    if (!in) return;

    size_t o = 0;
    for (size_t i = 0; in[i] && o + 1 < n; ) {
        unsigned char c = (unsigned char)in[i];
        if (c < 0x80) {
            out[o++] = (char)c;
            i++;
            continue;
        }
        // 跳过一个 UTF-8 码点,用 '?' 占位,避免把中文当拉丁字母切开。
        int skip = 1;
        if ((c & 0xE0) == 0xC0) skip = 2;
        else if ((c & 0xF0) == 0xE0) skip = 3;
        else if ((c & 0xF8) == 0xF0) skip = 4;
        i += (size_t)skip;
        out[o++] = '?';
    }
    out[o] = 0;
}
