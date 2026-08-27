#include "app_web.h"

#include "app.h"
#include "app_i18n.h"
#include "app_notif.h"
#include "app_tone.h"
#include "app_prefs.h"
#include "bsp_wifi.h"
#include "qrcode.h"
#include "ui_pixel.h"
#include "walkie.h"
#include "walkie_rtc.h"
#include "app_rtc.h"

#include "esp_http_server.h"
#include "esp_https_server.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "app_web";

static const char PAGE[] =
    "<!doctype html><html lang=en><meta charset=utf-8>"
    "<meta name=viewport content=\"width=device-width,initial-scale=1\">"
    "<title>Passport</title>"
    "<style>"
    "body{margin:0;background:#1689E8;font-family:sans-serif;color:#17202A}"
    "main{max-width:480px;margin:24px auto;background:#F4F4EA;"
    "padding:16px;border:4px solid #17202A}"
    "h1{margin:0 0 8px;font-size:22px}"
    "p{margin:0 0 12px;color:#0872C9;font-size:14px}"
    "textarea{width:100%;min-height:140px;font-size:20px;padding:10px;"
    "border:3px solid #17202A;box-sizing:border-box}"
    "button{width:100%;margin-top:12px;padding:14px;font-size:18px;"
    "font-weight:700;background:#FFD928;border:3px solid #17202A}"
    "#ok{min-height:1.4em;color:#55951D}"
    "</style>"
    "<main><h1>Passport</h1><p id=st></p>"
    "<form method=post action=/t>"
    "<textarea name=t id=t></textarea>"
    "<button type=submit>Send</button></form>"
    "<p id=ok></p>"
"<p><a href=/w>Walkie</a> · <a href=/rtc>WebRTC</a></p>"
"</main>"
    "<script>"
    "let T={ok:'Sent',fail:'Failed'};"
    "const f=document.querySelector('form');"
    "f.onsubmit=async e=>{"
    "e.preventDefault();"
    "const t=document.getElementById('t').value;"
    "try{"
    "const r=await fetch('/t',{method:'POST',"
    "headers:{'Content-Type':'text/plain;charset=utf-8'},body:t});"
    "document.getElementById('ok').textContent=r.ok?T.ok:T.fail;"
    "}catch(err){document.getElementById('ok').textContent=T.fail;}"
    "};"
    "async function st(){try{"
    "const j=await(await fetch('/s')).json();"
    "document.documentElement.lang=j.lang||'en';"
    "T.ok=j.ok;T.fail=j.fail;"
    "document.querySelector('button').textContent=j.send;"
    "document.getElementById('t').placeholder=j.ph;"
    "document.getElementById('st').textContent=j.field?"
    "j.busy.replace('%s',j.field):j.idle;"
    "}catch(e){}}"
    "st();setInterval(st,2000);"
    "</script>";

static httpd_handle_t s_httpd;
static httpd_handle_t s_https;
static SemaphoreHandle_t s_mu;
static uint32_t s_retry_at;
static void server_start(void);
static void https_ensure(void);

static void mu_ensure(void)
{
    if (!s_mu) s_mu = xSemaphoreCreateMutex();
}

static char s_pending[APP_WEB_TEXT_MAX + 1];
static bool s_have;
static bool s_fresh;

static char s_field[16];
static char *s_buf;
static size_t s_cap;
static void (*s_refresh)(void);

static lv_obj_t *s_box, *s_title, *s_body, *s_hint;
static char s_shown_text[APP_WEB_TEXT_MAX + 1];
static bool s_shown;

static lv_obj_t *s_qr_box, *s_qr_title, *s_qr_draw, *s_qr_url, *s_qr_hint;
static QRCode s_qr;
static uint8_t s_qr_mod[128];
static char s_qr_text[36];
static bool s_qr_ok;
static bool s_qr_shown;

