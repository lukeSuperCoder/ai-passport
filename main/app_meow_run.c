#include "app_meow_run.h"

#include <string.h>

static uint8_t step(uint8_t *rng)
{
    *rng = (uint8_t)(*rng * 37u + 17u);
    return *rng;
}

static void compact(app_meow_run_t *g)
{
    uint8_t i, n = 0;

    for (i = 0; i < g->n; i++) {
        if (g->o[i].st == APP_MEOW_RUN_LIVE) {
            if (n != i) g->o[n] = g->o[i];
            n++;
        }
    }
    g->n = n;
}

int app_meow_run_haz_pct(uint32_t t_ms)
{
    int steps;
    int pct;

    if (t_ms < APP_MEOW_RUN_HAZ_START) return 0;
    /* 20s=30%, 30s=60%, 40s=90%；50s 起不再加，停在 90%。 */
    if (t_ms >= APP_MEOW_RUN_HAZ_CAP) t_ms = APP_MEOW_RUN_HAZ_CAP - 1;
    steps = (int)((t_ms - APP_MEOW_RUN_HAZ_START) / APP_MEOW_RUN_HAZ_STEP);
    if (steps > APP_MEOW_RUN_HAZ_STEPS) steps = APP_MEOW_RUN_HAZ_STEPS;
    pct = APP_MEOW_RUN_HAZ_PCT + steps * APP_MEOW_RUN_HAZ_PCT;
    if (pct > 100) pct = 100;
    return pct;
}

/* 30s、40s、50s 各缩一档间隔；60s 起不再缩。 */
int app_meow_run_gap_n(uint32_t t_ms)
{
    int n;

    if (t_ms < APP_MEOW_RUN_GAP_START) return 0;
    if (t_ms >= APP_MEOW_RUN_GAP_CAP) t_ms = APP_MEOW_RUN_GAP_CAP - 1;
    n = (int)((t_ms - APP_MEOW_RUN_GAP_START) / APP_MEOW_RUN_GAP_STEP) + 1;
    if (n > APP_MEOW_RUN_GAP_N) n = APP_MEOW_RUN_GAP_N;
    return n;
}

/* 60s、70s、80s、90s 各加快一档；100s 起不再加。 */
int app_meow_run_spd_n(uint32_t t_ms)
{
    int n;

    if (t_ms < APP_MEOW_RUN_SPD_START) return 0;
    if (t_ms >= APP_MEOW_RUN_SPD_CAP) t_ms = APP_MEOW_RUN_SPD_CAP - 1;
    n = (int)((t_ms - APP_MEOW_RUN_SPD_START) / APP_MEOW_RUN_SPD_STEP) + 1;
    if (n > APP_MEOW_RUN_SPD_N) n = APP_MEOW_RUN_SPD_N;
    return n;
}

uint32_t app_meow_run_item_ms(uint32_t t_ms)
{
    return (uint32_t)APP_MEOW_RUN_ITEM_MS * (10u - (uint32_t)app_meow_run_gap_n(t_ms)) / 10u;
}

uint32_t app_meow_run_haz_ms(uint32_t t_ms)
{
    return (uint32_t)APP_MEOW_RUN_HAZ_EVERY * (10u - (uint32_t)app_meow_run_gap_n(t_ms)) / 10u;
}

uint32_t app_meow_run_travel(uint32_t t_ms)
{
    return (uint32_t)APP_MEOW_RUN_TRAVEL * 10u / (10u + (uint32_t)app_meow_run_spd_n(t_ms));
}

int app_meow_run_prog(const app_meow_run_obj_t *o, uint32_t now_ms)
{
    uint32_t dt;
    uint32_t travel;

    if (!o || now_ms <= o->t_ms) return 0;
    dt = now_ms - o->t_ms;
    travel = o->travel ? o->travel : (uint32_t)APP_MEOW_RUN_TRAVEL;
    return (int)(dt * 1000u / travel);
}

int app_meow_run_got_n(const app_meow_run_t *g)
{
    int n = 0;
    int i;

    if (!g) return 0;
    for (i = 0; i < APP_MEOW_G_N; i++) n += (int)g->got[i];
    return n;
}

bool app_meow_run_done(const app_meow_run_t *g)
{
    return g && g->over;
}

void app_meow_run_make(app_meow_run_t *g, uint8_t rng)
{
    if (!g) return;
    memset(g, 0, sizeof(*g));
    g->lane = 1;
    g->rng = rng;
    g->next_item = 0;
    g->next_haz = APP_MEOW_RUN_HAZ_START;
    g->last_good = -1;
}

void app_meow_run_move(app_meow_run_t *g, int dir)
{
    int lane;

    if (!g || g->over || dir == 0) return;
    lane = (int)g->lane + (dir < 0 ? -1 : 1);
    if (lane < 0) lane = 0;
    if (lane >= APP_MEOW_RUN_LANES) lane = APP_MEOW_RUN_LANES - 1;
    g->lane = (uint8_t)lane;
}

static int lane_busy(const app_meow_run_t *g, int lane, uint32_t now_ms)
{
    uint8_t i;

    for (i = 0; i < g->n; i++) {
        if (g->o[i].st != APP_MEOW_RUN_LIVE) continue;
        if ((int)g->o[i].lane != lane) continue;
        if (app_meow_run_prog(&g->o[i], now_ms) < 400) return 1;
    }
    return 0;
}

static int lane_has(const app_meow_run_t *g, int lane, int kind)
{
    uint8_t i;

    for (i = 0; i < g->n; i++) {
        if (g->o[i].st != APP_MEOW_RUN_LIVE) continue;
        if ((int)g->o[i].lane == lane && (int)g->o[i].kind == kind) return 1;
    }
    return 0;
}

