#pragma once

#include "walkie_codec.h"

#include "esp_err.h"

#include <stdbool.h>

typedef enum {
    WALKIE_MODE_WEBRTC = 0,
    WALKIE_MODE_BLE = 1,
} walkie_mode_t;

typedef enum {
    WALKIE_OFF = 0,
    WALKIE_WAIT,
    WALKIE_LINK,
    WALKIE_TALK,
    WALKIE_LISTEN,
} walkie_run_t;

typedef enum {
    WALKIE_E_OK = 0,
    WALKIE_E_WIFI,
    WALKIE_E_BLE,
    WALKIE_E_MEM,
    WALKIE_E_NET,
    WALKIE_E_AUDIO,
} walkie_err_t;

esp_err_t walkie_start(walkie_mode_t mode, int ch);
void walkie_stop(void);
walkie_err_t walkie_last_err(void);
void walkie_clear_err(void);
void walkie_set_ptt(bool on);
bool walkie_busy(void);
bool walkie_ptt(void);
walkie_run_t walkie_run(void);
walkie_mode_t walkie_mode(void);
int walkie_channel(void);
int walkie_peer_n(void);
const char *walkie_peer_name(int i);

void walkie_rx_bytes(const uint8_t *p, size_t n);
void walkie_note_peer(const char *name);