bool app_web_url(char *buf, size_t n)
{
    if (!buf || n < 32) return false;
    buf[0] = 0;
    if (bsp_wifi_state() != BSP_WIFI_CONNECTED) return false;
    char ip[20];
    if (bsp_wifi_ip(ip, sizeof(ip)) != ESP_OK) return false;
    if (!ip[0] || strcmp(ip, "0.0.0.0") == 0) return false;
    /* 必须非 80:Safari/Chrome 会把 http://IP 和 http://IP:80 升到 443。 */
    snprintf(buf, n, "http://%s:%d/", ip, APP_WEB_HTTP_PORT);
    return true;
}

static int hex(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static void url_decode(char *s)
{
    char *d = s;
    for (; *s; s++) {
        if (*s == '+') {
            *d++ = ' ';
            continue;
        }
        if (*s == '%' && s[1] && s[2]) {
            int h = hex(s[1]), l = hex(s[2]);
            if (h >= 0 && l >= 0) {
                *d++ = (char)((h << 4) | l);
                s += 2;
                continue;
            }
        }
        *d++ = *s;
    }
    *d = 0;
}

static void trim_ws(char *s)
{
    char *e = s + strlen(s);
    while (e > s && (e[-1] == '\r' || e[-1] == '\n' || e[-1] == ' ')) *--e = 0;
    char *p = s;
    while (*p == ' ' || *p == '\r' || *p == '\n') p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
}

static void store_text(const char *src)
{
    if (!src) return;
    char tmp[APP_WEB_TEXT_MAX * 3 + 8];
    ui_pixel_utf8_copy(tmp, sizeof(tmp), src);
    trim_ws(tmp);
    if (!tmp[0]) return;
    xSemaphoreTake(s_mu, portMAX_DELAY);
    ui_pixel_utf8_copy(s_pending, sizeof(s_pending), tmp);
    s_have = true;
    s_fresh = true;
    xSemaphoreGive(s_mu);
}

static void hide(void)
{
    if (s_box) lv_obj_add_flag(s_box, LV_OBJ_FLAG_HIDDEN);
    s_shown = false;
    s_shown_text[0] = 0;
}

static void qr_draw_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_DRAW_MAIN) return;
    if (!s_qr_ok) return;
    lv_obj_t *obj = lv_event_get_target(e);
    lv_layer_t *layer = lv_event_get_layer(e);
    lv_area_t coords;
    lv_obj_get_coords(obj, &coords);

    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_color = lv_color_hex(UI_INK);
    dsc.bg_opa = LV_OPA_COVER;
    dsc.border_width = 0;
    dsc.radius = 0;
    dsc.outline_width = 0;
    dsc.shadow_width = 0;

    int n = s_qr.size;
    int quiet = 2;
    int inner = n + quiet * 2;
    int w = (int)lv_area_get_width(&coords);
    int h = (int)lv_area_get_height(&coords);
    int scale = (w < h ? w : h) / inner;
    if (scale < 1) return;
    int ox = coords.x1 + (w - inner * scale) / 2;
    int oy = coords.y1 + (h - inner * scale) / 2;

    for (int y = 0; y < n; y++) {
        int x = 0;
        while (x < n) {
            if (!qrcode_getModule(&s_qr, (uint8_t)x, (uint8_t)y)) {
                x++;
                continue;
            }
            int x0 = x;
            while (x < n && qrcode_getModule(&s_qr, (uint8_t)x, (uint8_t)y)) x++;
            lv_area_t a;
            a.x1 = ox + (x0 + quiet) * scale;
            a.y1 = oy + (y + quiet) * scale;
            a.x2 = ox + (x + quiet) * scale - 1;
            a.y2 = a.y1 + scale - 1;
            lv_draw_rect(layer, &dsc, &a);
        }
    }
}

