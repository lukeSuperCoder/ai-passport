#pragma once

#include "app_tama.h"

#include <stdbool.h>
#include <stdint.h>

#define APP_TAMA_MAT_W        6
#define APP_TAMA_MAT_H        6
#define APP_TAMA_MAT_KIND     3
#define APP_TAMA_MAT_EMPTY    0xff
#define APP_TAMA_MAT_TIME0    60000
#define APP_TAMA_MAT_TIME_PER 1000
#define APP_TAMA_MAT_TIME_MAX 99000
#define APP_TAMA_MAT_PTS      10

#define APP_TAMA_MAT_EV_NONE   0
#define APP_TAMA_MAT_EV_SEL    1
#define APP_TAMA_MAT_EV_UNSEL  2
#define APP_TAMA_MAT_EV_CLEAR  3
#define APP_TAMA_MAT_EV_REVERT 4
#define APP_TAMA_MAT_EV_OVER   5
#define APP_TAMA_MAT_EV_NEXT   6

typedef struct {
    uint8_t cell[APP_TAMA_MAT_H][APP_TAMA_MAT_W];
    uint8_t mark[APP_TAMA_MAT_H][APP_TAMA_MAT_W];
    uint8_t pal[APP_TAMA_MAT_KIND];
    uint8_t cur_r;
    uint8_t cur_c;
    int8_t sel_r;
    int8_t sel_c;
    int8_t vdir;
    int8_t hdir;
    uint8_t rng;
    uint8_t over;
    int score;
    int left_ms;
} app_tama_mat_t;

void app_tama_mat_make(app_tama_mat_t *g, uint8_t rng);
void app_tama_mat_move(app_tama_mat_t *g, int axis, int jump);
int app_tama_mat_ok(app_tama_mat_t *g);
void app_tama_mat_unsel(app_tama_mat_t *g);
int app_tama_mat_tick(app_tama_mat_t *g, uint32_t dt_ms);
int app_tama_mat_matches(const app_tama_mat_t *g);
int app_tama_mat_sec(const app_tama_mat_t *g);
bool app_tama_mat_done(const app_tama_mat_t *g);
bool app_tama_mat_selected(const app_tama_mat_t *g);
int app_tama_mat_good(const app_tama_mat_t *g, int kind);
