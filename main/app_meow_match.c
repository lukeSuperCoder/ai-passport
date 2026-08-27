#include "app_meow_match.h"

#include <string.h>

static int board_left(const app_meow_mat_t *g);
static int has_move(app_meow_mat_t *g);
static void deal(app_meow_mat_t *g);

static uint8_t step(uint8_t *rng)
{
    *rng = (uint8_t)(*rng * 37u + 17u);
    return *rng;
}

static int in_board(int r, int c)
{
    return r >= 0 && r < APP_MEOW_MAT_H && c >= 0 && c < APP_MEOW_MAT_W;
}

static int near_sel(const app_meow_mat_t *g, int r, int c)
{
    int dr, dc;

    if (g->sel_r < 0) return 1;
    dr = r - (int)g->sel_r;
    dc = c - (int)g->sel_c;
    if (dr < 0) dr = -dr;
    if (dc < 0) dc = -dc;
    return dr <= 1 && dc <= 1 && dr + dc <= 1;
}

static int would_run(const app_meow_mat_t *g, int r, int c, uint8_t k)
{
    int n, i;

    if (k == APP_MEOW_MAT_EMPTY) return 0;
    n = 1;
    for (i = c - 1; i >= 0 && g->cell[r][i] == k; i--) n++;
    for (i = c + 1; i < APP_MEOW_MAT_W && g->cell[r][i] == k; i++) n++;
    if (n >= 3) return 1;
    n = 1;
    for (i = r - 1; i >= 0 && g->cell[i][c] == k; i--) n++;
    for (i = r + 1; i < APP_MEOW_MAT_H && g->cell[i][c] == k; i++) n++;
    return n >= 3;
}

static uint8_t roll_kind(app_meow_mat_t *g, int r, int c)
{
    uint8_t k;
    int n = 0;

    do {
        k = (uint8_t)(step(&g->rng) % APP_MEOW_MAT_KIND);
        n++;
    } while (would_run(g, r, c, k) && n < 16);
    return k;
}

static void fill_empty(app_meow_mat_t *g, int avoid)
{
    int r, c;

    for (r = 0; r < APP_MEOW_MAT_H; r++) {
        for (c = 0; c < APP_MEOW_MAT_W; c++) {
            if (g->cell[r][c] != APP_MEOW_MAT_EMPTY) continue;
            g->cell[r][c] = avoid ? roll_kind(g, r, c)
                                  : (uint8_t)(step(&g->rng) % APP_MEOW_MAT_KIND);
        }
    }
}

static int mark_matches(app_meow_mat_t *g)
{
    int r, c, n, i, got = 0;
    uint8_t k;

    memset(g->mark, 0, sizeof(g->mark));
    for (r = 0; r < APP_MEOW_MAT_H; r++) {
        c = 0;
        while (c < APP_MEOW_MAT_W) {
            k = g->cell[r][c];
            n = 1;
            while (c + n < APP_MEOW_MAT_W && g->cell[r][c + n] == k) n++;
            if (k != APP_MEOW_MAT_EMPTY && n >= 3) {
                for (i = 0; i < n; i++) g->mark[r][c + i] = 1;
            }
            c += n;
        }
    }
    for (c = 0; c < APP_MEOW_MAT_W; c++) {
        r = 0;
        while (r < APP_MEOW_MAT_H) {
            k = g->cell[r][c];
            n = 1;
            while (r + n < APP_MEOW_MAT_H && g->cell[r + n][c] == k) n++;
            if (k != APP_MEOW_MAT_EMPTY && n >= 3) {
                for (i = 0; i < n; i++) g->mark[r + i][c] = 1;
            }
            r += n;
        }
    }
    for (r = 0; r < APP_MEOW_MAT_H; r++) {
        for (c = 0; c < APP_MEOW_MAT_W; c++) {
            if (g->mark[r][c]) got++;
        }
    }
    return got;
}