static void qr_refresh(void)
{
    if (!s_qr_title) return;
    lv_label_set_text(s_qr_title, app_str(APP_STR_QR_TITLE));
    char url[36];
    if (!app_web_url(url, sizeof(url))) {
        s_qr_ok = false;
        s_qr_text[0] = 0;
        lv_label_set_text(s_qr_url, "");
        lv_label_set_text(s_qr_hint, app_str(APP_STR_QR_NEED));
        if (s_qr_draw) lv_obj_invalidate(s_qr_draw);
        return;
    }
    lv_label_set_text(s_qr_url, url);
    lv_label_set_text(s_qr_hint, app_str(APP_STR_QR_HINT));
    if (s_qr_ok && strcmp(s_qr_text, url) == 0) return;
    strlcpy(s_qr_text, url, sizeof(s_qr_text));
    memset(s_qr_mod, 0, sizeof(s_qr_mod));
    s_qr_ok = qrcode_initText(&s_qr, s_qr_mod, 3, ECC_MEDIUM, url) >= 0;
    if (s_qr_draw) lv_obj_invalidate(s_qr_draw);
}

void app_web_qr_open(void)
{
    if (!s_qr_box) return;
    qr_refresh();
    s_qr_shown = true;
    lv_obj_remove_flag(s_qr_box, LV_OBJ_FLAG_HIDDEN);
    if (bsp_wifi_state() == BSP_WIFI_CONNECTED) server_start();
    app_shell_wake();
}

void app_web_qr_close(void)
{
    if (s_qr_box) lv_obj_add_flag(s_qr_box, LV_OBJ_FLAG_HIDDEN);
    s_qr_shown = false;
}

bool app_web_qr_visible(void)
{
    return s_qr_shown;
}

bool app_web_qr_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    if (!s_qr_shown) return false;
    if (btn == BSP_BTN_OK && (ev == BSP_BTN_CLICK || ev == BSP_BTN_LONG)) {
        app_web_qr_close();
        return true;
    }
    return true;
}

static const char *field_label(void)
{
    if (strcmp(s_field, "password") == 0) return app_str(APP_STR_WEB_PASSWORD);
    if (strcmp(s_field, "keyword") == 0) return app_str(APP_STR_WEB_KEYWORD);
    if (strcmp(s_field, "city") == 0) return app_str(APP_STR_WEB_CITY);
    if (strcmp(s_field, "app") == 0) return app_str(APP_STR_TOTP_APP);
    if (strcmp(s_field, "account") == 0 || strcmp(s_field, "name") == 0) {
        return app_str(APP_STR_TOTP_NAME);
    }
    if (strcmp(s_field, "secret") == 0) return app_str(APP_STR_TOTP_SECRET);
    return s_field[0] ? s_field : app_str(APP_STR_WEB_FIELD);
}

static void paint_box(void)
{
    if (!s_box) return;
    s_shown = true;
    lv_label_set_text(s_title, app_str(APP_STR_WEB_TITLE));
    lv_label_set_text(s_body, s_shown_text[0] ? s_shown_text : " ");
    if (s_buf && s_cap) {
        lv_label_set_text_fmt(s_hint, app_str(APP_STR_WEB_WRITE), field_label());
    } else {
        lv_label_set_text(s_hint, app_str(APP_STR_WEB_IDLE));
    }
    lv_obj_remove_flag(s_box, LV_OBJ_FLAG_HIDDEN);
    app_shell_wake();
}

