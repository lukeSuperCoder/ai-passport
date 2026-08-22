// main/demo_wifi.c —— 扫描 AP、三键输入密码、连接;成功后由 BSP 写入 NVS 并自动重连。
// 扫描/连接放工作任务。OK 长按返回仍由 main 拦截。
#include "demo.h"
#include "bsp_wifi.h"
#include "bsp_display.h"
#include "ui_pixel.h"
#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

#define KB_COLS 6
#define KB_ROWS 8
#define KB_N    (KB_COLS * KB_ROWS)

// 主页就带数字和常用符号,避免还要翻页才能输入密码。
static const char *const KB_LOWER[KB_N] = {
    "1", "2", "3", "4", "5", "6",
    "7", "8", "9", "0", ".", "@",
    "a", "b", "c", "d", "e", "f",
    "g", "h", "i", "j", "k", "l",
    "m", "n", "o", "p", "q", "r",
    "s", "t", "u", "v", "w", "x",
    "y", "z", "_", "-", "#", "/",
    "SPC", "DEL", "Aa", "*", "GO", "BK",
};
static const char *const KB_UPPER[KB_N] = {
    "1", "2", "3", "4", "5", "6",
    "7", "8", "9", "0", ".", "@",
    "A", "B", "C", "D", "E", "F",
    "G", "H", "I", "J", "K", "L",
    "M", "N", "O", "P", "Q", "R",
    "S", "T", "U", "V", "W", "X",
    "Y", "Z", "_", "-", "#", "/",
    "SPC", "DEL", "Aa", "*", "GO", "BK",
};
static const char *const KB_SYM[KB_N] = {
    "!", "?", ":", ";", "+", "=",
    "*", "&", "%", "$", ",", "~",
    "(", ")", "[", "]", "{", "}",
    "<", ">", "'", "\"", "\\", "|",
    "^", "`", ",", ".", ";", ":",
    "+", "-", "_", "#", "@", "/",
    "!", "?", "*", "&", "%", "$",
    "SPC", "DEL", "Aa", "~", "GO", "BK",
};

typedef enum {
    VIEW_LIST = 0,
    VIEW_PASS,
} view_t;

static lv_obj_t *s_scr, *s_hint, *s_body;
static lv_timer_t *s_timer;
static lv_timer_t *s_hold_timer;
static TaskHandle_t s_task;

static volatile int s_req;            // 1=scan 2=connect 3=forget
static volatile bool s_scanning;
static view_t s_view;
static bsp_wifi_ap_t s_aps[BSP_WIFI_SCAN_MAX];
static int s_ap_count;
static int s_sel;                     // list 选择: 0..ap_count-1 AP, 其后 Rescan / Forget
static int s_kb_sel;
static int s_kb_set;                  // 0 lower 1 upper 2 sym
static char s_pick_ssid[BSP_WIFI_SSID_MAX + 1];
static char s_pass[BSP_WIFI_PASS_MAX + 1];
static bsp_wifi_state_t s_seen_state;
static bool s_show_forget;
static int s_hold_btn = -1;           // BSP_BTN_UP / DOWN, -1=未按住
static int s_hold_ms;

static const char *const *kb_keys(void) {
    if (s_kb_set == 1) return KB_UPPER;
    if (s_kb_set == 2) return KB_SYM;
    return KB_LOWER;
}

static int list_item_count(void) {
    int n = s_ap_count + 1;                           // Rescan
    if (s_show_forget) n++;
    return n;
}

static void mask_pass(char *out, size_t n) {
    size_t len = strlen(s_pass);
    if (len == 0) {
        snprintf(out, n, "(empty)");
        return;
    }
    size_t i = 0;
    for (; i + 1 < len && i + 1 < n; i++) out[i] = '*';
    if (i < n - 1) out[i++] = s_pass[len - 1];
    out[i] = 0;
}

static void render_list(char *out, size_t n) {
    size_t used = 0;
    out[0] = 0;
    int items = list_item_count();
    if (s_sel >= items) s_sel = items ? items - 1 : 0;
    int window = 6;
    int start = s_sel - window / 2;
    if (start < 0) start = 0;
    if (start + window > items) start = items > window ? items - window : 0;

    for (int i = start; i < items && i < start + window; i++) {
        char line[80];
        const char *mark = (i == s_sel) ? ">" : " ";
        if (i < s_ap_count) {
            char ssid[40];
            ui_pixel_utf8_copy(ssid, sizeof(ssid), s_aps[i].ssid);
            snprintf(line, sizeof(line), "%s%s %s %ddBm\n",
                     mark, s_aps[i].open ? " " : "*", ssid, (int)s_aps[i].rssi);
        } else if (i == s_ap_count) {
            snprintf(line, sizeof(line), "%s Rescan\n", mark);
        } else {
            snprintf(line, sizeof(line), "%s Forget\n", mark);
        }
        size_t ln = strlen(line);
        if (used + ln >= n) break;
        memcpy(out + used, line, ln + 1);
        used += ln;
    }
}