static void clear_marked(app_meow_mat_t *g)
{
    int r, c;

    for (r = 0; r < APP_MEOW_MAT_H; r++) {
        for (c = 0; c < APP_MEOW_MAT_W; c++) {
            if (g->mark[r][c]) g->cell[r][c] = APP_MEOW_MAT_EMPTY;
        }
    }
}

static void score_marks(app_meow_mat_t *g, int n)
{
    if (n <= 0) return;
    g->score += n * APP_MEOW_MAT_PTS;
    g->left_ms += n * APP_MEOW_MAT_TIME_PER;
    if (g->left_ms > APP_MEOW_MAT_TIME_MAX) {
        g->left_ms = APP_MEOW_MAT_TIME_MAX;
    }
}

static int begin_vanish(app_meow_mat_t *g)
{
    int n = mark_matches(g);

    if (n <= 0) return 0;
    score_marks(g, n);
    g->anim = APP_MEOW_MAT_ST_VANISH;
    g->anim_ms = 0;
    return n;
}

static void begin_fall(app_meow_mat_t *g)
{
    uint8_t old[APP_MEOW_MAT_H][APP_MEOW_MAT_W];
    int r, c, w, nnew;

    memcpy(old, g->cell, sizeof(old));
    memset(g->cell, APP_MEOW_MAT_EMPTY, sizeof(g->cell));
    memset(g->fall, 0, sizeof(g->fall));
    for (c = 0; c < APP_MEOW_MAT_W; c++) {
        w = APP_MEOW_MAT_H - 1;
        for (r = APP_MEOW_MAT_H - 1; r >= 0; r--) {
            if (old[r][c] == APP_MEOW_MAT_EMPTY) continue;
            g->cell[w][c] = old[r][c];
            g->fall[w][c] = (uint8_t)(w - r);
            w--;
        }
        nnew = w + 1;
        for (r = 0; r < nnew; r++) g->fall[r][c] = (uint8_t)nnew;
    }
    fill_empty(g, 1);
    memset(g->mark, 0, sizeof(g->mark));
    g->anim = APP_MEOW_MAT_ST_FALL;
    g->anim_ms = 0;
}

static int finish_fall(app_meow_mat_t *g)
{
    memset(g->fall, 0, sizeof(g->fall));
    g->anim = APP_MEOW_MAT_ST_IDLE;
    g->anim_ms = 0;
    if (begin_vanish(g)) return APP_MEOW_MAT_EV_CLEAR;
    if (board_left(g) <= 0 || !has_move(g)) {
        deal(g);
        return APP_MEOW_MAT_EV_NEXT;
    }
    return APP_MEOW_MAT_EV_NONE;
}

static int step_anim(app_meow_mat_t *g, uint32_t dt_ms)
{
    uint32_t dur;
    uint32_t next;

    if (g->anim == APP_MEOW_MAT_ST_VANISH) dur = APP_MEOW_MAT_VANISH_MS;
    else if (g->anim == APP_MEOW_MAT_ST_FALL) dur = APP_MEOW_MAT_FALL_MS;
    else return APP_MEOW_MAT_EV_NONE;
    next = (uint32_t)g->anim_ms + dt_ms;
    if (next > 0xffffu) next = 0xffffu;
    g->anim_ms = (uint16_t)next;
    if (g->anim_ms < dur) return APP_MEOW_MAT_EV_NONE;
    g->anim_ms = (uint16_t)dur;
    if (g->anim == APP_MEOW_MAT_ST_VANISH) {
        clear_marked(g);
        begin_fall(g);
        return APP_MEOW_MAT_EV_NONE;
    }
    return finish_fall(g);
}

static void swap_cells(app_meow_mat_t *g, int r0, int c0, int r1, int c1)
{
    uint8_t t = g->cell[r0][c0];
    g->cell[r0][c0] = g->cell[r1][c1];
    g->cell[r1][c1] = t;
}

int app_meow_mat_matches(const app_meow_mat_t *g)
{
    app_meow_mat_t tmp;

    if (!g) return 0;
    tmp = *g;
    return mark_matches(&tmp);
}

