#include "app.h"
#include "app_i18n.h"
#include "app_ui.h"
#include "app_web.h"
#include "bsp_wifi.h"
#include "ui_pixel.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdio.h>
#include <string.h>

typedef enum { VIEW_LIST = 0, VIEW_PASS } view_t;

static lv_obj_t *s_hint, *s_body;
static lv_timer_t *s_timer, *s_hold_timer;
static TaskHandle_t s_task;
static volatile int s_req;
static volatile bool s_scanning;
static view_t s_view;
static bsp_wifi_ap_t s_aps[BSP_WIFI_SCAN_MAX];
static int s_ap_count;
static int s_sel;
static int s_kb_sel, s_kb_set;
static char s_pick[BSP_WIFI_SSID_MAX + 1];
static char s_pass[BSP_WIFI_PASS_MAX + 1];
static int s_hold_btn = -1;
static int s_hold_ms;

static bool web_ready(void)
{
    char url[36];
    return app_web_url(url, sizeof(url));
}

static int extra_count(void)
{
    int n = 2; // Power, Auto
    if (web_ready()) n++; // QR
    n += s_ap_count;
    n += 1; // Rescan
    if (bsp_wifi_has_saved()) n++;
    return n;
}

static int ap_base(void)
{
    return web_ready() ? 3 : 2;
}

static void mask_pass(char *out, size_t n)
{
    size_t len = strlen(s_pass);
    if (len == 0) {
        snprintf(out, n, "%s", app_str(APP_STR_EMPTY));
        return;
    }
    size_t i = 0;
    for (; i + 1 < len && i + 1 < n; i++) out[i] = '*';
    if (i < n - 1) out[i++] = s_pass[len - 1];
    out[i] = 0;
}

static void render_list(char *out, size_t n)
{
    int items = extra_count();
    if (s_sel >= items) s_sel = items ? items - 1 : 0;
    int window = 8;
    int start = s_sel - window / 2;
    if (start < 0) start = 0;
    if (start + window > items) start = items > window ? items - window : 0;

    size_t used = 0;
    out[0] = 0;
    for (int i = start; i < items && i < start + window; i++) {
        char line[80];
        const char *m = (i == s_sel) ? ">" : " ";
        if (i == 0) {
            snprintf(line, sizeof(line), "%s %s  %s\n", m,
                     app_str(APP_STR_POWER), app_str_onoff(bsp_wifi_enabled()));
        } else if (i == 1) {
            snprintf(line, sizeof(line), "%s %s  %s\n", m,
                     app_str(APP_STR_AUTO), app_str_onoff(bsp_wifi_auto_connect()));
        } else if (web_ready() && i == 2) {
            snprintf(line, sizeof(line), "%s %s\n", m, app_str(APP_STR_QR));
        } else if (i < ap_base() + s_ap_count) {
            int a = i - ap_base();
            char ssid[40];
            ui_pixel_utf8_copy(ssid, sizeof(ssid), s_aps[a].ssid);
            bool on = (bsp_wifi_state() == BSP_WIFI_CONNECTED
                       && strcmp(s_aps[a].ssid, bsp_wifi_ssid()) == 0);
            if (s_aps[a].rssi != 0) {
                snprintf(line, sizeof(line), "%s%s%s %ddB%s\n",
                         m, s_aps[a].open ? "" : "*", ssid, (int)s_aps[a].rssi,
                         on ? app_str(APP_STR_LINKED) : "");
            } else {
                snprintf(line, sizeof(line), "%s%s%s%s\n",
                         m, s_aps[a].open ? "" : "*", ssid,
                         on ? app_str(APP_STR_LINKED) : "");
            }
        } else if (i == ap_base() + s_ap_count) {
            snprintf(line, sizeof(line), "%s %s\n", m, app_str(APP_STR_RESCAN));
        } else {
            snprintf(line, sizeof(line), "%s %s\n", m, app_str(APP_STR_FORGET));
        }
        size_t ln = strlen(line);
        if (used + ln >= n) break;
        memcpy(out + used, line, ln + 1);
        used += ln;
    }
}