static esp_err_t send_html(httpd_req_t *req)
{
    ESP_LOGI(TAG, "GET /");
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, PAGE, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t send_status(httpd_req_t *req)
{
    char json[768];
    xSemaphoreTake(s_mu, portMAX_DELAY);
    const char *f = (s_buf && s_field[0]) ? s_field : "";
    snprintf(json, sizeof(json),
             "{\"lang\":\"%s\",\"field\":\"%s\",\"idle\":\"%s\","
             "\"busy\":\"%s\",\"send\":\"%s\",\"ph\":\"%s\","
             "\"ok\":\"%s\",\"fail\":\"%s\",\"hint\":\"%s\",\"tls\":%d}",
             app_lang_html(), f,
             app_str(APP_STR_WEB_NO_PAGE),
             app_str(APP_STR_WEB_BUSY),
             app_str(APP_STR_WEB_SEND),
             app_str(APP_STR_WEB_PLACEHOLDER),
             app_str(APP_STR_WEB_SENT),
             app_str(APP_STR_WEB_FAIL),
             app_str(APP_STR_WEB_HINT),
             app_web_https_up() ? 1 : 0);
    xSemaphoreGive(s_mu);
    httpd_resp_set_type(req, "application/json; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t recv_text(httpd_req_t *req)
{
    int total = req->content_len;
    if (total < 0 || total > 512) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "too long");
        return ESP_FAIL;
    }
    char raw[513];
    int got = 0;
    while (got < total) {
        int n = httpd_req_recv(req, raw + got, (size_t)(total - got));
        if (n <= 0) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "recv");
            return ESP_FAIL;
        }
        got += n;
    }
    raw[got] = 0;

    char ctype[64] = { 0 };
    httpd_req_get_hdr_value_str(req, "Content-Type", ctype, sizeof(ctype));
    char *text = raw;
    if (strstr(ctype, "application/x-www-form-urlencoded")) {
        url_decode(raw);
        if (!strncmp(raw, "t=", 2)) text = raw + 2;
        char *amp = strchr(text, '&');
        if (amp) *amp = 0;
    }
    store_text(text);
    httpd_resp_set_type(req, "text/plain; charset=utf-8");
    return httpd_resp_sendstr(req, "ok");
}

static const httpd_uri_t URI_ROOT = {
    .uri = "/", .method = HTTP_GET, .handler = send_html,
};
static const httpd_uri_t URI_STATUS = {
    .uri = "/s", .method = HTTP_GET, .handler = send_status,
};
static const httpd_uri_t URI_POST = {
    .uri = "/t", .method = HTTP_POST, .handler = recv_text,
};
static const httpd_uri_t URI_FORM = {
    .uri = "/", .method = HTTP_POST, .handler = recv_text,
};

extern const unsigned char servercert_pem_start[] asm("_binary_servercert_pem_start");
extern const unsigned char servercert_pem_end[]   asm("_binary_servercert_pem_end");
extern const unsigned char prvtkey_pem_start[] asm("_binary_prvtkey_pem_start");
extern const unsigned char prvtkey_pem_end[]   asm("_binary_prvtkey_pem_end");

static bool ua_webview(httpd_req_t *req)
{
    char ua[96] = { 0 };
    httpd_req_get_hdr_value_str(req, "User-Agent", ua, sizeof(ua));
    return strstr(ua, "MicroMessenger") || strstr(ua, "AlipayClient")
        || strstr(ua, "DingTalk");
}

static void register_common(httpd_handle_t hd)
{
    httpd_register_uri_handler(hd, &URI_ROOT);
    httpd_register_uri_handler(hd, &URI_STATUS);
    httpd_register_uri_handler(hd, &URI_POST);
    httpd_register_uri_handler(hd, &URI_FORM);
    walkie_web_attach(hd);
    app_rtc_attach(hd);
}

static bool s_https_busy;
static bool s_tls_want;
static uint32_t s_https_retry_at;

bool app_web_https_up(void)
{
    return s_https != NULL;
}