int app_meow_mat_sec(const app_meow_mat_t *g)
{
    if (!g || g->left_ms <= 0) return 0;
    return (g->left_ms + 999) / 1000;
}

bool app_meow_mat_done(const app_meow_mat_t *g)
{
    return g && g->over;
}

bool app_meow_mat_selected(const app_meow_mat_t *g)
{
    return g && g->sel_r >= 0;
}

int app_meow_mat_good(const app_meow_mat_t *g, int kind)
{
    if (!g || kind < 0 || kind >= APP_MEOW_MAT_KIND) return -1;
    return (int)g->pal[kind];
}

static void pick_pal(app_meow_mat_t *g)
{
    int i, j;

    for (i = 0; i < APP_MEOW_MAT_KIND; i++) {
        uint8_t k;
        int ok;

        do {
            k = (uint8_t)(step(&g->rng) % APP_MEOW_G_N);
            ok = 1;
            for (j = 0; j < i; j++) {
                if (g->pal[j] == k) {
                    ok = 0;
                    break;
                }
            }
        } while (!ok);
        g->pal[i] = k;
    }
}

static int board_left(const app_meow_mat_t *g)
{
    int r, c, n = 0;

    for (r = 0; r < APP_MEOW_MAT_H; r++) {
        for (c = 0; c < APP_MEOW_MAT_W; c++) {
            if (g->cell[r][c] != APP_MEOW_MAT_EMPTY) n++;
        }
    }
    return n;
}

static int has_move(app_meow_mat_t *g)
{
    int r, c, n;

    for (r = 0; r < APP_MEOW_MAT_H; r++) {
        for (c = 0; c < APP_MEOW_MAT_W; c++) {
            if (g->cell[r][c] == APP_MEOW_MAT_EMPTY) continue;
            if (c + 1 < APP_MEOW_MAT_W &&
                g->cell[r][c + 1] != APP_MEOW_MAT_EMPTY) {
                swap_cells(g, r, c, r, c + 1);
                n = mark_matches(g);
                swap_cells(g, r, c, r, c + 1);
                if (n > 0) return 1;
            }
            if (r + 1 < APP_MEOW_MAT_H &&
                g->cell[r + 1][c] != APP_MEOW_MAT_EMPTY) {
                swap_cells(g, r, c, r + 1, c);
                n = mark_matches(g);
                swap_cells(g, r, c, r + 1, c);
                if (n > 0) return 1;
            }
        }
    }
    return 0;
}

static void deal(app_meow_mat_t *g)
{
    g->cur_r = 0;
    g->cur_c = 0;
    g->vdir = 1;
    g->hdir = 1;
    g->sel_r = -1;
    g->sel_c = -1;
    g->anim = APP_MEOW_MAT_ST_IDLE;
    g->anim_ms = 0;
    pick_pal(g);
    memset(g->cell, APP_MEOW_MAT_EMPTY, sizeof(g->cell));
    memset(g->mark, 0, sizeof(g->mark));
    memset(g->fall, 0, sizeof(g->fall));
    fill_empty(g, 1);
}

void app_meow_mat_make(app_meow_mat_t *g, uint8_t rng)
{
    if (!g) return;
    memset(g, 0, sizeof(*g));
    g->rng = rng;
    g->left_ms = APP_MEOW_MAT_TIME0;
    deal(g);
}

void app_meow_mat_unsel(app_meow_mat_t *g)
{
    if (!g) return;
    g->sel_r = -1;
    g->sel_c = -1;
}

static int clamp_step(int pos, int dir, int lo, int hi)
{
    int dest = pos + dir;

    if (dest < lo || dest > hi) {
        dir = -dir;
        dest = pos + dir;
    }
    if (dest < lo) dest = lo;
    if (dest > hi) dest = hi;
    return dest;
}

