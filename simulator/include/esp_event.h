#pragma once

#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>

typedef const char *esp_event_base_t;
typedef void (*esp_event_handler_t)(void *, esp_event_base_t, int32_t, void *);

#define ESP_EVENT_DEFINE_BASE(name) esp_event_base_t name = #name

esp_err_t esp_event_handler_register(esp_event_base_t base, int32_t id,
                                     esp_event_handler_t handler, void *arg);
esp_err_t esp_event_post(esp_event_base_t base, int32_t id, const void *data,
                         size_t data_size, uint32_t ticks_to_wait);