static void refresh(void)
{
    if (!s_hint || !s_body) return;
    bsp_wifi_state_t st = bsp_wifi_state();
    char ip[20];
    bsp_wifi_ip(ip, sizeof(ip));

    if (!bsp_wifi_enabled()) lv_label_set_text(s_hint, app_str(APP_STR_WIFI_OFF));
    else if (s_scanning) lv_label_set_text(s_hint, app_str(APP_STR_SCANNING));
    else if (st == BSP_WIFI_CONNECTING) {
        lv_label_set_text_fmt(s_hint, app_str(APP_STR_CONNECTING), bsp_wifi_ssid());
    } else if (s_view == VIEW_PASS) {
        if (st == BSP_WIFI_FAILED) {
            lv_label_set_text(s_hint, app_str(APP_STR_FAIL_RETRY));
        } else if (st == BSP_WIFI_CONNECTED) {
            lv_label_set_text_fmt(s_hint, app_str(APP_STR_GO_WEB), ip);
        } else {
            lv_label_set_text(s_hint, app_str(APP_STR_HOLD_SKIP));
        }
    } else if (st == BSP_WIFI_CONNECTED) {
        lv_label_set_text_fmt(s_hint, "%s  %s", bsp_wifi_ssid(), ip);
    } else if (st == BSP_WIFI_FAILED) {
        lv_label_set_text(s_hint, app_str(APP_STR_FAIL_PASS));
    } else {
        lv_label_set_text(s_hint, app_str(APP_STR_OK_CHOOSE));
    }

    char body[900];
    if (s_view == VIEW_PASS) {
        char vis[BSP_WIFI_PASS_MAX + 8];
        mask_pass(vis, sizeof(vis));
        app_kb_render(body, sizeof(body), s_pick, vis, s_kb_sel, s_kb_set);
    } else {
        render_list(body, sizeof(body));
    }
    lv_label_set_text(s_body, body);
    if (s_view == VIEW_PASS) {
        app_web_set_target("password", s_pass, sizeof(s_pass), refresh);
    } else {
        app_web_clear_target();
    }
}

static void tick(lv_timer_t *t)
{
    (void)t;
    refresh();
}

static void move_sel(int delta)
{
    if (s_view == VIEW_PASS) app_ui_move(&s_kb_sel, KB_N, delta);
    else app_ui_move(&s_sel, extra_count(), delta);
}

static void hold_tick(lv_timer_t *t)
{
    (void)t;
    if (s_hold_btn < 0 || s_scanning) return;
    s_hold_ms += 80;
    if (s_hold_ms < 280) return;
    int dir = (s_hold_btn == BSP_BTN_UP) ? -1 : 1;
    int step = (s_hold_ms >= 800) ? (s_view == VIEW_PASS ? KB_COLS : 3) : 1;
    move_sel(dir * step);
    refresh();
}

static void remember_current_ssid(void)
{
    const char *cur = bsp_wifi_ssid();
    if (!cur[0]) return;
    for (int i = 0; i < s_ap_count; i++) {
        if (strcmp(s_aps[i].ssid, cur) == 0) return;
    }
    if (s_ap_count >= BSP_WIFI_SCAN_MAX) return;
    bsp_wifi_ap_t row = { 0 };
    strlcpy(row.ssid, cur, sizeof(row.ssid));
    memmove(&s_aps[1], &s_aps[0], (size_t)s_ap_count * sizeof(s_aps[0]));
    s_aps[0] = row;
    s_ap_count++;
}

