#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "app_meow_match.h"

static void fill_plain(app_meow_mat_t *g)
{
    int r, c;

    for (r = 0; r < APP_MEOW_MAT_H; r++) {
        for (c = 0; c < APP_MEOW_MAT_W; c++) {
            g->cell[r][c] = (uint8_t)((r + c * 2) % APP_MEOW_MAT_KIND);
        }
    }
}

static void assert_full(const app_meow_mat_t *g)
{
    int r, c;

    for (r = 0; r < APP_MEOW_MAT_H; r++) {
        for (c = 0; c < APP_MEOW_MAT_W; c++) {
            assert(g->cell[r][c] < APP_MEOW_MAT_KIND);
        }
    }
    assert(app_meow_mat_matches(g) == 0);
}

static int settle(app_meow_mat_t *g)
{
    int n = APP_MEOW_MAT_EV_NONE;
    int ev;

    while (app_meow_mat_busy(g) && !app_meow_mat_done(g)) {
        ev = app_meow_mat_tick(g, 40);
        if (ev == APP_MEOW_MAT_EV_NEXT || ev == APP_MEOW_MAT_EV_CLEAR) n = ev;
        if (ev == APP_MEOW_MAT_EV_OVER) break;
    }
    return n;
}

static int play_ok(app_meow_mat_t *g)
{
    int n = app_meow_mat_ok(g);
    int s = settle(g);

    if (s == APP_MEOW_MAT_EV_NEXT) return s;
    return n;
}

