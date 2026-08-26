#include <assert.h>
#include <stdio.h>
#include "app_meow_rhythm.h"

static int first_pending(const app_meow_rhy_t *g)
{
    int i;

    for (i = 0; i < g->n_n; i++) {
        if (g->st[i] == APP_MEOW_RHY_NONE) return i;
    }
    return -1;
}

static void play_one(app_meow_rhy_t *g)
{
    int i = first_pending(g);
    uint32_t t;
    int r;

    assert(i >= 0);
    t = g->n[i].t_ms;
    r = app_meow_rhy_press(g, g->n[i].lane, t);
    assert(r == APP_MEOW_RHY_G_PERF || r == APP_MEOW_RHY_G_GOOD);
    if (g->n[i].hold_ms) {
        r = app_meow_rhy_release(g, g->n[i].lane, t + g->n[i].hold_ms);
        assert(r == APP_MEOW_RHY_G_PERF || r == APP_MEOW_RHY_G_GOOD);
    }
    app_meow_rhy_tick(g, t + 1);
}

static void check_open(const app_meow_rhy_t *g)
{
    int i;

    assert(g->n_n >= 2 && g->n_n <= APP_MEOW_RHY_MAX);
    assert(g->travel_ms == APP_MEOW_RHY_TRAVEL);
    assert(g->unit_ms == 1200);
    assert(g->stage == 0);
    assert(!g->failed);
    for (i = 0; i < g->n_n; i++) {
        assert(g->n[i].lane <= 1);
        assert(g->n[i].pitch <= 5);
        assert(g->n[i].t_ms >= APP_MEOW_RHY_TRAVEL);
        if (g->n[i].hold_ms) assert(g->n[i].hold_ms >= 400);
        if (i > 0) {
            uint32_t prev = g->n[i - 1].t_ms + g->n[i - 1].hold_ms;
            uint32_t dt = g->n[i].t_ms - g->n[i - 1].t_ms;
            assert(g->n[i].t_ms >= prev);
            if (g->n[i].lane != g->n[i - 1].lane) {
                assert(dt >= APP_MEOW_RHY_GOOD + APP_MEOW_RHY_SWITCH);
            }
        }
    }
    assert(g->n[1].t_ms - g->n[0].t_ms >= 1100);
}