static void choose(void)
{
    if (!bsp_wifi_enabled() && s_sel != 0) return;
    if (s_sel == 0) {
        bsp_wifi_set_enabled(!bsp_wifi_enabled());
        if (bsp_wifi_enabled()) s_req = 1;
        return;
    }
    if (s_sel == 1) {
        bsp_wifi_set_auto_connect(!bsp_wifi_auto_connect());
        return;
    }
    if (web_ready() && s_sel == 2) {
        app_web_qr_open();
        return;
    }
    int base = ap_base();
    if (s_sel < base + s_ap_count) {
        int a = s_sel - base;
        strlcpy(s_pick, s_aps[a].ssid, sizeof(s_pick));
        if (s_aps[a].open) {
            s_pass[0] = 0;
            s_req = 2;
        } else {
            s_pass[0] = 0;
            bsp_wifi_saved_pass(s_pick, s_pass, sizeof(s_pass));
            s_kb_sel = 0;
            s_kb_set = 0;
            s_view = VIEW_PASS;
        }
        return;
    }
    if (s_sel == base + s_ap_count) {
        s_req = 1;
        return;
    }
    s_req = 3;
}

static void wifi_task(void *arg)
{
    (void)arg;
    for (;;) {
        int req = s_req;
        if (req == 1) {
            s_req = 0;
            s_scanning = true;
            int n = bsp_wifi_scan(s_aps, BSP_WIFI_SCAN_MAX);
            s_ap_count = n < 0 ? 0 : n;
            remember_current_ssid();
            s_view = VIEW_LIST;
            s_scanning = false;
        } else if (req == 2) {
            s_req = 0;
            bsp_wifi_connect(s_pick, s_pass);
        } else if (req == 3) {
            s_req = 0;
            bsp_wifi_forget();
            s_ap_count = 0;
            s_sel = 0;
            s_view = VIEW_LIST;
            s_req = bsp_wifi_enabled() ? 1 : 0;
        } else {
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
}

void app_wifi_enter(lv_obj_t *p)
{
    s_view = VIEW_LIST;
    s_ap_count = 0;
    s_sel = 0;
    s_kb_sel = 0;
    s_kb_set = 0;
    s_pass[0] = 0;
    s_pick[0] = 0;
    s_req = 0;
    s_scanning = false;
    s_hold_btn = -1;
    s_hold_ms = 0;

    lv_obj_t *card = app_ui_card(p);
    app_ui_title(card, app_str(APP_STR_WIFI));
    s_hint = app_ui_hint(card);
    s_body = app_ui_body(card, 44);

    s_timer = lv_timer_create(tick, 250, NULL);
    s_hold_timer = lv_timer_create(hold_tick, 80, NULL);
    if (!s_task) xTaskCreate(wifi_task, "app_wifi", 4096, NULL, 4, &s_task);
    remember_current_ssid();
    if (bsp_wifi_enabled()) s_req = 1;
    refresh();
}

void app_wifi_exit(void)
{
    s_req = 0;
    s_hold_btn = -1;
    if (s_hold_timer) { lv_timer_delete(s_hold_timer); s_hold_timer = NULL; }
    if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
    if (s_task) { vTaskDelete(s_task); s_task = NULL; }
    app_web_clear_target();
    app_web_qr_close();
    s_hint = s_body = NULL;
}

void app_wifi_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    if (s_scanning) return;
    if (btn == BSP_BTN_UP || btn == BSP_BTN_DOWN) {
        if (ev == BSP_BTN_PRESS) {
            s_hold_btn = (int)btn;
            s_hold_ms = 0;
            move_sel(btn == BSP_BTN_UP ? -1 : 1);
            refresh();
        } else if (ev == BSP_BTN_RELEASE && s_hold_btn == (int)btn) {
            s_hold_btn = -1;
            s_hold_ms = 0;
        }
        return;
    }
    if (ev != BSP_BTN_CLICK) return;
    if (btn != BSP_BTN_OK) return;
    if (s_view == VIEW_PASS) {
        int r = app_kb_click(s_pass, sizeof(s_pass), &s_kb_sel, &s_kb_set);
        if (r == 2) s_req = 2;
        if (r == 3) {
            s_view = VIEW_LIST;
            app_web_qr_close();
        }
        if (r == 4) app_web_qr_open();
    } else {
        choose();
    }
    refresh();
}
