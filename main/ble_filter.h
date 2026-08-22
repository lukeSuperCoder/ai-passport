#pragma once

#include <stdbool.h>
#include <stddef.h>

bool ble_filter_is_sms(const char *app_id);
bool ble_filter_has_keyword(const char *text, const char *extra_kw);
void ble_filter_extract_code(const char *msg, char *out, size_t n);
void ble_filter_pick_code(const char *title, const char *subtitle,
                         const char *message, char *out, size_t n);
void ble_filter_to_latin(const char *in, char *out, size_t n);
