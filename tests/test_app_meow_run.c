#include <assert.h>
#include <stdio.h>
#include "app_meow_run.h"

static int count_kind(const app_meow_run_t *g, int kind)
{
    int n = 0;
    int i;

    for (i = 0; i < g->n; i++) {
        if (g->o[i].st == APP_MEOW_RUN_LIVE && g->o[i].kind == kind) n++;
    }
    return n;
}

int main(void)
{
    app_meow_run_t g;
    int seed, seen_haz, seen_clear;

    assert(app_meow_run_haz_pct(0) == 0);
    assert(app_meow_run_haz_pct(APP_MEOW_RUN_HAZ_START - 1) == 0);
    assert(app_meow_run_haz_pct(APP_MEOW_RUN_HAZ_START) == 30);
    assert(app_meow_run_haz_pct(29999) == 30);
    assert(app_meow_run_haz_pct(30000) == 60);
    assert(app_meow_run_haz_pct(40000) == 90);
    assert(app_meow_run_haz_pct(APP_MEOW_RUN_HAZ_CAP) == 90);
    assert(app_meow_run_haz_pct(80000) == 90);

    assert(app_meow_run_gap_n(0) == 0);
    assert(app_meow_run_gap_n(APP_MEOW_RUN_GAP_START - 1) == 0);
    assert(app_meow_run_gap_n(APP_MEOW_RUN_GAP_START) == 1);
    assert(app_meow_run_gap_n(39999) == 1);
    assert(app_meow_run_gap_n(40000) == 2);
    assert(app_meow_run_gap_n(50000) == 3);
    assert(app_meow_run_gap_n(APP_MEOW_RUN_GAP_CAP) == 3);
    assert(app_meow_run_gap_n(90000) == 3);
    assert(app_meow_run_item_ms(0) == APP_MEOW_RUN_ITEM_MS);
    assert(app_meow_run_item_ms(30000) == 2700);
    assert(app_meow_run_item_ms(40000) == 2400);
    assert(app_meow_run_item_ms(50000) == 2100);
    assert(app_meow_run_item_ms(60000) == 2100);
    assert(app_meow_run_haz_ms(0) == APP_MEOW_RUN_HAZ_EVERY);
    assert(app_meow_run_haz_ms(30000) == 1800);
    assert(app_meow_run_haz_ms(50000) == 1400);
    assert(app_meow_run_spd_n(0) == 0);
    assert(app_meow_run_spd_n(APP_MEOW_RUN_SPD_START - 1) == 0);
    assert(app_meow_run_spd_n(APP_MEOW_RUN_SPD_START) == 1);
    assert(app_meow_run_spd_n(70000) == 2);
    assert(app_meow_run_spd_n(80000) == 3);
    assert(app_meow_run_spd_n(90000) == 4);
    assert(app_meow_run_spd_n(APP_MEOW_RUN_SPD_CAP) == 4);
    assert(app_meow_run_spd_n(120000) == 4);
    assert(app_meow_run_travel(0) == APP_MEOW_RUN_TRAVEL);
    assert(app_meow_run_travel(60000) == 2181);
    assert(app_meow_run_travel(70000) == 2000);
    assert(app_meow_run_travel(100000) == 1714);

    app_meow_run_make(&g, 0x21);
    assert(g.lane == 1);
    assert(g.last_good == -1);
    assert(!app_meow_run_done(&g));
    assert(app_meow_run_got_n(&g) == 0);
    app_meow_run_move(&g, -1);
    assert(g.lane == 0);
    app_meow_run_move(&g, -1);
    assert(g.lane == 0);
    app_meow_run_move(&g, 1);
    app_meow_run_move(&g, 1);
    app_meow_run_move(&g, 1);
    assert(g.lane == 2);

    app_meow_run_make(&g, 0x11);
    assert(app_meow_run_step(&g, 0, APP_MEOW_G_FRUIT) == APP_MEOW_RUN_EV_NONE);
    assert(g.n == 1);
    assert(g.o[0].kind == APP_MEOW_RUN_ITEM);
    assert(g.o[0].good == APP_MEOW_G_FRUIT);
    assert(app_meow_run_prog(&g.o[0], 0) == 0);
    assert(app_meow_run_prog(&g.o[0], APP_MEOW_RUN_TRAVEL) == 1000);
    assert(count_kind(&g, APP_MEOW_RUN_HAZ) == 0);
    assert(app_meow_run_step(&g, APP_MEOW_RUN_ITEM_MS - 1, APP_MEOW_G_WATER) ==
           APP_MEOW_RUN_EV_NONE);
    assert(count_kind(&g, APP_MEOW_RUN_ITEM) == 0);
    assert(g.next_item == APP_MEOW_RUN_ITEM_MS);
    assert(app_meow_run_step(&g, APP_MEOW_RUN_ITEM_MS, APP_MEOW_G_WATER) ==
           APP_MEOW_RUN_EV_NONE);
    assert(count_kind(&g, APP_MEOW_RUN_ITEM) == 1);
    assert(g.o[0].good == APP_MEOW_G_WATER);

    app_meow_run_make(&g, 0x11);
    app_meow_run_step(&g, 0, APP_MEOW_G_ONIGIRI);
    g.lane = g.o[0].lane;
    assert(app_meow_run_step(&g, 2200, -1) == APP_MEOW_RUN_EV_ITEM);
    assert(g.got[APP_MEOW_G_ONIGIRI] == 1);
    assert(g.last_good == APP_MEOW_G_ONIGIRI);
    assert(g.last_at == 2200);
    assert(app_meow_run_got_n(&g) == 1);
    assert(!app_meow_run_done(&g));

    app_meow_run_make(&g, 0x11);
    app_meow_run_step(&g, 0, APP_MEOW_G_ONIGIRI);
    g.lane = (uint8_t)((g.o[0].lane + 1) % APP_MEOW_RUN_LANES);
    assert(app_meow_run_step(&g, 2900, -1) == APP_MEOW_RUN_EV_NONE);
    assert(g.got[APP_MEOW_G_ONIGIRI] == 0);
    assert(count_kind(&g, APP_MEOW_RUN_ITEM) == 0);
    assert(!app_meow_run_done(&g));
    assert(app_meow_run_step(&g, APP_MEOW_RUN_HAZ_START - 1, -1) ==
           APP_MEOW_RUN_EV_NONE);
    assert(!app_meow_run_done(&g));

    app_meow_run_make(&g, 0x33);
    assert(app_meow_run_put(&g, 0, APP_MEOW_RUN_HAZ, 1, 2) >= 0);
    assert(g.o[0].good == 2);
    assert(app_meow_run_step(&g, 2200, -1) == APP_MEOW_RUN_EV_HAZ);
    assert(app_meow_run_done(&g));
    assert(app_meow_run_step(&g, 3000, APP_MEOW_G_COLA) == APP_MEOW_RUN_EV_NONE);

    app_meow_run_make(&g, 0x44);
    assert(app_meow_run_put(&g, 0, APP_MEOW_RUN_ITEM, 0, APP_MEOW_G_SALAD) >= 0);
    g.lane = 0;
    assert(app_meow_run_step(&g, 2200, -1) == APP_MEOW_RUN_EV_ITEM);
    assert(g.got[APP_MEOW_G_SALAD] == 1);

    /* 食物还叠在身上时换到该道，仍该吃到，不能当漏接结束。 */
    app_meow_run_make(&g, 0x55);
    assert(app_meow_run_put(&g, 0, APP_MEOW_RUN_ITEM, 2, APP_MEOW_G_FRUIT) >= 0);
    g.next_item = 100000;
    g.next_haz = 100000;
    g.lane = 0;
    assert(app_meow_run_step(&g, 2500, -1) == APP_MEOW_RUN_EV_NONE);
    assert(!app_meow_run_done(&g));
    g.lane = 2;
    assert(app_meow_run_step(&g, 2500, -1) == APP_MEOW_RUN_EV_ITEM);
    assert(g.got[APP_MEOW_G_FRUIT] == 1);
    assert(!app_meow_run_done(&g));

    app_meow_run_make(&g, 1);
    app_meow_run_step(&g, APP_MEOW_RUN_HAZ_START - 1, 0);
    assert(count_kind(&g, APP_MEOW_RUN_HAZ) == 0);

    seen_haz = 0;
    seen_clear = 0;
    for (seed = 0; seed < 256; seed++) {
        app_meow_run_make(&g, (uint8_t)seed);
        app_meow_run_step(&g, APP_MEOW_RUN_HAZ_START, 0);
        if (count_kind(&g, APP_MEOW_RUN_HAZ) > 0) seen_haz++;
        else seen_clear++;
    }
    assert(seen_haz > 0 && seen_clear > 0);

    seen_haz = 0;
    for (seed = 0; seed < 32; seed++) {
        app_meow_run_make(&g, (uint8_t)seed);
        app_meow_run_step(&g, APP_MEOW_RUN_HAZ_CAP, 0);
        if (count_kind(&g, APP_MEOW_RUN_HAZ) >= 1 || app_meow_run_done(&g)) {
            seen_haz++;
        }
    }
    assert(seen_haz > 16);

    app_meow_run_make(&g, 0x11);
    g.next_item = 30000;
    g.next_haz = 100000;
    assert(app_meow_run_step(&g, 30000, APP_MEOW_G_WATER) == APP_MEOW_RUN_EV_NONE);
    assert(g.next_item == 32700);
    assert(g.n >= 1);
    assert(g.o[0].travel == APP_MEOW_RUN_TRAVEL);

    app_meow_run_make(&g, 0x11);
    g.next_item = 100000;
    g.next_haz = 100000;
    assert(app_meow_run_put(&g, 60000, APP_MEOW_RUN_ITEM, 1, APP_MEOW_G_FRUIT) >= 0);
    assert(g.o[0].travel == 2181);
    assert(app_meow_run_prog(&g.o[0], 60000 + 2181) == 1000);

    puts("ok");
    return 0;
}