static bool https_start(void)
{
    if (s_https) return true;
    httpd_ssl_config_t cfg = HTTPD_SSL_CONFIG_DEFAULT();
    cfg.port_secure = APP_WEB_HTTPS_PORT;
    cfg.httpd.max_open_sockets = 2;
    cfg.httpd.lru_purge_enable = true;
    cfg.httpd.stack_size = 8192;
    cfg.httpd.max_uri_handlers = 12;
    cfg.httpd.ctrl_port = 32769;
    cfg.servercert = servercert_pem_start;
    cfg.servercert_len = servercert_pem_end - servercert_pem_start;
    cfg.prvtkey_pem = prvtkey_pem_start;
    cfg.prvtkey_len = prvtkey_pem_end - prvtkey_pem_start;
    ESP_LOGI(TAG, "https try heap=%u blk=%u",
             (unsigned)esp_get_free_heap_size(),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    esp_err_t e = httpd_ssl_start(&s_https, &cfg);
    if (e != ESP_OK) {
        s_https = NULL;
        ESP_LOGE(TAG, "https start %s heap=%u blk=%u",
                 esp_err_to_name(e),
                 (unsigned)esp_get_free_heap_size(),
                 (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
        return false;
    }
    register_common(s_https);
    ESP_LOGI(TAG, "https :%d heap=%u", APP_WEB_HTTPS_PORT,
             (unsigned)esp_get_free_heap_size());
    return true;
}

bool app_web_redirect_https(void *reqp, const char *path)
{
    httpd_req_t *req = reqp;
    if (!req || !path || !path[0]) return false;
    /* /rtc /w 才要 TLS。扫码首页走 :8080;TLS 放 8443,避免占 443
     * 后浏览器把 http://IP 升成 https://IP 并卡在证书页。 */
    s_tls_want = true;
    https_ensure();
    if (!s_https) return false;
    if (req->handle == s_https) return false;
    if (ua_webview(req)) return false;

    char host[48] = { 0 };
    httpd_req_get_hdr_value_str(req, "Host", host, sizeof(host));
    char *colon = strchr(host, ':');
    if (colon) *colon = 0;
    if (!host[0]) bsp_wifi_ip(host, sizeof(host));
    if (!host[0]) return false;

    char loc[72];
    int n = snprintf(loc, sizeof(loc), "https://%s:%d%s",
                     host, APP_WEB_HTTPS_PORT, path);
    if (n <= 0 || n >= (int)sizeof(loc)) return false;
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", loc);
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_send(req, NULL, 0);
    return true;
}

static void https_task(void *arg)
{
    (void)arg;
    bool ok = https_start();
    s_https_busy = false;
    if (!ok) s_https_retry_at = xTaskGetTickCount() + pdMS_TO_TICKS(8000);
    vTaskDelete(NULL);
}

static void https_ensure(void)
{
    if (s_https || s_https_busy) return;
    uint32_t now = xTaskGetTickCount();
    if (s_https_retry_at && now < s_https_retry_at) return;
    s_https_busy = true;
    if (xTaskCreate(https_task, "https", 8192, NULL, 4, NULL) != pdPASS) {
        s_https_busy = false;
        if (!https_start()) {
            s_https_retry_at = now + pdMS_TO_TICKS(8000);
        }
    }
}

static void server_start(void)
{
    if (s_httpd) return;
    uint32_t now = xTaskGetTickCount();
    if (s_retry_at && now < s_retry_at) return;

    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port = APP_WEB_HTTP_PORT;
    cfg.ctrl_port = 32768;
    cfg.max_open_sockets = 2;
    cfg.lru_purge_enable = true;
    cfg.stack_size = 4096;
    cfg.max_uri_handlers = 12;
    esp_err_t e = httpd_start(&s_httpd, &cfg);
    if (e != ESP_OK) {
        /* 失败时 listen fd 可能没关,250ms 连着重试会 EADDRINUSE 把 socket 耗尽。 */
        s_httpd = NULL;
        s_retry_at = now + pdMS_TO_TICKS(8000);
        ESP_LOGE(TAG, "httpd start %s heap=%u largest=%u",
                 esp_err_to_name(e),
                 (unsigned)esp_get_free_heap_size(),
                 (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
        return;
    }
    s_retry_at = 0;
    register_common(s_httpd);
    bsp_wifi_ps_hold();
    char ip[20] = { 0 };
    bsp_wifi_ip(ip, sizeof(ip));
    ESP_LOGI(TAG, "http://%s:%d/  heap=%u",
             ip[0] ? ip : "0.0.0.0", APP_WEB_HTTP_PORT,
             (unsigned)esp_get_free_heap_size());
}

void app_web_init(lv_obj_t *screen)
{
    mu_ensure();

    s_qr_box = lv_obj_create(screen);
    ui_pixel_strip_theme(s_qr_box);
    lv_obj_set_pos(s_qr_box, 10, APP_HEADER_H + 10);
    lv_obj_set_size(s_qr_box, 220, 320 - APP_HEADER_H - 20);
    lv_obj_set_style_border_width(s_qr_box, 4, 0);
    lv_obj_set_style_border_color(s_qr_box, lv_color_hex(UI_INK), 0);
    lv_obj_set_style_pad_all(s_qr_box, 8, 0);
    lv_obj_set_style_bg_opa(s_qr_box, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(s_qr_box, lv_color_hex(UI_PAPER), 0);

    s_qr_title = lv_label_create(s_qr_box);
    lv_obj_set_style_text_font(s_qr_title, ui_pixel_font_20(), 0);
    lv_obj_set_style_text_color(s_qr_title, lv_color_hex(UI_INK), 0);
    lv_obj_set_width(s_qr_title, 196);
    lv_label_set_long_mode(s_qr_title, LV_LABEL_LONG_CLIP);
    lv_obj_align(s_qr_title, LV_ALIGN_TOP_LEFT, 0, 0);

    /* 标题行高 28;二维码缩小,地址和说明依次排在下面,避免贴底换行叠字。 */
    s_qr_draw = lv_obj_create(s_qr_box);
    ui_pixel_strip_theme(s_qr_draw);
    lv_obj_set_size(s_qr_draw, 136, 136);
    lv_obj_set_style_bg_opa(s_qr_draw, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(s_qr_draw, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(s_qr_draw, LV_ALIGN_TOP_MID, 0, 30);
    lv_obj_add_event_cb(s_qr_draw, qr_draw_cb, LV_EVENT_DRAW_MAIN, NULL);

    s_qr_url = lv_label_create(s_qr_box);
    lv_obj_set_style_text_font(s_qr_url, ui_pixel_font_14(), 0);
    lv_obj_set_style_text_color(s_qr_url, lv_color_hex(UI_INK), 0);
    lv_obj_set_width(s_qr_url, 196);
    lv_label_set_long_mode(s_qr_url, LV_LABEL_LONG_CLIP);
    lv_obj_align(s_qr_url, LV_ALIGN_TOP_LEFT, 0, 170);

    s_qr_hint = lv_label_create(s_qr_box);
    lv_obj_set_style_text_font(s_qr_hint, ui_pixel_font_14(), 0);
    lv_obj_set_style_text_color(s_qr_hint, lv_color_hex(UI_SKY_DARK), 0);
    lv_obj_set_width(s_qr_hint, 196);
    lv_label_set_long_mode(s_qr_hint, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_qr_hint, LV_ALIGN_TOP_LEFT, 0, 198);

    lv_obj_add_flag(s_qr_box, LV_OBJ_FLAG_HIDDEN);

    s_box = lv_obj_create(screen);
    ui_pixel_strip_theme(s_box);
    lv_obj_set_pos(s_box, 10, APP_HEADER_H + 10);
    lv_obj_set_size(s_box, 220, 320 - APP_HEADER_H - 20);
    lv_obj_set_style_border_width(s_box, 4, 0);
    lv_obj_set_style_border_color(s_box, lv_color_hex(UI_INK), 0);
    lv_obj_set_style_pad_all(s_box, 10, 0);
    lv_obj_set_style_bg_opa(s_box, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(s_box, lv_color_hex(UI_PAPER), 0);

    s_title = lv_label_create(s_box);
    lv_obj_set_style_text_font(s_title, ui_pixel_font_20(), 0);
    lv_obj_set_style_text_color(s_title, lv_color_hex(UI_INK), 0);
    lv_obj_set_width(s_title, 190);
    lv_obj_align(s_title, LV_ALIGN_TOP_LEFT, 0, 0);

    s_body = lv_label_create(s_box);
    lv_obj_set_style_text_font(s_body, ui_pixel_font_14(), 0);
    lv_obj_set_style_text_color(s_body, lv_color_hex(UI_INK), 0);
    lv_obj_set_width(s_body, 190);
    lv_label_set_long_mode(s_body, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_body, LV_ALIGN_TOP_LEFT, 0, 40);

    s_hint = lv_label_create(s_box);
    lv_obj_set_style_text_font(s_hint, ui_pixel_font_14(), 0);
    lv_obj_set_style_text_color(s_hint, lv_color_hex(UI_SKY_DARK), 0);
    lv_obj_set_width(s_hint, 190);
    lv_label_set_long_mode(s_hint, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_hint, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    hide();
}

void app_web_listen(void)
{
    mu_ensure();
    server_start();
}

void app_web_poll(void)
{
    bool up = bsp_wifi_state() == BSP_WIFI_CONNECTED;
    /* 80 在 BLE 之前拉起后保持监听。QR 关掉就 stop 的话,
     * BLE 占堆后再 start 会 ESP_ERR_HTTPD_TASK。 */
    if (up || s_httpd) server_start();
    if (up && (walkie_busy() || s_tls_want)) https_ensure();
    else if (!up && s_https) {
        httpd_ssl_stop(s_https);
        s_https = NULL;
        s_https_busy = false;
        s_https_retry_at = 0;
        s_tls_want = false;
    }

    if (app_notif_pairing()) {
        if (s_shown) hide();
        if (s_qr_shown) app_web_qr_close();
        return;
    }
    if (s_qr_shown) qr_refresh();
    if (!s_mu) return;

    xSemaphoreTake(s_mu, portMAX_DELAY);
    bool have = s_have && s_pending[0];
    bool target = s_buf && s_cap;
    bool fresh = s_fresh;
    char next[APP_WEB_TEXT_MAX + 1];
    next[0] = 0;
    if (have && target) strlcpy(next, s_pending, sizeof(next));
    if (have && target) s_fresh = false;
    xSemaphoreGive(s_mu);

    if (!have || !target) {
        if (s_shown && !target) hide();
        return;
    }
    if (!s_shown || fresh || strcmp(s_shown_text, next) != 0) {
        strlcpy(s_shown_text, next, sizeof(s_shown_text));
        paint_box();
        app_shell_wake();
        if (fresh) app_tone_play((int)app_prefs()->tone_msg);
    }
}

bool app_web_visible(void)
{
    return s_shown;
}

bool app_web_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    if (!s_shown) return false;
    if (btn == BSP_BTN_OK && ev == BSP_BTN_LONG) {
        xSemaphoreTake(s_mu, portMAX_DELAY);
        s_have = false;
        s_pending[0] = 0;
        s_fresh = false;
        xSemaphoreGive(s_mu);
        hide();
        return true;
    }
    if (ev != BSP_BTN_CLICK) return true;
    if (btn != BSP_BTN_OK) return true;

    char text[APP_WEB_TEXT_MAX + 1];
    void (*refresh)(void) = NULL;
    xSemaphoreTake(s_mu, portMAX_DELAY);
    strlcpy(text, s_pending, sizeof(text));
    char *buf = s_buf;
    size_t cap = s_cap;
    refresh = s_refresh;
    s_have = false;
    s_pending[0] = 0;
    s_fresh = false;
    xSemaphoreGive(s_mu);

    hide();
    if (buf && cap) {
        ui_pixel_utf8_copy(buf, cap, text);
        if (refresh) refresh();
    }
    return true;
}

void app_web_set_target(const char *name, char *buf, size_t cap,
                        void (*refresh)(void))
{
    if (!s_mu) return;
    xSemaphoreTake(s_mu, portMAX_DELAY);
    s_buf = buf;
    s_cap = cap;
    s_refresh = refresh;
    s_field[0] = 0;
    if (name) {
        size_t i = 0;
        for (; name[i] && i + 1 < sizeof(s_field); i++) {
            char c = name[i];
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9')) {
                s_field[i] = c;
            } else {
                break;
            }
        }
        s_field[i] = 0;
    }
    xSemaphoreGive(s_mu);
}

void app_web_clear_target(void)
{
    if (!s_mu) return;
    xSemaphoreTake(s_mu, portMAX_DELAY);
    s_buf = NULL;
    s_cap = 0;
    s_refresh = NULL;
    s_field[0] = 0;
    xSemaphoreGive(s_mu);
}