void app_meow_mat_move(app_meow_mat_t *g, int axis, int jump)
{
    int lo, hi, pos, dest;
    int8_t *dir;

    if (!g || g->over || g->anim) return;
    if (axis == 0) {
        pos = (int)g->cur_r;
        dir = &g->vdir;
        if (g->sel_r >= 0) {
            lo = (int)g->sel_r - 1;
            hi = (int)g->sel_r + 1;
            if (lo < 0) lo = 0;
            if (hi >= APP_MEOW_MAT_H) hi = APP_MEOW_MAT_H - 1;
            g->cur_c = (uint8_t)g->sel_c;
        } else {
            lo = 0;
            hi = APP_MEOW_MAT_H - 1;
        }
    } else {
        pos = (int)g->cur_c;
        dir = &g->hdir;
        if (g->sel_c >= 0) {
            lo = (int)g->sel_c - 1;
            hi = (int)g->sel_c + 1;
            if (lo < 0) lo = 0;
            if (hi >= APP_MEOW_MAT_W) hi = APP_MEOW_MAT_W - 1;
            g->cur_r = (uint8_t)g->sel_r;
        } else {
            lo = 0;
            hi = APP_MEOW_MAT_W - 1;
        }
    }
    if (*dir == 0) *dir = 1;
    if (jump) {
        if (pos == hi) dest = lo;
        else if (pos == lo) dest = hi;
        else dest = (*dir > 0) ? hi : lo;
        *dir = (int8_t)((dest > pos) ? -1 : 1);
    } else {
        dest = clamp_step(pos, *dir, lo, hi);
        if (dest == pos) {
            *dir = (int8_t)(-*dir);
            dest = clamp_step(pos, *dir, lo, hi);
        } else if (dest == lo || dest == hi) {
            *dir = (int8_t)(-*dir);
        }
    }
    if (axis == 0) g->cur_r = (uint8_t)dest;
    else g->cur_c = (uint8_t)dest;
    if (!near_sel(g, (int)g->cur_r, (int)g->cur_c)) {
        if (g->sel_r >= 0) {
            g->cur_r = (uint8_t)g->sel_r;
            g->cur_c = (uint8_t)g->sel_c;
        }
    }
}

int app_meow_mat_ok(app_meow_mat_t *g)
{
    int r0, c0, r1, c1, n;

    if (!g || g->over || g->anim) return APP_MEOW_MAT_EV_NONE;
    if (g->sel_r < 0) {
        g->sel_r = (int8_t)g->cur_r;
        g->sel_c = (int8_t)g->cur_c;
        return APP_MEOW_MAT_EV_SEL;
    }
    r0 = (int)g->sel_r;
    c0 = (int)g->sel_c;
    r1 = (int)g->cur_r;
    c1 = (int)g->cur_c;
    if (r0 == r1 && c0 == c1) {
        app_meow_mat_unsel(g);
        return APP_MEOW_MAT_EV_UNSEL;
    }
    if (!in_board(r1, c1) || !near_sel(g, r1, c1) || (r0 == r1 && c0 == c1)) {
        return APP_MEOW_MAT_EV_NONE;
    }
    swap_cells(g, r0, c0, r1, c1);
    n = begin_vanish(g);
    if (n <= 0) {
        swap_cells(g, r0, c0, r1, c1);
        app_meow_mat_unsel(g);
        return APP_MEOW_MAT_EV_REVERT;
    }
    app_meow_mat_unsel(g);
    return APP_MEOW_MAT_EV_CLEAR;
}

bool app_meow_mat_busy(const app_meow_mat_t *g)
{
    return g && g->anim != APP_MEOW_MAT_ST_IDLE;
}

int app_meow_mat_tick(app_meow_mat_t *g, uint32_t dt_ms)
{
    int ev = APP_MEOW_MAT_EV_NONE;

    if (!g || g->over) return APP_MEOW_MAT_EV_NONE;
    if (g->anim) ev = step_anim(g, dt_ms);
    if (dt_ms >= (uint32_t)g->left_ms) {
        g->left_ms = 0;
        g->over = 1;
        return APP_MEOW_MAT_EV_OVER;
    }
    g->left_ms -= (int)dt_ms;
    return ev;
}
