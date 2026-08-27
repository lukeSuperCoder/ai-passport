#include "app_meow_rhythm.h"

#include <string.h>

static uint8_t step(uint8_t *rng)
{
    *rng = (uint8_t)(*rng * 37u + 17u);
    return *rng;
}

uint16_t app_meow_rhy_gap(int stage)
{
    static const uint16_t gap[APP_MEOW_RHY_STAGE_N] = {
        1200, 1000, 850, 650, 400
    };

    if (stage < 0) stage = 0;
    if (stage >= APP_MEOW_RHY_STAGE_N) stage = APP_MEOW_RHY_STAGE_N - 1;
    return gap[stage];
}

static void maybe_level(app_meow_rhy_t *g)
{
    if (g->stage + 1 >= APP_MEOW_RHY_STAGE_N) return;
    if (g->stage_hits < APP_MEOW_RHY_NEED) return;
    g->stage++;
    g->stage_hits = 0;
    g->unit_ms = app_meow_rhy_gap(g->stage);
}

static void mark_hit(app_meow_rhy_t *g, int i, int grade, uint32_t now_ms)
{
    int pts;

    if (g->st[i] >= APP_MEOW_RHY_HIT) return;
    g->st[i] = APP_MEOW_RHY_HIT;
    g->fx_at[i] = now_ms;
    g->fx_grade[i] = (uint8_t)grade;
    if (g->n[i].hold_ms) {
        pts = (grade == APP_MEOW_RHY_G_PERF) ? APP_MEOW_RHY_PTS_HOLD_PERF
                                             : APP_MEOW_RHY_PTS_HOLD_GOOD;
    } else {
        pts = (grade == APP_MEOW_RHY_G_PERF) ? APP_MEOW_RHY_PTS_TAP_PERF
                                             : APP_MEOW_RHY_PTS_TAP_GOOD;
    }
    g->score += pts;
    g->hits++;
    g->stage_hits++;
    g->combo++;
    if (g->combo > g->best_combo) g->best_combo = g->combo;
    g->last_grade = (uint8_t)grade;
    maybe_level(g);
}

static void mark_miss(app_meow_rhy_t *g, int i, uint32_t now_ms)
{
    if (g->st[i] >= APP_MEOW_RHY_HIT) return;
    g->st[i] = APP_MEOW_RHY_MISS;
    g->misses++;
    g->combo = 0;
    g->last_grade = APP_MEOW_RHY_G_MISS;
    g->failed = 1;
    g->fail_at = now_ms;
    if (g->hold_i == (uint8_t)i) g->hold_i = 0xff;
}

static void compact(app_meow_rhy_t *g, uint32_t now_ms)
{
    int w = 0, i;
    uint32_t cued = 0;
    uint8_t hold = 0xff;

    for (i = 0; i < g->n_n; i++) {
        if (g->st[i] == APP_MEOW_RHY_MISS) continue;
        if (g->st[i] == APP_MEOW_RHY_HIT &&
            now_ms >= g->fx_at[i] + APP_MEOW_RHY_FX) continue;
        if (g->hold_i == (uint8_t)i) hold = (uint8_t)w;
        if (g->cued & (1u << i)) cued |= (1u << w);
        if (w != i) {
            g->n[w] = g->n[i];
            g->st[w] = g->st[i];
            g->fx_at[w] = g->fx_at[i];
            g->fx_grade[w] = g->fx_grade[i];
        }
        w++;
    }
    g->n_n = (uint8_t)w;
    g->hold_i = hold;
    g->cued = cued;
}

static uint32_t onset_sep(const app_meow_rhy_t *g, uint8_t next_lane)
{
    const app_meow_rhy_note_t *p;
    uint32_t need = app_meow_rhy_gap(g->stage);

    if (g->n_n == 0) return need;
    p = &g->n[g->n_n - 1];
    if (p->hold_ms) {
        uint32_t after = (uint32_t)p->hold_ms + APP_MEOW_RHY_SWITCH;
        if (after > need) need = after;
    }
    if (p->lane != next_lane) {
        uint32_t cross = (uint32_t)APP_MEOW_RHY_GOOD + APP_MEOW_RHY_SWITCH;
        if (p->hold_ms) {
            uint32_t h = (uint32_t)p->hold_ms + APP_MEOW_RHY_SWITCH;
            if (h > cross) cross = h;
        }
        if (cross > need) need = cross;
    }
    return need;
}

static void emit(app_meow_rhy_t *g)
{
    uint16_t gap = app_meow_rhy_gap(g->stage);
    int hold = 0;
    app_meow_rhy_note_t *n;

    if (g->n_n >= APP_MEOW_RHY_MAX) compact(g, g->next_t);
    if (g->n_n >= APP_MEOW_RHY_MAX) return;
    if (g->n_n > 0) {
        uint32_t earliest = g->n[g->n_n - 1].t_ms + onset_sep(g, g->lane);
        if (g->next_t < earliest) g->next_t = earliest;
    }
    step(&g->rng);
    if (g->stage >= 2 && (g->rng % 6u) == 0) hold = (int)gap * 2;
    n = &g->n[g->n_n];
    n->t_ms = g->next_t;
    n->hold_ms = (uint16_t)hold;
    n->lane = g->lane;
    n->pitch = (uint8_t)g->pitch;
    g->st[g->n_n] = APP_MEOW_RHY_NONE;
    g->fx_at[g->n_n] = 0;
    g->fx_grade[g->n_n] = 0;
    g->n_n++;
    g->next_t += hold ? (uint32_t)hold + (uint32_t)gap : (uint32_t)gap;
    step(&g->rng);
    if (g->stage == 0 || (g->rng % 5u) != 0) g->lane ^= 1;
    g->pitch = (int8_t)(g->pitch + g->dir);
    if (g->pitch < 0) {
        g->pitch = 1;
        g->dir = 1;
    } else if (g->pitch > 5) {
        g->pitch = 4;
        g->dir = -1;
    }
    if ((g->rng % 7u) == 0) g->dir = (int8_t)(-g->dir);
}