static int pick_lane(app_meow_run_t *g, uint32_t now_ms, int avoid_kind)
{
    int free[APP_MEOW_RUN_LANES];
    int n = 0;
    int i;

    for (i = 0; i < APP_MEOW_RUN_LANES; i++) {
        if (lane_busy(g, i, now_ms)) continue;
        if (avoid_kind && lane_has(g, i, avoid_kind)) continue;
        free[n++] = i;
    }
    if (n <= 0) return -1;
    return free[step(&g->rng) % n];
}

int app_meow_run_put(app_meow_run_t *g, uint32_t now_ms, int kind, int lane,
                     int good)
{
    app_meow_run_obj_t *o;

    if (!g || g->over) return -1;
    if (kind != APP_MEOW_RUN_ITEM && kind != APP_MEOW_RUN_HAZ) return -1;
    if (lane < 0 || lane >= APP_MEOW_RUN_LANES) return -1;
    compact(g);
    if (g->n >= APP_MEOW_RUN_MAX) return -1;
    o = &g->o[g->n++];
    o->t_ms = now_ms;
    o->travel = (uint16_t)app_meow_run_travel(now_ms);
    o->lane = (uint8_t)lane;
    o->kind = (uint8_t)kind;
    if (kind == APP_MEOW_RUN_HAZ) {
        if (good < 0 || good >= APP_MEOW_HAZ_N) good = 0;
    }
    o->good = (int8_t)good;
    o->st = APP_MEOW_RUN_LIVE;
    return (int)g->n - 1;
}

static int take_item(app_meow_run_t *g, app_meow_run_obj_t *o, uint32_t now_ms)
{
    int id = (int)o->good;

    o->st = APP_MEOW_RUN_GOT;
    if (id < 0 || id >= APP_MEOW_G_N) return 0;
    g->last_good = (int8_t)id;
    g->last_lane = o->lane;
    g->last_at = now_ms;
    if (g->got[id] < 255) g->got[id]++;
    if (g->items < 0xffff) g->items++;
    return 1;
}

static int collide(app_meow_run_t *g, uint32_t now_ms)
{
    int ev = APP_MEOW_RUN_EV_NONE;
    uint8_t i;

    for (i = 0; i < g->n; i++) {
        app_meow_run_obj_t *o = &g->o[i];
        int p;

        if (o->st != APP_MEOW_RUN_LIVE) continue;
        p = app_meow_run_prog(o, now_ms);
        if (p < APP_MEOW_RUN_HIT0) continue;
        if (o->lane != g->lane) {
            /* 别的道上漏接食物只消失，不结束。 */
            if (p > APP_MEOW_RUN_HIT1) o->st = APP_MEOW_RUN_MISS;
            continue;
        }
        if (o->kind == APP_MEOW_RUN_HAZ) {
            if (p > APP_MEOW_RUN_HIT1) {
                o->st = APP_MEOW_RUN_MISS;
                continue;
            }
            o->st = APP_MEOW_RUN_BUMP;
            g->over = 1;
            return APP_MEOW_RUN_EV_HAZ;
        }
        if (p > APP_MEOW_RUN_HIT1) {
            o->st = APP_MEOW_RUN_MISS;
            continue;
        }
        if (take_item(g, o, now_ms)) ev = APP_MEOW_RUN_EV_ITEM;
    }
    compact(g);
    return ev;
}

static void spawn_item(app_meow_run_t *g, uint32_t t_ms, int good)
{
    int lane = pick_lane(g, t_ms, 0);

    if (lane < 0) return;
    if (good < 0 || good >= APP_MEOW_G_N) good = (int)(step(&g->rng) % APP_MEOW_G_N);
    app_meow_run_put(g, t_ms, APP_MEOW_RUN_ITEM, lane, good);
}

static void spawn_haz(app_meow_run_t *g, uint32_t t_ms)
{
    int lane;
    int pct = app_meow_run_haz_pct(t_ms);

    if (pct <= 0) return;
    if ((int)(step(&g->rng) % 100u) >= pct) return;
    /* 障碍不跟食物叠同一道，免得去捡食物却被判定撞上。 */
    lane = pick_lane(g, t_ms, APP_MEOW_RUN_ITEM);
    if (lane < 0) return;
    app_meow_run_put(g, t_ms, APP_MEOW_RUN_HAZ, lane,
                     (int)(step(&g->rng) % APP_MEOW_HAZ_N));
}

int app_meow_run_step(app_meow_run_t *g, uint32_t now_ms, int good)
{
    int ev;

    if (!g || g->over) return APP_MEOW_RUN_EV_NONE;
    ev = collide(g, now_ms);
    if (g->over) return ev;
    while (g->next_item <= now_ms) {
        int ev2;

        spawn_item(g, g->next_item, good);
        g->next_item += app_meow_run_item_ms(g->next_item);
        ev2 = collide(g, now_ms);
        if (ev2 == APP_MEOW_RUN_EV_HAZ) return ev2;
        if (ev2 == APP_MEOW_RUN_EV_ITEM) ev = ev2;
    }
    while (now_ms >= APP_MEOW_RUN_HAZ_START && g->next_haz <= now_ms) {
        int ev2;

        spawn_haz(g, g->next_haz);
        g->next_haz += app_meow_run_haz_ms(g->next_haz);
        ev2 = collide(g, now_ms);
        if (ev2 == APP_MEOW_RUN_EV_HAZ) return ev2;
        if (ev2 == APP_MEOW_RUN_EV_ITEM) ev = ev2;
    }
    return ev;
}
