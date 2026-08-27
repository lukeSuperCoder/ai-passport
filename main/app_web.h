#pragma once

#include "bsp_button.h"
#include "lvgl.h"

#include <stdbool.h>
#include <stddef.h>

#define APP_WEB_TEXT_MAX 160

void app_web_init(lv_obj_t *screen);
void app_web_listen(void);
void app_web_poll(void);
bool app_web_visible(void);
bool app_web_key(bsp_btn_t btn, bsp_btn_ev_t ev);

// 打开键盘等输入页时登记;写入时拷进 buf 并调用 refresh。
void app_web_set_target(const char *name, char *buf, size_t cap,
                        void (*refresh)(void));
void app_web_clear_target(void);

#define APP_WEB_HTTP_PORT 8080
#define APP_WEB_HTTPS_PORT 8443

bool app_web_url(char *buf, size_t n);
/* HTTP 不是安全源,navigator.mediaDevices 为 undefined。
 * TLS 挂在 8443,不占 443:Safari/Chrome 会把 http://IP 升到 https://IP,
 * 自签证书一旦出现在 443,扫码页就再也打不开。 */
bool app_web_redirect_https(void *httpd_req, const char *path);
bool app_web_https_up(void);
void app_web_qr_open(void);
void app_web_qr_close(void);
bool app_web_qr_visible(void);
bool app_web_qr_key(bsp_btn_t btn, bsp_btn_ev_t ev);