static void fill(app_meow_rhy_t *g, uint32_t now_ms)
{
    if (g->failed) return;
    compact(g, now_ms);
    while (g->n_n < APP_MEOW_RHY_MAX &&
           (g->n_n < 3 ||
            g->next_t < now_ms + (uint32_t)g->travel_ms + 400u)) {
        emit(g);
    }
}

void app_meow_rhy_make(app_meow_rhy_t *g, uint8_t rng)
{
    memset(g, 0, sizeof(*g));
    g->hold_i = 0xff;
    g->travel_ms = APP_MEOW_RHY_TRAVEL;
    g->unit_ms = app_meow_rhy_gap(0);
    g->rng = rng;
    step(&g->rng);
    g->lane = g->rng & 1;
    g->pitch = 2;
    g->dir = 1;
    g->next_t = APP_MEOW_RHY_TRAVEL;
    fill(g, 0);
}

int app_meow_rhy_press(app_meow_rhy_t *g, int lane, uint32_t now_ms)
{
    int best = -1;
    int best_d = APP_MEOW_RHY_GOOD + 1;
    int i, grade;

    if (!g || g->failed || lane < 0 || lane > 1) return 0;
    if (g->hold_i != 0xff) return 0;
    for (i = 0; i < g->n_n; i++) {
        int d;

        if (g->st[i] != APP_MEOW_RHY_NONE) continue;
        if (g->n[i].lane != (uint8_t)lane) continue;
        d = (int)now_ms - (int)g->n[i].t_ms;
        if (d < 0) d = -d;
        if (d <= APP_MEOW_RHY_GOOD && d < best_d) {
            best = i;
            best_d = d;
        }
    }
    if (best < 0) return 0;
    grade = (best_d <= APP_MEOW_RHY_PERF) ? APP_MEOW_RHY_G_PERF
                                          : APP_MEOW_RHY_G_GOOD;
    if (g->n[best].hold_ms) {
        g->st[best] = APP_MEOW_RHY_OPEN;
        g->hold_i = (uint8_t)best;
        g->hold_grade = (uint8_t)grade;
        g->last_grade = (uint8_t)grade;
        g->fx_at[best] = now_ms;
        g->fx_grade[best] = (uint8_t)grade;
        return grade;
    }
    mark_hit(g, best, grade, now_ms);
    return grade;
}

int app_meow_rhy_release(app_meow_rhy_t *g, int lane, uint32_t now_ms)
{
    int i;
    uint32_t end;

    if (!g || g->hold_i == 0xff) return 0;
    i = g->hold_i;
    if (g->n[i].lane != (uint8_t)lane) return 0;
    end = g->n[i].t_ms + g->n[i].hold_ms;
    g->hold_i = 0xff;
    if (now_ms + APP_MEOW_RHY_GOOD < end) {
        mark_miss(g, i, now_ms);
        return APP_MEOW_RHY_G_MISS;
    }
    mark_hit(g, i, g->hold_grade, now_ms);
    return (int)g->hold_grade;
}

void app_meow_rhy_tick(app_meow_rhy_t *g, uint32_t now_ms)
{
    int i;

    if (!g) return;
    if (g->hold_i != 0xff) {
        i = g->hold_i;
        if (now_ms > g->n[i].t_ms + g->n[i].hold_ms + APP_MEOW_RHY_GOOD) {
            mark_hit(g, i, g->hold_grade, now_ms);
            g->hold_i = 0xff;
        }
    }
    for (i = 0; i < g->n_n; i++) {
        if (g->st[i] != APP_MEOW_RHY_NONE) continue;
        if (now_ms > g->n[i].t_ms + APP_MEOW_RHY_GOOD) {
            mark_miss(g, i, now_ms);
        }
    }
    fill(g, now_ms);
}

bool app_meow_rhy_done(const app_meow_rhy_t *g, uint32_t now_ms)
{
    if (!g) return true;
    if (!g->failed) return false;
    return now_ms >= g->fail_at + 280;
}

int app_meow_rhy_poll_cue(app_meow_rhy_t *g, uint32_t now_ms)
{
    int i;

    if (!g) return -1;
    for (i = 0; i < g->n_n; i++) {
        if (g->cued & (1u << i)) continue;
        if (now_ms + 20 >= g->n[i].t_ms) {
            g->cued |= (1u << i);
            return (int)g->n[i].pitch;
        }
    }
    return -1;
}

bool app_meow_rhy_near(const app_meow_rhy_t *g, int lane, uint32_t now_ms)
{
    int i;

    if (!g || lane < 0 || lane > 1) return false;
    for (i = 0; i < g->n_n; i++) {
        int d;

        if (g->n[i].lane != (uint8_t)lane) continue;
        if (g->st[i] >= APP_MEOW_RHY_HIT) continue;
        d = (int)now_ms - (int)g->n[i].t_ms;
        if (d < 0) d = -d;
        if (d <= APP_MEOW_RHY_NEAR) return true;
    }
    return false;
}

int app_meow_rhy_hz(int pitch)
{
    static const uint16_t hz[] = { 262, 294, 330, 392, 440, 523 };

    if (pitch < 0) pitch = 0;
    if (pitch > 5) pitch = 5;
    return (int)hz[pitch];
}
