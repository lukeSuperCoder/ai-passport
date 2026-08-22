#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "app_logic.h"

static app_notif_item_t make_item(const char *title, const char *msg, uint8_t prio)
{
    app_notif_item_t it;
    memset(&it, 0, sizeof(it));
    if (title) {
        size_t n = strlen(title);
        if (n >= sizeof(it.title)) n = sizeof(it.title) - 1;
        memcpy(it.title, title, n);
    }
    if (msg) {
        size_t n = strlen(msg);
        if (n >= sizeof(it.message)) n = sizeof(it.message) - 1;
        memcpy(it.message, msg, n);
    }
    it.prio = prio;
    return it;
}

int main(void)
{
    char t[16];
    app_time_format(8, 22, 1, 9, t, sizeof(t));
    assert(strcmp(t, "8/22 1:09") == 0);
    app_time_format(12, 3, 0, 5, t, sizeof(t));
    assert(strcmp(t, "12/3 0:05") == 0);

    assert(app_ancs_date_text("20140915T173018", t, sizeof(t)));
    assert(strcmp(t, "9/15 17:30") == 0);
    assert(app_ancs_date_text("20260822T010905", t, sizeof(t)));
    assert(strcmp(t, "8/22 1:09") == 0);
    assert(!app_ancs_date_text("", t, sizeof(t)));
    assert(!app_ancs_date_text("20140915", t, sizeof(t)));
    assert(!app_ancs_date_text("20141315T173018", t, sizeof(t)));
    assert(!app_ancs_date_text(NULL, t, sizeof(t)));

    assert(!app_notif_show_subtitle("Mom", ""));
    assert(!app_notif_show_subtitle("Mom", NULL));
    assert(!app_notif_show_subtitle("Mom", "Mom"));
    assert(app_notif_show_subtitle("Mom", "Family"));
    assert(app_notif_show_subtitle("", "Family"));

    app_kw_t kws[3];
    memset(kws, 0, sizeof(kws));
    assert(app_kw_match("hello", kws, 0) == APP_PRIO_NORMAL);
    assert(app_kw_match("hello", kws, 3) == -1);

    strcpy(kws[0].text, "code");
    kws[0].prio = APP_PRIO_NORMAL;
    strcpy(kws[1].text, "otp");
    kws[1].prio = APP_PRIO_HIGH;
    assert(app_kw_match("your CODE is 1", kws, 2) == APP_PRIO_NORMAL);
    assert(app_kw_match("OTP 99", kws, 2) == APP_PRIO_HIGH);
    assert(app_kw_match("code and otp", kws, 2) == APP_PRIO_HIGH);
    assert(app_kw_match("nothing", kws, 2) == -1);

    strcpy(kws[2].text, "\xE9\xAA\x8C\xE8\xAF\x81\xE7\xA0\x81");
    kws[2].prio = APP_PRIO_HIGH;
    assert(app_kw_match("\xE9\xAA\x8C\xE8\xAF\x81\xE7\xA0\x81 12", kws, 3) == APP_PRIO_HIGH);

    app_notif_q_t q;
    app_notif_q_init(&q);
    assert(app_notif_q_count(&q) == 0);
    assert(app_notif_q_front(&q) == NULL);
    app_notif_item_t a = make_item("a", "one", APP_PRIO_NORMAL);
    strcpy(a.subtitle, "sub");
    strcpy(a.app_name, "Mail");
    strcpy(a.date, "20140915T173018");
    assert(app_notif_q_push(&q, &a));
    app_notif_item_t b = make_item("b", "two", APP_PRIO_HIGH);
    assert(app_notif_q_push(&q, &b));
    assert(app_notif_q_count(&q) == 2);
    assert(strcmp(app_notif_q_front(&q)->title, "a") == 0);
    assert(strcmp(app_notif_q_front(&q)->subtitle, "sub") == 0);
    assert(strcmp(app_notif_q_front(&q)->app_name, "Mail") == 0);
    assert(strcmp(app_notif_q_front(&q)->date, "20140915T173018") == 0);
    app_notif_q_pop(&q);
    assert(strcmp(app_notif_q_front(&q)->title, "b") == 0);
    assert(app_notif_q_front(&q)->prio == APP_PRIO_HIGH);
    app_notif_q_pop(&q);
    assert(app_notif_q_count(&q) == 0);

    for (int i = 0; i < APP_NOTIF_Q + 2; i++) {
        char title[8];
        title[0] = (char)('A' + i);
        title[1] = 0;
        app_notif_item_t it = make_item(title, "m", APP_PRIO_NORMAL);
        app_notif_q_push(&q, &it);
    }
    assert(app_notif_q_count(&q) == APP_NOTIF_Q);
    assert(app_notif_q_front(&q)->title[0] == 'C');

    app_log_t log;
    app_log_init(&log);
    assert(app_log_count(&log) == 0);
    assert(app_log_at(&log, 0) == NULL);
    assert(!app_log_push(&log, NULL));

    app_log_item_t rec;
    memset(&rec, 0, sizeof(rec));
    strcpy(rec.app_id, "com.tencent.xin");
    strcpy(rec.app_name, "WeChat");
    strcpy(rec.title, "Hi");
    strcpy(rec.message, "hello");
    strcpy(rec.date, "20260822T140300");
    rec.category = 4;
    assert(app_log_push(&log, &rec));

    memset(&rec, 0, sizeof(rec));
    strcpy(rec.app_id, "com.apple.MobileSMS");
    strcpy(rec.title, "code");
    rec.category = 4;
    assert(app_log_push(&log, &rec));

    memset(&rec, 0, sizeof(rec));
    strcpy(rec.app_id, "com.tencent.xin");
    strcpy(rec.app_name, "WeChat");
    strcpy(rec.title, "Two");
    rec.category = 6;
    assert(app_log_push(&log, &rec));

    assert(app_log_count(&log) == 3);
    assert(strcmp(app_log_at(&log, 0)->title, "Two") == 0);
    assert(strcmp(app_log_at(&log, 2)->title, "Hi") == 0);
    assert(strcmp(app_log_app_key(app_log_at(&log, 1)), "com.apple.MobileSMS") == 0);

    char label[24];
    app_log_app_label(app_log_at(&log, 0), label, sizeof(label));
    assert(strcmp(label, "WeChat") == 0);
    app_log_app_label(app_log_at(&log, 1), label, sizeof(label));
    assert(strcmp(label, "MobileSMS") == 0);

    app_log_group_t groups[8];
    int gn = app_log_apps(&log, groups, 8);
    assert(gn == 2);
    assert(strcmp(groups[0].app_id, "com.tencent.xin") == 0);
    assert(groups[0].count == 2);
    assert(strcmp(groups[1].app_id, "com.apple.MobileSMS") == 0);
    assert(groups[1].count == 1);

    gn = app_log_cats(&log, groups, 8);
    assert(gn == 2);
    assert(groups[0].category == 6 && groups[0].count == 1);
    assert(groups[1].category == 4 && groups[1].count == 2);

    int idx[8];
    assert(app_log_match_app(&log, "com.tencent.xin", idx, 8) == 2);
    assert(idx[0] == 0 && idx[1] == 2);
    assert(app_log_match_cat(&log, 4, idx, 8) == 2);

    app_log_t rm;
    app_log_init(&rm);
    for (int i = 0; i < 5; i++) {
        memset(&rec, 0, sizeof(rec));
        rec.title[0] = (char)('A' + i);
        rec.title[1] = 0;
        app_log_push(&rm, &rec);
    }
    assert(app_log_at(&rm, 0)->title[0] == 'E');
    assert(app_log_remove(&rm, 2));
    assert(app_log_count(&rm) == 4);
    assert(app_log_at(&rm, 0)->title[0] == 'E');
    assert(app_log_at(&rm, 1)->title[0] == 'D');
    assert(app_log_at(&rm, 2)->title[0] == 'B');
    assert(app_log_at(&rm, 3)->title[0] == 'A');
    assert(app_log_remove(&rm, 0));
    assert(app_log_at(&rm, 0)->title[0] == 'D');
    assert(app_log_remove(&rm, 2));
    assert(app_log_count(&rm) == 2);
    assert(app_log_at(&rm, 0)->title[0] == 'D');
    assert(app_log_at(&rm, 1)->title[0] == 'B');
    assert(!app_log_remove(&rm, 2));
    assert(app_log_remove(&rm, 0));
    assert(app_log_remove(&rm, 0));
    assert(app_log_count(&rm) == 0);
    assert(!app_log_remove(&rm, 0));

    app_log_init(&rm);
    for (int i = 0; i < APP_LOG_N + 2; i++) {
        memset(&rec, 0, sizeof(rec));
        rec.title[0] = (char)('A' + (i % 26));
        rec.title[1] = 0;
        app_log_push(&rm, &rec);
    }
    char newest = app_log_at(&rm, 0)->title[0];
    char oldest = app_log_at(&rm, APP_LOG_N - 1)->title[0];
    assert(app_log_remove(&rm, 0));
    assert(app_log_count(&rm) == APP_LOG_N - 1);
    assert(app_log_at(&rm, 0)->title[0] != newest);
    assert(app_log_remove(&rm, app_log_count(&rm) - 1));
    assert(app_log_count(&rm) == APP_LOG_N - 2);
    assert(app_log_at(&rm, app_log_count(&rm) - 1)->title[0] != oldest);

    for (int i = 0; i < APP_LOG_N + 3; i++) {
        memset(&rec, 0, sizeof(rec));
        rec.title[0] = (char)('A' + (i % 26));
        rec.category = (uint8_t)(i % 3);
        app_log_push(&log, &rec);
    }
    assert(app_log_count(&log) == APP_LOG_N);
    assert(app_log_at(&log, 0)->title[0] == (char)('A' + ((APP_LOG_N + 2) % 26)));

    app_log_clear(&log);
    assert(app_log_count(&log) == 0);
    assert(app_log_apps(&log, groups, 8) == 0);

    uint8_t key[32];
    int kn = app_b32_decode("GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ", key, sizeof(key));
    assert(kn == 20);
    assert(memcmp(key, "12345678901234567890", 20) == 0);
    assert(app_hotp(key, 20, 0, 6) == 755224);
    assert(app_hotp(key, 20, 1, 6) == 287082);

    app_totp_acct_t acct;
    memset(&acct, 0, sizeof(acct));
    strcpy(acct.secret, "GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ");
    acct.digits = 8;
    acct.period = 30;
    char code[12];
    int remain = -1;
    assert(app_totp_code(&acct, 59, code, sizeof(code), &remain));
    assert(strcmp(code, "94287082") == 0);
    assert(remain == 1);
    assert(app_totp_code(&acct, 1111111109, code, sizeof(code), &remain));
    assert(strcmp(code, "07081804") == 0);
    assert(app_totp_code(&acct, 1111111111, code, sizeof(code), NULL));
    assert(strcmp(code, "14050471") == 0);
    assert(app_totp_code(&acct, 1234567890, code, sizeof(code), NULL));
    assert(strcmp(code, "89005924") == 0);
    assert(app_totp_code(&acct, 2000000000, code, sizeof(code), NULL));
    assert(strcmp(code, "69279037") == 0);

    char pretty[16];
    app_totp_format_code("123456", pretty, sizeof(pretty));
    assert(strcmp(pretty, "123 456") == 0);
    app_totp_format_code("12345678", pretty, sizeof(pretty));
    assert(strcmp(pretty, "1234 5678") == 0);
    app_totp_mask("JBSWY3DPEHPK3PXP", pretty, sizeof(pretty));
    assert(strcmp(pretty, "****3PXP") == 0);

    memset(&acct, 0, sizeof(acct));
    assert(app_totp_ingest(" jbswy3dpehpk3pxp ", &acct, true));
    assert(strcmp(acct.secret, "JBSWY3DPEHPK3PXP") == 0);
    assert(acct.digits == 6 && acct.period == 30);

    memset(&acct, 0, sizeof(acct));
    assert(app_totp_ingest(
        "otpauth://totp/ACME:john%40ex.com?secret=GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ"
        "&issuer=ACME&digits=8&period=30&algorithm=SHA1",
        &acct, true));
    assert(strcmp(acct.issuer, "ACME") == 0);
    assert(strcmp(acct.label, "john@ex.com") == 0);
    assert(acct.digits == 8 && acct.period == 30);
    assert(app_totp_code(&acct, 59, code, sizeof(code), NULL));
    assert(strcmp(code, "94287082") == 0);

    memset(&acct, 0, sizeof(acct));
    strcpy(acct.issuer, "Keep");
    strcpy(acct.label, "me");
    assert(app_totp_ingest("otpauth://totp/Other?secret=JBSWY3DPEHPK3PXP",
                           &acct, false));
    assert(strcmp(acct.issuer, "Keep") == 0);
    assert(strcmp(acct.label, "me") == 0);

    memset(&acct, 0, sizeof(acct));
    assert(!app_totp_ingest(
        "otpauth://totp/X?secret=JBSWY3DPEHPK3PXP&algorithm=SHA256",
        &acct, true));
    assert(!app_totp_ingest("not-base32!!", &acct, false));

    char iss[24], lab[32];
    app_totp_split_name("GitHub:alice", iss, sizeof(iss), lab, sizeof(lab));
    assert(strcmp(iss, "GitHub") == 0);
    assert(strcmp(lab, "alice") == 0);
    app_totp_split_name("Google", iss, sizeof(iss), lab, sizeof(lab));
    assert(strcmp(iss, "Google") == 0);
    assert(lab[0] == 0);

    app_totp_list_t list;
    app_totp_list_init(&list);
    app_totp_acct_t a1;
    memset(&a1, 0, sizeof(a1));
    strcpy(a1.issuer, "GitHub");
    strcpy(a1.label, "bob");
    strcpy(a1.secret, "JBSWY3DPEHPK3PXP");
    assert(app_totp_list_add(&list, &a1));
    strcpy(a1.label, "alice");
    assert(app_totp_list_add(&list, &a1));
    strcpy(a1.issuer, "Google");
    strcpy(a1.label, "work");
    assert(app_totp_list_add(&list, &a1));
    for (int i = 0; i < 10; i++) {
        a1.label[0] = (char)('0' + i);
        a1.label[1] = 0;
        assert(app_totp_list_add(&list, &a1));
    }
    assert(list.n == 13);
    assert(strcmp(list.items[0].issuer, "GitHub") == 0);
    assert(strcmp(list.items[0].label, "alice") == 0);
    assert(strcmp(list.items[1].label, "bob") == 0);
    assert(app_totp_same_group(&list.items[0], &list.items[1]));
    assert(!app_totp_same_group(&list.items[0], &list.items[2]));
    strcpy(a1.issuer, "GitHub");
    strcpy(a1.label, "bob");
    assert(app_totp_list_find(&list, &a1) == 1);
    assert(app_totp_list_delete(&list, 0));
    assert(list.n == 12);
    app_totp_list_clear(&list);
    assert(list.n == 0);
    assert(list.items == NULL);

    app_dlog_t dlog;
    app_dlog_init(&dlog);
    assert(app_dlog_count(&dlog) == 0);
    char line[APP_DLOG_W];
    app_dlog_copy(&dlog, 0, line, sizeof(line));
    assert(line[0] == 0);

    app_dlog_feed(&dlog, "hello\n", 6);
    assert(app_dlog_count(&dlog) == 1);
    app_dlog_copy(&dlog, 0, line, sizeof(line));
    assert(strcmp(line, "hello") == 0);
    assert(!app_dlog_cont(&dlog, 0));

    app_dlog_feed(&dlog, "ab", 2);
    assert(app_dlog_count(&dlog) == 1);
    app_dlog_feed(&dlog, "c\n", 2);
    assert(app_dlog_count(&dlog) == 2);
    app_dlog_copy(&dlog, 1, line, sizeof(line));
    assert(strcmp(line, "abc") == 0);
    assert(!app_dlog_cont(&dlog, 1));

    const char *ansi = "\033[0;32mI (1) t: x\033[0m\n";
    app_dlog_feed(&dlog, ansi, (int)strlen(ansi));
    assert(app_dlog_count(&dlog) == 3);
    app_dlog_copy(&dlog, 2, line, sizeof(line));
    assert(strcmp(line, "I (1) t: x") == 0);

    char longl[APP_DLOG_W + 8];
    memset(longl, 'A', sizeof(longl) - 2);
    longl[sizeof(longl) - 2] = '\n';
    longl[sizeof(longl) - 1] = 0;
    int before = app_dlog_count(&dlog);
    app_dlog_feed(&dlog, longl, (int)strlen(longl));
    assert(app_dlog_count(&dlog) == before + 2);
    assert(!app_dlog_cont(&dlog, before));
    assert(app_dlog_cont(&dlog, before + 1));

    app_dlog_feed(&dlog, "tail", 4);
    app_dlog_flush(&dlog);
    app_dlog_copy(&dlog, app_dlog_count(&dlog) - 1, line, sizeof(line));
    assert(strcmp(line, "tail") == 0);
    assert(!app_dlog_cont(&dlog, app_dlog_count(&dlog) - 1));

    for (int i = 0; i < APP_DLOG_N + 5; i++) {
        char one[8];
        snprintf(one, sizeof(one), "%d\n", i);
        app_dlog_feed(&dlog, one, (int)strlen(one));
    }
    assert(app_dlog_count(&dlog) == APP_DLOG_N);
    app_dlog_copy(&dlog, APP_DLOG_N - 1, line, sizeof(line));
    char expect[16];
    snprintf(expect, sizeof(expect), "%d", APP_DLOG_N + 4);
    assert(strcmp(line, expect) == 0);

    app_dlog_clear(&dlog);
    assert(app_dlog_count(&dlog) == 0);

    return 0;
}