int main(void)
{
    app_meow_rhy_t a, b;
    int seed, i, cues;
    uint32_t t;

    assert(app_meow_rhy_gap(0) == 1200);
    assert(app_meow_rhy_gap(4) == 400);
    assert(app_meow_rhy_gap(9) == 400);

    for (seed = 0; seed < 64; seed++) {
        app_meow_rhy_make(&a, (uint8_t)seed);
        check_open(&a);
        app_meow_rhy_make(&b, (uint8_t)seed);
        assert(a.n_n == b.n_n);
        assert(a.n[0].t_ms == b.n[0].t_ms);
        assert(a.n[0].lane == b.n[0].lane);
    }

    app_meow_rhy_make(&a, 0x3C);
    for (i = 0; i < APP_MEOW_RHY_NEED * 4 + 4; i++) play_one(&a);
    assert(a.stage == APP_MEOW_RHY_STAGE_N - 1);
    assert(a.unit_ms == 400);
    assert(!a.failed);
    assert(!app_meow_rhy_done(&a, a.n[0].t_ms + 10000));
    assert(a.hits >= APP_MEOW_RHY_NEED * 4);
    assert(a.n_n <= APP_MEOW_RHY_MAX);
    for (i = 1; i < a.n_n; i++) {
        if (a.n[i].lane != a.n[i - 1].lane) {
            assert(a.n[i].t_ms - a.n[i - 1].t_ms >=
                   APP_MEOW_RHY_GOOD + APP_MEOW_RHY_SWITCH);
        }
    }

    app_meow_rhy_make(&a, 0x11);
    t = a.n[0].t_ms + APP_MEOW_RHY_GOOD + 1;
    app_meow_rhy_tick(&a, t);
    assert(a.failed);
    assert(a.misses >= 1);
    assert(app_meow_rhy_done(&a, t + 280));
    assert(!app_meow_rhy_done(&a, t));

    app_meow_rhy_make(&a, 0x22);
    assert(app_meow_rhy_press(&a, a.n[0].lane, 0) == 0);
    for (i = 0; i < 40 && a.n[0].hold_ms == 0; i++) play_one(&a);
    if (a.n[first_pending(&a)].hold_ms) {
        int p = first_pending(&a);
        assert(app_meow_rhy_press(&a, a.n[p].lane, a.n[p].t_ms) >
               APP_MEOW_RHY_G_NONE);
        assert(app_meow_rhy_release(&a, a.n[p].lane, a.n[p].t_ms + 40) ==
               APP_MEOW_RHY_G_MISS);
        assert(a.failed);
    }

    app_meow_rhy_make(&a, 0x08);
    cues = 0;
    for (t = 0; t < a.n[0].t_ms + 40; t += 40) {
        if (app_meow_rhy_poll_cue(&a, t) >= 0) cues++;
    }
    assert(cues >= 1);
    assert(app_meow_rhy_near(&a, a.n[0].lane, a.n[0].t_ms));
    assert(!app_meow_rhy_near(&a, a.n[0].lane, 0));
    assert(app_meow_rhy_hz(0) == 262);
    assert(app_meow_rhy_hz(5) == 523);

    app_meow_rhy_make(&a, 0x22);
    {
        int p = first_pending(&a);
        int before;
        while (p >= 0 && a.n[p].hold_ms) {
            play_one(&a);
            p = first_pending(&a);
        }
        assert(p >= 0);
        before = a.score;
        assert(app_meow_rhy_press(&a, a.n[p].lane, a.n[p].t_ms) ==
               APP_MEOW_RHY_G_PERF);
        assert(a.score == before + APP_MEOW_RHY_PTS_TAP_PERF);
    }

    app_meow_rhy_make(&a, 0x33);
    {
        int p = first_pending(&a);
        int before;
        while (p >= 0 && a.n[p].hold_ms) {
            play_one(&a);
            p = first_pending(&a);
        }
        assert(p >= 0);
        before = a.score;
        assert(app_meow_rhy_press(&a, a.n[p].lane,
                                 a.n[p].t_ms + APP_MEOW_RHY_PERF + 1) ==
               APP_MEOW_RHY_G_GOOD);
        assert(a.score == before + APP_MEOW_RHY_PTS_TAP_GOOD);
    }

    {
        int found_perf = 0, found_good = 0;
        for (seed = 0; seed < 128 && (!found_perf || !found_good); seed++) {
            int p;
            app_meow_rhy_make(&a, (uint8_t)seed);
            for (i = 0; i < 80; i++) {
                int off, expect, want, before, r;
                p = first_pending(&a);
                if (p < 0) break;
                if (a.n[p].hold_ms == 0) {
                    play_one(&a);
                    continue;
                }
                off = found_perf ? (APP_MEOW_RHY_PERF + 1) : 0;
                expect = found_perf ? APP_MEOW_RHY_PTS_HOLD_GOOD
                                    : APP_MEOW_RHY_PTS_HOLD_PERF;
                want = found_perf ? APP_MEOW_RHY_G_GOOD : APP_MEOW_RHY_G_PERF;
                before = a.score;
                r = app_meow_rhy_press(&a, a.n[p].lane,
                                       a.n[p].t_ms + (uint32_t)off);
                assert(r == want);
                r = app_meow_rhy_release(&a, a.n[p].lane,
                                         a.n[p].t_ms + a.n[p].hold_ms);
                assert(r == want);
                assert(a.score == before + expect);
                if (want == APP_MEOW_RHY_G_PERF) found_perf = 1;
                else found_good = 1;
                break;
            }
        }
        assert(found_perf && found_good);
    }

    puts("ok");
    return 0;
}
