#pragma once

#include "app_tama.h"

#include <stdbool.h>
#include <stdint.h>

#define APP_TAMA_RUN_LANES     3
#define APP_TAMA_RUN_MAX       8
#define APP_TAMA_RUN_ITEM_MS   3000
#define APP_TAMA_RUN_HAZ_START 20000
#define APP_TAMA_RUN_HAZ_EVERY 2000
#define APP_TAMA_RUN_HAZ_STEP  10000
#define APP_TAMA_RUN_HAZ_CAP   50000
#define APP_TAMA_RUN_HAZ_STEPS 2
#define APP_TAMA_RUN_HAZ_PCT   30
#define APP_TAMA_RUN_TRAVEL    2400
#define APP_TAMA_RUN_HIT0      820
#define APP_TAMA_RUN_HIT1      1200
#define APP_TAMA_RUN_GAP_START 30000
#define APP_TAMA_RUN_GAP_STEP  10000
#define APP_TAMA_RUN_GAP_CAP   60000
#define APP_TAMA_RUN_GAP_N     3
#define APP_TAMA_RUN_SPD_START 60000
#define APP_TAMA_RUN_SPD_STEP  10000
#define APP_TAMA_RUN_SPD_CAP   100000
#define APP_TAMA_RUN_SPD_N     4
#define APP_TAMA_RUN_FX_MS     280
#define APP_TAMA_RUN_TIP_MS    800

#define APP_TAMA_RUN_LIVE 0
#define APP_TAMA_RUN_GOT  1
#define APP_TAMA_RUN_MISS 2
#define APP_TAMA_RUN_BUMP 3

#define APP_TAMA_RUN_ITEM 1
#define APP_TAMA_RUN_HAZ  2
#define APP_TAMA_HAZ_N     4

#define APP_TAMA_RUN_EV_NONE 0
#define APP_TAMA_RUN_EV_ITEM 1
#define APP_TAMA_RUN_EV_HAZ  2

typedef struct {
    uint32_t t_ms;
    uint16_t travel;
    uint8_t lane;
    uint8_t kind;
    int8_t good;
    uint8_t st;
} app_tama_run_obj_t;

typedef struct {
    app_tama_run_obj_t o[APP_TAMA_RUN_MAX];
    uint8_t n;
    uint8_t lane;
    uint8_t rng;
    uint8_t over;
    uint8_t got[APP_TAMA_G_N];
    uint16_t items;
    uint32_t next_item;
    uint32_t next_haz;
    int8_t last_good;
    uint8_t last_lane;
    uint32_t last_at;
} app_tama_run_t;

void app_tama_run_make(app_tama_run_t *g, uint8_t rng);
void app_tama_run_move(app_tama_run_t *g, int dir);
int app_tama_run_haz_pct(uint32_t t_ms);
int app_tama_run_gap_n(uint32_t t_ms);
int app_tama_run_spd_n(uint32_t t_ms);
uint32_t app_tama_run_item_ms(uint32_t t_ms);
uint32_t app_tama_run_haz_ms(uint32_t t_ms);
uint32_t app_tama_run_travel(uint32_t t_ms);
int app_tama_run_prog(const app_tama_run_obj_t *o, uint32_t now_ms);
int app_tama_run_put(app_tama_run_t *g, uint32_t now_ms, int kind, int lane,
                     int good);
int app_tama_run_step(app_tama_run_t *g, uint32_t now_ms, int good);
bool app_tama_run_done(const app_tama_run_t *g);
int app_tama_run_got_n(const app_tama_run_t *g);