int main(void)
{
    app_meow_mat_t g;
    int seed, r, c, n;
    uint8_t a, b;

    for (seed = 0; seed < 64; seed++) {
        app_meow_mat_make(&g, (uint8_t)seed);
        assert(g.left_ms == APP_MEOW_MAT_TIME0);
        assert(g.score == 0);
        assert(g.cur_r == 0 && g.cur_c == 0);
        assert(!app_meow_mat_selected(&g));
        assert(app_meow_mat_matches(&g) == 0);
        {
            unsigned seen = 0;
            int k, j;

            for (k = 0; k < APP_MEOW_MAT_KIND; k++) {
                int id = app_meow_mat_good(&g, k);
                assert(id >= 0 && id < APP_MEOW_G_N);
                for (j = 0; j < k; j++) {
                    assert(app_meow_mat_good(&g, j) != id);
                }
                seen |= 1u << id;
            }
            assert(seen != 0);
        }
        for (r = 0; r < APP_MEOW_MAT_H; r++) {
            for (c = 0; c < APP_MEOW_MAT_W; c++) {
                assert(g.cell[r][c] < APP_MEOW_MAT_KIND);
            }
        }
    }

    {
        app_meow_mat_t a, b;
        int same = 1, k;

        app_meow_mat_make(&a, 0x11);
        app_meow_mat_make(&b, 0xC3);
        for (k = 0; k < APP_MEOW_MAT_KIND; k++) {
            if (app_meow_mat_good(&a, k) != app_meow_mat_good(&b, k)) same = 0;
        }
        assert(!same);
    }

    app_meow_mat_make(&g, 1);
    app_meow_mat_move(&g, 0, 0);
    assert(g.cur_r == 1);
    app_meow_mat_move(&g, 0, 1);
    assert(g.cur_r == APP_MEOW_MAT_H - 1);
    app_meow_mat_move(&g, 0, 1);
    assert(g.cur_r == 0);
    app_meow_mat_move(&g, 1, 0);
    assert(g.cur_c == 1);
    app_meow_mat_move(&g, 1, 1);
    assert(g.cur_c == APP_MEOW_MAT_W - 1);

    app_meow_mat_make(&g, 2);
    assert(app_meow_mat_ok(&g) == APP_MEOW_MAT_EV_SEL);
    assert(app_meow_mat_selected(&g));
    assert(g.sel_r == 0 && g.sel_c == 0);
    app_meow_mat_move(&g, 0, 0);
    assert(g.cur_r == 1 && g.cur_c == 0);
    app_meow_mat_move(&g, 0, 0);
    assert(g.cur_r == 0 && g.cur_c == 0);
    app_meow_mat_move(&g, 1, 0);
    assert(g.cur_r == 0 && g.cur_c == 1);
    app_meow_mat_move(&g, 1, 0);
    assert(g.cur_c <= 1);
    app_meow_mat_move(&g, 0, 1);
    assert(g.cur_r <= 1 && g.cur_c == 0);
    app_meow_mat_unsel(&g);
    assert(!app_meow_mat_selected(&g));

    app_meow_mat_make(&g, 3);
    fill_plain(&g);
    assert(app_meow_mat_matches(&g) == 0);
    g.cell[0][0] = 0;
    g.cell[0][1] = 0;
    g.cell[0][2] = 1;
    g.cell[1][2] = 0;
    g.cur_r = 0;
    g.cur_c = 2;
    assert(app_meow_mat_ok(&g) == APP_MEOW_MAT_EV_SEL);
    g.cur_r = 1;
    g.cur_c = 2;
    a = g.cell[0][2];
    b = g.cell[1][2];
    (void)a;
    (void)b;
    n = play_ok(&g);
    assert(n == APP_MEOW_MAT_EV_CLEAR || n == APP_MEOW_MAT_EV_NEXT);
    assert(g.score >= 3 * APP_MEOW_MAT_PTS);
    assert(g.left_ms > APP_MEOW_MAT_TIME0);
    assert(!app_meow_mat_selected(&g));
    assert(!app_meow_mat_busy(&g));
    assert_full(&g);

    app_meow_mat_make(&g, 4);
    fill_plain(&g);
    g.cur_r = 0;
    g.cur_c = 0;
    app_meow_mat_ok(&g);
    g.cur_r = 0;
    g.cur_c = 1;
    a = g.cell[0][0];
    b = g.cell[0][1];
    assert(app_meow_mat_ok(&g) == APP_MEOW_MAT_EV_REVERT);
    assert(g.cell[0][0] == a);
    assert(g.cell[0][1] == b);
    assert(g.score == 0);

    app_meow_mat_make(&g, 5);
    assert(app_meow_mat_tick(&g, 1000) == APP_MEOW_MAT_EV_NONE);
    assert(app_meow_mat_sec(&g) == (APP_MEOW_MAT_TIME0 - 1000 + 999) / 1000);
    assert(app_meow_mat_tick(&g, APP_MEOW_MAT_TIME0) == APP_MEOW_MAT_EV_OVER);
    assert(app_meow_mat_done(&g));
    assert(g.left_ms == 0);
    assert(app_meow_mat_ok(&g) == APP_MEOW_MAT_EV_NONE);

    app_meow_mat_make(&g, 6);
    g.score = 0;
    g.left_ms = APP_MEOW_MAT_TIME0;
    fill_plain(&g);
    g.cell[2][1] = 2;
    g.cell[2][2] = 2;
    g.cell[2][3] = 2;
    /* force a resolve via a dummy matched swap */
    g.cell[3][3] = 2;
    g.cell[2][3] = 1;
    g.cur_r = 2;
    g.cur_c = 3;
    app_meow_mat_ok(&g);
    g.cur_r = 3;
    g.cur_c = 3;
    n = play_ok(&g);
    assert(n == APP_MEOW_MAT_EV_CLEAR || n == APP_MEOW_MAT_EV_NEXT);
    assert(g.score % APP_MEOW_MAT_PTS == 0);
    assert(g.score >= 30);
    assert_full(&g);

    /* 连线先消失、周边不动，再垂直落下 */
    app_meow_mat_make(&g, 9);
    fill_plain(&g);
    g.cell[4][0] = 0;
    g.cell[4][1] = 0;
    g.cell[4][2] = 1;
    g.cell[5][2] = 0;
    a = g.cell[3][1];
    b = g.cell[4][3];
    {
        uint8_t side = g.cell[5][1];

        g.cur_r = 4;
        g.cur_c = 2;
        assert(app_meow_mat_ok(&g) == APP_MEOW_MAT_EV_SEL);
        g.cur_r = 5;
        g.cur_c = 2;
        n = app_meow_mat_ok(&g);
        assert(n == APP_MEOW_MAT_EV_CLEAR);
        assert(app_meow_mat_busy(&g));
        assert(g.anim == APP_MEOW_MAT_ST_VANISH);
        assert(g.cell[4][0] == 0 && g.cell[4][1] == 0 && g.cell[4][2] == 0);
        assert(g.mark[4][0] && g.mark[4][1] && g.mark[4][2]);
        assert(g.cell[3][1] == a);
        assert(g.cell[4][3] == b);
        assert(g.cell[5][1] == side);
        assert(app_meow_mat_tick(&g, APP_MEOW_MAT_VANISH_MS - 1) ==
               APP_MEOW_MAT_EV_NONE);
        assert(g.cell[4][1] == 0);
        assert(g.cell[3][1] == a);
        assert(g.cell[4][3] == b);
        app_meow_mat_tick(&g, 1);
        assert(g.anim == APP_MEOW_MAT_ST_FALL);
        assert(g.cell[4][1] == a);
        assert(g.fall[4][1] >= 1);
        settle(&g);
        assert(!app_meow_mat_busy(&g));
        assert(g.cell[4][1] == a);
        assert_full(&g);
    }

    app_meow_mat_make(&g, 7);
    memset(g.cell, APP_MEOW_MAT_EMPTY, sizeof(g.cell));
    g.cell[5][0] = 0;
    g.cell[5][1] = 0;
    g.cell[5][2] = 1;
    g.cell[4][2] = 0;
    g.score = 40;
    g.left_ms = 20000;
    g.cur_r = 4;
    g.cur_c = 2;
    assert(app_meow_mat_ok(&g) == APP_MEOW_MAT_EV_SEL);
    g.cur_r = 5;
    g.cur_c = 2;
    n = play_ok(&g);
    assert(n == APP_MEOW_MAT_EV_CLEAR || n == APP_MEOW_MAT_EV_NEXT);
    assert(g.score >= 40 + 3 * APP_MEOW_MAT_PTS);
    assert(g.left_ms > 20000);
    assert(!app_meow_mat_selected(&g));
    assert_full(&g);

    app_meow_mat_make(&g, 8);
    memset(g.cell, APP_MEOW_MAT_EMPTY, sizeof(g.cell));
    g.cell[5][0] = 0;
    g.cell[5][1] = 0;
    g.cell[5][2] = 1;
    g.cell[4][2] = 0;
    g.cell[0][0] = 1;
    g.cell[0][5] = 2;
    g.score = 10;
    g.left_ms = 15000;
    g.cur_r = 4;
    g.cur_c = 2;
    assert(app_meow_mat_ok(&g) == APP_MEOW_MAT_EV_SEL);
    g.cur_r = 5;
    g.cur_c = 2;
    n = play_ok(&g);
    assert(n == APP_MEOW_MAT_EV_CLEAR || n == APP_MEOW_MAT_EV_NEXT);
    assert(g.score >= 10 + 3 * APP_MEOW_MAT_PTS);
    assert(g.left_ms > 15000);
    assert_full(&g);

    puts("ok");
    return 0;
}
