#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define APP_KW_MAX      8
#define APP_KW_LEN      24
#define APP_NOTIF_Q     6
#define APP_PRIO_NORMAL 0
#define APP_PRIO_HIGH   1

typedef struct {
    char text[APP_KW_LEN + 1];
    uint8_t prio;
} app_kw_t;

typedef struct {
    char title[48 + 1];
    char subtitle[48 + 1];
    char message[128 + 1];
    char app_name[32 + 1];
    char date[15 + 1];
    uint8_t prio;
} app_notif_item_t;

// month 1-12. Minutes keep a leading zero; month/day/hour do not.
void app_time_format(int month, int day, int hour, int minute,
                     char *out, size_t n);

// ANCS Date YYYYMMDDTHHMMSS -> "8/22 1:09". false if missing or invalid.
bool app_ancs_date_text(const char *iso, char *out, size_t n);

// Subtitle only when present and not a duplicate of title.
bool app_notif_show_subtitle(const char *title, const char *subtitle);

// -1 = no match. If kw_n==0 every text matches as NORMAL.
int app_kw_match(const char *text, const app_kw_t *kws, int kw_n);

typedef struct {
    app_notif_item_t items[APP_NOTIF_Q];
    int head;
    int count;
} app_notif_q_t;

void app_notif_q_init(app_notif_q_t *q);
bool app_notif_q_push(app_notif_q_t *q, const app_notif_item_t *it);
const app_notif_item_t *app_notif_q_front(const app_notif_q_t *q);
void app_notif_q_pop(app_notif_q_t *q);
int app_notif_q_count(const app_notif_q_t *q);

#define APP_TOTP_ISSUER_LEN 24
#define APP_TOTP_LABEL_LEN  32
#define APP_TOTP_SECRET_LEN 64

typedef struct {
    char issuer[APP_TOTP_ISSUER_LEN + 1];
    char label[APP_TOTP_LABEL_LEN + 1];
    char secret[APP_TOTP_SECRET_LEN + 1];
    uint8_t digits;
    uint8_t period;
} app_totp_acct_t;

typedef struct {
    app_totp_acct_t *items;
    uint16_t n;
    uint16_t cap;
} app_totp_list_t;

// Base32 (RFC 4648). Ignores space, hyphen, and padding. Returns byte count
// or -1 on invalid input / overflow.
int app_b32_decode(const char *in, uint8_t *out, size_t out_n);

uint32_t app_hotp(const uint8_t *key, size_t key_n, uint64_t counter, int digits);

bool app_totp_code(const app_totp_acct_t *a, uint64_t unix_sec,
                   char *out, size_t n, int *remain);

void app_totp_format_code(const char *digits, char *out, size_t n);
void app_totp_mask(const char *secret, char *out, size_t n);

void app_totp_split_name(const char *name, char *issuer, size_t issuer_n,
                         char *label, size_t label_n);

// otpauth://totp/... or a raw Base32 secret. fill_names updates issuer/label.
bool app_totp_ingest(const char *text, app_totp_acct_t *acct, bool fill_names);

void app_totp_list_init(app_totp_list_t *l);
void app_totp_list_clear(app_totp_list_t *l);
bool app_totp_list_add(app_totp_list_t *l, const app_totp_acct_t *a);
bool app_totp_list_update(app_totp_list_t *l, int i, const app_totp_acct_t *a);
bool app_totp_list_delete(app_totp_list_t *l, int i);
void app_totp_list_sort(app_totp_list_t *l);
int app_totp_list_find(const app_totp_list_t *l, const app_totp_acct_t *a);
const char *app_totp_issuer(const app_totp_acct_t *a);
const char *app_totp_label(const app_totp_acct_t *a);
bool app_totp_same_group(const app_totp_acct_t *a, const app_totp_acct_t *b);

#define APP_LOG_N        16
#define APP_LOG_APP_ID   40
#define APP_LOG_APP_NAME 32
#define APP_LOG_TITLE    40
#define APP_LOG_SUB      32
#define APP_LOG_MSG      80
#define APP_LOG_DATE     15
#define APP_CAT_N        12

typedef struct {
    char app_id[APP_LOG_APP_ID + 1];
    char app_name[APP_LOG_APP_NAME + 1];
    char title[APP_LOG_TITLE + 1];
    char subtitle[APP_LOG_SUB + 1];
    char message[APP_LOG_MSG + 1];
    char date[APP_LOG_DATE + 1];
    uint8_t category;
} app_log_item_t;

typedef struct {
    app_log_item_t items[APP_LOG_N];
    int head;
    int count;
} app_log_t;

typedef struct {
    char app_id[APP_LOG_APP_ID + 1];
    char app_name[APP_LOG_APP_NAME + 1];
    uint8_t category;
    int count;
} app_log_group_t;

void app_log_init(app_log_t *log);
bool app_log_push(app_log_t *log, const app_log_item_t *src);
void app_log_clear(app_log_t *log);
int app_log_count(const app_log_t *log);
// newest_i 0 = most recent
const app_log_item_t *app_log_at(const app_log_t *log, int newest_i);
bool app_log_remove(app_log_t *log, int newest_i);
const char *app_log_app_key(const app_log_item_t *it);
void app_log_app_label(const app_log_item_t *it, char *out, size_t n);
int app_log_apps(const app_log_t *log, app_log_group_t *out, int max);
int app_log_cats(const app_log_t *log, app_log_group_t *out, int max);
int app_log_match_app(const app_log_t *log, const char *app_id, int *idx, int max);
int app_log_match_cat(const app_log_t *log, uint8_t category, int *idx, int max);

// 系统调试日志环形缓冲。无 PSRAM,行宽按 200px/14px 字体截断后折行。
#define APP_DLOG_N 48
#define APP_DLOG_W 32

typedef struct {
    char line[APP_DLOG_N][APP_DLOG_W];
    char acc[APP_DLOG_W];
    uint8_t acc_n;
    uint8_t count;
    uint8_t head;
    uint8_t esc;
    uint8_t acc_cont;
    uint8_t cont[APP_DLOG_N];
} app_dlog_t;

void app_dlog_init(app_dlog_t *l);
void app_dlog_clear(app_dlog_t *l);
void app_dlog_feed(app_dlog_t *l, const char *s, int n);
void app_dlog_flush(app_dlog_t *l);
int app_dlog_count(const app_dlog_t *l);
void app_dlog_copy(const app_dlog_t *l, int oldest_i, char *out, size_t n);
bool app_dlog_cont(const app_dlog_t *l, int oldest_i);
