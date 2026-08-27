#pragma once

#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>

esp_err_t walkie_rtc_start(int ch);
void walkie_rtc_stop(void);
void walkie_rtc_send(const uint8_t *p, size_t n);
void walkie_rtc_poll(void);
int walkie_rtc_ws_n(void);
void walkie_web_attach(void *httpd);
void walkie_web_detach(void);