static void render_kb(char *out, size_t n) {
    const char *const *keys = kb_keys();
    char pass[BSP_WIFI_PASS_MAX + 8];
    mask_pass(pass, sizeof(pass));
    int off = snprintf(out, n, "%s\n%s\n", s_pick_ssid, pass);
    if (off < 0 || (size_t)off >= n) return;
    for (int r = 0; r < KB_ROWS; r++) {
        for (int c = 0; c < KB_COLS; c++) {
            int i = r * KB_COLS + c;
            char cell[12];
            if (i == s_kb_sel) snprintf(cell, sizeof(cell), "[%s]", keys[i]);
            else snprintf(cell, sizeof(cell), " %s ", keys[i]);
            int w = snprintf(out + off, n - (size_t)off, "%s", cell);
            if (w < 0) return;
            off += w;
            if ((size_t)off >= n) return;
        }
        int w = snprintf(out + off, n - (size_t)off, "\n");
        if (w < 0) return;
        off += w;
        if ((size_t)off >= n) return;
    }
}

static void refresh(void) {
    if (!s_hint || !s_body) return;
    bsp_wifi_state_t st = bsp_wifi_state();
    char ip[20];
    bsp_wifi_ip(ip, sizeof(ip));

    if (s_scanning) {
        lv_label_set_text(s_hint, "scanning...");
    } else if (st == BSP_WIFI_CONNECTING) {
        lv_label_set_text_fmt(s_hint, "connecting %s", bsp_wifi_ssid());
    } else if (s_view == VIEW_PASS) {
        lv_label_set_text(s_hint, st == BSP_WIFI_FAILED
            ? "failed  GO retry  BK: list"
            : "hold UP/DOWN skip  BK: list");
    } else if (st == BSP_WIFI_CONNECTED) {
        lv_label_set_text_fmt(s_hint, "online %s", ip);
    } else if (st == BSP_WIFI_FAILED) {
        lv_label_set_text(s_hint, "failed  check password");
    } else {
        lv_label_set_text(s_hint, "UP/DOWN  OK: choose");
    }

    char body[900];
    if (s_view == VIEW_PASS) render_kb(body, sizeof(body));
    else render_list(body, sizeof(body));
    lv_label_set_text(s_body, body);
}

static void tick(lv_timer_t *t) {
    (void)t;
    bsp_wifi_state_t st = bsp_wifi_state();
    if (st == BSP_WIFI_CONNECTED && s_seen_state != BSP_WIFI_CONNECTED) {
        s_view = VIEW_LIST;
        s_sel = 0;
        s_show_forget = true;
    }
    s_seen_state = st;
    refresh();
}

static void apply_key(const char *k) {
    if (strcmp(k, "DEL") == 0) {
        size_t len = strlen(s_pass);
        if (len) s_pass[len - 1] = 0;
        return;
    }
    if (strcmp(k, "SPC") == 0) k = " ";
    if (strcmp(k, "Aa") == 0) {
        s_kb_set = (s_kb_set + 1) % 3;
        return;
    }
    if (strcmp(k, "BK") == 0) {
        s_view = VIEW_LIST;
        s_hold_btn = -1;
        return;
    }
    if (strcmp(k, "GO") == 0) {
        if (!s_pick_ssid[0]) return;
        s_req = 2;
        return;
    }
    size_t len = strlen(s_pass);
    if (len + strlen(k) >= sizeof(s_pass)) return;
    memcpy(s_pass + len, k, strlen(k) + 1);
}

static void choose_list_item(void) {
    int items = list_item_count();
    if (items <= 0) {
        s_req = 1;
        return;
    }
    if (s_sel < s_ap_count) {
        strlcpy(s_pick_ssid, s_aps[s_sel].ssid, sizeof(s_pick_ssid));
        if (s_aps[s_sel].open) {
            s_pass[0] = 0;
            s_req = 2;
        } else {
            s_pass[0] = 0;
            bsp_wifi_saved_pass(s_pick_ssid, s_pass, sizeof(s_pass));
            s_kb_sel = 0;
            s_kb_set = 0;
            s_view = VIEW_PASS;
        }
        return;
    }
    if (s_sel == s_ap_count) {
        s_req = 1;
        return;
    }
    s_req = 3;
}

