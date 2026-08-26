#pragma once

#include <stdbool.h>
#include <stdint.h>

#define APP_MEOW_RHY_MAX     20
#define APP_MEOW_RHY_TRAVEL  3200
#define APP_MEOW_RHY_NEAR    400
#define APP_MEOW_RHY_PERF    220
#define APP_MEOW_RHY_GOOD    340
#define APP_MEOW_RHY_SWITCH  200
#define APP_MEOW_RHY_STAGE_N 5
#define APP_MEOW_RHY_NEED    8
#define APP_MEOW_RHY_FX      280

#define APP_MEOW_RHY_NONE 0
#define APP_MEOW_RHY_OPEN 1
#define APP_MEOW_RHY_HIT  2
#define APP_MEOW_RHY_MISS 3

#define APP_MEOW_RHY_G_NONE 0
#define APP_MEOW_RHY_G_GOOD 1
#define APP_MEOW_RHY_G_PERF 2
#define APP_MEOW_RHY_G_MISS 3

#define APP_MEOW_RHY_PTS_TAP_GOOD  15
#define APP_MEOW_RHY_PTS_TAP_PERF  20
#define APP_MEOW_RHY_PTS_HOLD_GOOD 20
#define APP_MEOW_RHY_PTS_HOLD_PERF 25

typedef struct {
    uint32_t t_ms;
    uint16_t hold_ms;
    uint8_t lane;
    uint8_t pitch;
} app_meow_rhy_note_t;

typedef struct {
    app_meow_rhy_note_t n[APP_MEOW_RHY_MAX];
    uint8_t st[APP_MEOW_RHY_MAX];
    uint8_t n_n;
    uint8_t hold_i;
    uint8_t hold_grade;
    uint8_t last_grade;
    uint8_t stage;
    uint8_t stage_hits;
    uint8_t failed;
    uint8_t rng;
    uint8_t lane;
    int8_t pitch;
    int8_t dir;
    uint16_t hits;
    uint16_t misses;
    uint16_t combo;
    uint16_t best_combo;
    uint16_t unit_ms;
    uint16_t travel_ms;
    uint32_t fx_at[APP_MEOW_RHY_MAX];
    uint8_t fx_grade[APP_MEOW_RHY_MAX];
    uint32_t cued;
    uint32_t next_t;
    uint32_t fail_at;
    int score;
} app_meow_rhy_t;

void app_meow_rhy_make(app_meow_rhy_t *g, uint8_t rng);
int app_meow_rhy_press(app_meow_rhy_t *g, int lane, uint32_t now_ms);
int app_meow_rhy_release(app_meow_rhy_t *g, int lane, uint32_t now_ms);
void app_meow_rhy_tick(app_meow_rhy_t *g, uint32_t now_ms);
bool app_meow_rhy_done(const app_meow_rhy_t *g, uint32_t now_ms);
int app_meow_rhy_poll_cue(app_meow_rhy_t *g, uint32_t now_ms);
bool app_meow_rhy_near(const app_meow_rhy_t *g, int lane, uint32_t now_ms);
uint16_t app_meow_rhy_gap(int stage);
int app_meow_rhy_hz(int pitch);
