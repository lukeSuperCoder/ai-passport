#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void walkie_ble_prepare(void);
esp_err_t walkie_ble_start(int ch);
void walkie_ble_stop(void);
void walkie_ble_send(const uint8_t *p, size_t n);
bool walkie_ble_linked(void);