static void move_sel(int delta) {
    if (s_view == VIEW_PASS) {
        int n = KB_N;
        s_kb_sel = (s_kb_sel + delta) % n;
        if (s_kb_sel < 0) s_kb_sel += n;
    } else {
        int items = list_item_count();
        if (items < 1) items = 1;
        s_sel = (s_sel + delta) % items;
        if (s_sel < 0) s_sel += items;
    }
}

static void hold_tick(lv_timer_t *t) {
    (void)t;
    if (s_hold_btn < 0 || s_scanning) return;
    s_hold_ms += 80;
    if (s_hold_ms < 280) return;                 // 先等一下,避免轻点变成连跳
    int dir = (s_hold_btn == BSP_BTN_UP) ? -1 : 1;
    int step = (s_hold_ms >= 800) ? KB_COLS : 1; // 按住更久则按行跳
    if (s_view != VIEW_PASS) step = (s_hold_ms >= 800) ? 3 : 1;
    move_sel(dir * step);
    refresh();
}

static void wifi_task(void *arg) {
    (void)arg;
    for (;;) {
        int req = s_req;
        if (req == 1) {
            s_req = 0;
            s_scanning = true;
            int n = bsp_wifi_scan(s_aps, BSP_WIFI_SCAN_MAX);
            s_ap_count = n < 0 ? 0 : n;
            s_sel = 0;
            s_view = VIEW_LIST;
            s_scanning = false;
        } else if (req == 2) {
            s_req = 0;
            bsp_wifi_connect(s_pick_ssid, s_pass);
        } else if (req == 3) {
            s_req = 0;
            bsp_wifi_forget();
            s_ap_count = 0;
            s_sel = 0;
            s_view = VIEW_LIST;
            s_show_forget = false;
            s_req = 1;
        } else {
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
}

void demo_wifi_enter(void) {
    s_view = VIEW_LIST;
    s_ap_count = 0;
    s_sel = 0;
    s_kb_sel = 0;
    s_kb_set = 0;
    s_pass[0] = 0;
    s_pick_ssid[0] = 0;
    s_req = 0;
    s_scanning = false;
    s_seen_state = bsp_wifi_state();
    s_show_forget = bsp_wifi_has_saved() || s_seen_state == BSP_WIFI_CONNECTED;

    s_scr = ui_pixel_screen_create("WIFI");
    lv_obj_t *panel = ui_pixel_panel_create(s_scr, 10, 46, 220, 232, UI_PAPER);

    s_hint = lv_label_create(panel);
    lv_obj_set_style_text_font(s_hint, ui_pixel_font_14(), 0);
    lv_obj_set_style_text_color(s_hint, lv_color_hex(UI_SKY_DARK), 0);
    lv_obj_set_width(s_hint, 196);
    lv_label_set_long_mode(s_hint, LV_LABEL_LONG_CLIP);
    lv_obj_align(s_hint, LV_ALIGN_TOP_LEFT, 0, 0);

    s_body = lv_label_create(panel);
    lv_obj_set_style_text_font(s_body, ui_pixel_font_14(), 0);
    lv_obj_set_style_text_color(s_body, lv_color_hex(UI_INK), 0);
    lv_label_set_long_mode(s_body, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_body, 196);
    lv_obj_align(s_body, LV_ALIGN_TOP_LEFT, 0, 20);

    s_hold_btn = -1;
    s_hold_ms = 0;
    s_timer = lv_timer_create(tick, 250, NULL);
    s_hold_timer = lv_timer_create(hold_tick, 80, NULL);

    if (!s_task) xTaskCreate(wifi_task, "demo_wifi", 4096, NULL, 4, &s_task);
    if (bsp_wifi_state() != BSP_WIFI_CONNECTED) s_req = 1;
    refresh();
    lv_screen_load(s_scr);
}

void demo_wifi_exit(void) {
    s_req = 0;
    s_hold_btn = -1;
    if (s_hold_timer) { lv_timer_delete(s_hold_timer); s_hold_timer = NULL; }
    if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
    if (s_task) { vTaskDelete(s_task); s_task = NULL; }
    if (s_scr) {
        lv_obj_delete(s_scr);
        s_scr = NULL;
        s_hint = s_body = NULL;
    }
}

void demo_wifi_key(bsp_btn_t btn, bsp_btn_ev_t ev) {
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
    if (btn == BSP_BTN_OK) {
        if (s_view == VIEW_PASS) apply_key(kb_keys()[s_kb_sel]);
        else choose_list_item();
        refresh();
    }
}
