#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "app_meow.h"

static app_meow_t pet;

static void reset_at(uint32_t t, uint8_t rng)
{
    app_meow_reset(&pet, t, rng);
    assert(app_meow_valid(&pet));
    assert(pet.stage == APP_MEOW_EGG);
}

int main(void)
{
    app_meow_t z;
    memset(&z, 0, sizeof(z));
    assert(!app_meow_valid(&z));

    reset_at(1000, 0x11);
    assert(pet.hunger == APP_MEOW_STAT_MAX);
    assert(pet.happy == APP_MEOW_STAT_MAX);
    assert(pet.health == APP_MEOW_STAT_MAX);
    assert(app_meow_act(&pet, APP_MEOW_FEED) == APP_MEOW_EGG_WAIT);
    assert(app_meow_play(&pet, 0) < 0);

    /* 蛋孵化固定 60 真实秒,不跟游戏分钟走。 */
    app_meow_advance(&pet, 1000 + 59, 12);
    assert(pet.stage == APP_MEOW_EGG);
    app_meow_advance(&pet, 1000 + APP_MEOW_HATCH_SEC, 12);
    assert(pet.stage == APP_MEOW_BABY);
    assert(pet.age_min == 0);
    assert(pet.species == APP_MEOW_SP_BABY);
    assert(pet.level == 1);
    assert(pet.hunger == 80);
    assert(pet.happy == 70);
    assert(pet.health == 90);
    assert(pet.dirt == 10);
    assert(app_meow_xp_need(1) == 20);
    assert(app_meow_good_tier(APP_MEOW_G_ONIGIRI) == 0);
    assert(app_meow_good_tier(APP_MEOW_G_SHOVEL) == 3);
    assert(app_meow_inv(&pet, APP_MEOW_G_ONIGIRI) == 2);
    assert(app_meow_inv(&pet, APP_MEOW_G_FRUIT) == 1);
    assert(app_meow_inv(&pet, APP_MEOW_G_WATER) == 2);
    assert(app_meow_inv(&pet, APP_MEOW_G_SOAP) == 1);
    assert(app_meow_inv(&pet, APP_MEOW_G_TRASH) == 1);
    assert(app_meow_inv(&pet, APP_MEOW_G_TISSUE) == 1);
    assert(app_meow_inv(&pet, APP_MEOW_G_STOMACH) == 1);
    assert(app_meow_inv(&pet, APP_MEOW_G_COLD) == 1);
    assert(app_meow_dur(&pet, APP_MEOW_G_WATER) == 3);
    assert(pet.name[0] == 0);
    assert(pet.named == 0);
    assert(app_meow_name(&pet)[0] == 0);
    app_meow_set_name(&pet, "  Mochi  ");
    assert(strcmp(app_meow_name(&pet), "Mochi") == 0);
    app_meow_set_name(&pet, "   ");
    assert(app_meow_name(&pet)[0] == 0);
    app_meow_set_name(&pet, "团团");
    assert(strcmp(app_meow_name(&pet), "团团") == 0);
    app_meow_set_name(&pet, "小小小小小");
    assert(strcmp(app_meow_name(&pet), "小小小小小") == 0);
    app_meow_set_name(&pet, "小小小小小小");
    assert(strcmp(app_meow_name(&pet), "小小小小小") == 0);
    app_meow_set_name(&pet, "HelloWorld");
    assert(strcmp(app_meow_name(&pet), "HelloWorld") == 0);
    app_meow_set_name(&pet, "HelloWorldX");
    assert(strcmp(app_meow_name(&pet), "HelloWorld") == 0);
    assert(app_meow_good_cat(APP_MEOW_G_BURGER) == APP_MEOW_CAT_FOOD);
    assert(app_meow_good_cat(APP_MEOW_G_COLD) == APP_MEOW_CAT_MED);
    assert(app_meow_good_cat(APP_MEOW_G_SHOVEL) == APP_MEOW_CAT_GEAR);
    assert(app_meow_good_gain(APP_MEOW_G_BURGER) == 30);
    assert(app_meow_good_gain(APP_MEOW_G_ONIGIRI) == 24);
    assert(app_meow_good_gain(APP_MEOW_G_SALAD) == 28);
    assert(app_meow_good_gain(APP_MEOW_G_FRUIT) == 24);
    assert(app_meow_good_gain(APP_MEOW_G_WATER) == 14);
    assert(app_meow_good_gain(APP_MEOW_G_COLA) == 20);
    assert(app_meow_good_gain(APP_MEOW_G_SOAP) == 48);
    assert(app_meow_good_gain(APP_MEOW_G_SHOVEL) == 0);

    app_meow_set_name(&pet, "Mochi");
    pet.named = 1;
    pet.age_min = APP_MEOW_BABY_MIN - 1;
    pet.level = 1;
    app_meow_advance(&pet, pet.last_sec + APP_MEOW_SEC_PER_MIN, 12);
    assert(pet.stage == APP_MEOW_BABY);
    assert(strcmp(app_meow_name(&pet), "Mochi") == 0);
    pet.age_min = APP_MEOW_BABY_MIN - 1;
    pet.level = APP_MEOW_BABY_LV;
    app_meow_advance(&pet, pet.last_sec + APP_MEOW_SEC_PER_MIN, 12);
    assert(pet.stage == APP_MEOW_CHILD);
    assert(pet.species == APP_MEOW_SP_CHILD);
    assert(strcmp(app_meow_name(&pet), "Mochi") == 0);
    assert(pet.hunger == 90);
    assert(pet.health == 95);

    pet.care_good = 8;
    pet.care_miss = 0;
    pet.age_min = APP_MEOW_CHILD_MIN - 1;
    pet.level = APP_MEOW_CHILD_LV;
    app_meow_advance(&pet, pet.last_sec + APP_MEOW_SEC_PER_MIN, 12);
    assert(pet.stage == APP_MEOW_TEEN);
    assert(pet.species == APP_MEOW_SP_TEEN_A || pet.species == APP_MEOW_SP_TEEN_B);
    assert(strcmp(app_meow_name(&pet), "Mochi") == 0);
    assert(pet.hunger == 95);

    pet.age_min = APP_MEOW_TEEN_MIN - 1;
    pet.level = APP_MEOW_TEEN_LV;
    app_meow_advance(&pet, pet.last_sec + APP_MEOW_SEC_PER_MIN, 12);
    assert(pet.stage == APP_MEOW_ADULT);
    assert(strcmp(app_meow_name(&pet), "Mochi") == 0);

    unsigned seen = 0;
    for (int i = 0; i < 24; i++) {
        app_meow_t q;
        app_meow_reset(&q, 30000, (uint8_t)(0x21 + i * 3));
        app_meow_advance(&q, 30000 + APP_MEOW_HATCH_SEC, 12);
        q.care_good = (uint8_t)(i % 9);
        q.care_miss = (uint8_t)((23 - i) % 7);
        q.stage = APP_MEOW_TEEN;
        q.level = APP_MEOW_TEEN_LV;
        q.age_min = APP_MEOW_TEEN_MIN - 1;
        app_meow_advance(&q, q.last_sec + APP_MEOW_SEC_PER_MIN, 12);
        assert(q.stage == APP_MEOW_ADULT);
        assert(q.species >= APP_MEOW_SP_ADULT_0 && q.species <= APP_MEOW_SP_MAX);
        seen |= 1u << q.species;
    }
    int kinds = 0;
    for (int s = APP_MEOW_SP_ADULT_0; s <= APP_MEOW_SP_MAX; s++) {
        if (seen & (1u << s)) kinds++;
    }
    assert(kinds >= 3);

    pet.hunger = 80;
    pet.happy = 70;
    pet.health = 90;
    pet.dirt = 10;
    pet.alert_ack = 0;
    int d = -1, lv = -1;
    assert(app_meow_alert_poll(&pet, &d, &lv) == 0);
    pet.hunger = 51;
    assert(app_meow_alert_poll(&pet, &d, &lv) == 0);
    pet.hunger = 50;
    assert(app_meow_alert_poll(&pet, &d, &lv) == 1);
    assert(d == APP_MEOW_D_HUNGER && lv == APP_MEOW_ALERT_WARN);
    assert(app_meow_alert_poll(&pet, &d, &lv) == 0);
    pet.hunger = 30;
    assert(app_meow_alert_poll(&pet, &d, &lv) == 1);
    assert(lv == APP_MEOW_ALERT_HIT);
    pet.hunger = 10;
    assert(app_meow_alert_poll(&pet, &d, &lv) == 1);
    assert(lv == APP_MEOW_ALERT_CRIT);
    pet.hunger = 80;
    assert(app_meow_alert_poll(&pet, &d, &lv) == 0);
    pet.hunger = 50;
    assert(app_meow_alert_poll(&pet, &d, &lv) == 1);
    assert(lv == APP_MEOW_ALERT_WARN);
    pet.health = 30;
    assert(app_meow_danger_lv(&pet, APP_MEOW_D_HEALTH) == APP_MEOW_ALERT_HIT);
    pet.happy = 10;
    assert(app_meow_danger_lv(&pet, APP_MEOW_D_HAPPY) == APP_MEOW_ALERT_CRIT);
    pet.health = 90;
    pet.happy = 70;

    pet.hunger = APP_MEOW_STAT_MAX;
    assert(app_meow_act(&pet, APP_MEOW_FEED) == APP_MEOW_FULL);
    assert(app_meow_act(&pet, APP_MEOW_CLEAN) == APP_MEOW_NONE);
    assert(app_meow_can(&pet, APP_MEOW_PLAY));

    uint8_t h0 = pet.hunger;
    pet.hunger = 20;
    uint16_t food0 = app_meow_inv(&pet, APP_MEOW_G_ONIGIRI);
    assert(app_meow_act(&pet, APP_MEOW_FEED) == APP_MEOW_OK);
    assert(pet.hunger == 42);
    assert(app_meow_inv(&pet, APP_MEOW_G_ONIGIRI) == food0 - 1);
    pet.hunger = h0;

    pet.hunger = 20;
    pet.poop_in = 0;
    pet.weight = 20;
    assert(app_meow_act(&pet, APP_MEOW_DRINK) == APP_MEOW_OK);
    assert(pet.hunger == 28);
    assert(pet.poop_in == 0);
    assert(pet.weight == 20);
    pet.hunger = APP_MEOW_STAT_MAX;
    assert(app_meow_act(&pet, APP_MEOW_DRINK) == APP_MEOW_FULL);

    pet.happy = 20;
    assert(app_meow_act(&pet, APP_MEOW_PET) == APP_MEOW_OK);
    assert(pet.happy == 32);
    pet.happy = APP_MEOW_STAT_MAX;
    assert(app_meow_act(&pet, APP_MEOW_PET) == APP_MEOW_NONE);

    pet.happy = 20;
    pet.hunger = 20;
    assert(app_meow_act(&pet, APP_MEOW_WALK) == APP_MEOW_OK);
    assert(pet.happy == 30);
    assert(pet.hunger == 12);

    pet.poop = 1;
    pet.happy = 20;
    assert(app_meow_act(&pet, APP_MEOW_BATH) == APP_MEOW_OK);
    assert(pet.poop == 0);
    assert(pet.happy == 28);
    pet.poop = 0;
    pet.happy = APP_MEOW_STAT_MAX;
    assert(app_meow_act(&pet, APP_MEOW_BATH) == APP_MEOW_NONE);

    pet.sleeping = 0;
    pet.lights_off = 0;
    assert(app_meow_act(&pet, APP_MEOW_BED) == APP_MEOW_OK);
    assert(pet.lights_off && pet.sleeping);
    assert(app_meow_act(&pet, APP_MEOW_BED) == APP_MEOW_NONE);
    assert(app_meow_can(&pet, APP_MEOW_BED));
    assert(app_meow_can(&pet, APP_MEOW_PLAY));
    assert(app_meow_act(&pet, APP_MEOW_FEED) == APP_MEOW_SLEEP);
    pet.sleeping = 0;
    pet.lights_off = 0;

    /* Night: 21:00..08:00 sleeps. Lights on: still feed; lights off: lock. */
    pet.hunger = 20;
    app_meow_give(&pet, APP_MEOW_G_ONIGIRI, 1);
    app_meow_advance(&pet, pet.last_sec + APP_MEOW_SEC_PER_MIN, 22);
    assert(pet.sleeping);
    assert(!pet.lights_off);
    assert(app_meow_bed_call(&pet));
    assert(app_meow_can(&pet, APP_MEOW_FEED));
    assert(app_meow_act(&pet, APP_MEOW_FEED) == APP_MEOW_OK);
    assert(app_meow_can(&pet, APP_MEOW_PLAY));
    assert(app_meow_play_apply(&pet, 1) == 1);
    assert(pet.sleeping);
    assert(!app_meow_trip_can(&pet));
    assert(app_meow_act(&pet, APP_MEOW_LIGHT) == APP_MEOW_OK);
    assert(pet.lights_off);
    assert(app_meow_rest_lock(&pet));
    assert(!app_meow_can(&pet, APP_MEOW_FEED));
    assert(app_meow_act(&pet, APP_MEOW_FEED) == APP_MEOW_SLEEP);
    pet.hunger = APP_MEOW_STAT_MAX;
    pet.happy = APP_MEOW_STAT_MAX;
    pet.health = APP_MEOW_STAT_MAX;
    pet.sick = 0;
    pet.poop = 0;
    pet.alert_ack = 0;
    assert(!app_meow_bed_call(&pet));
    app_meow_act(&pet, APP_MEOW_LIGHT);
    assert(!pet.lights_off);
    assert(app_meow_bed_call(&pet));

    /* Unknown clock does not force sleep. */
    pet.sleeping = 0;
    app_meow_advance(&pet, pet.last_sec + APP_MEOW_SEC_PER_MIN, -1);
    assert(!pet.sleeping);

    assert(app_meow_asleep_at(21, 21, 8));
    assert(app_meow_asleep_at(7, 21, 8));
    assert(!app_meow_asleep_at(8, 21, 8));
    assert(!app_meow_asleep_at(20, 21, 8));
    assert(!app_meow_asleep_at(12, 21, 8));
    assert(!app_meow_asleep_at(-1, 21, 8));
    assert(!app_meow_asleep_at(22, 21, 21));
    assert(app_meow_asleep_at(13, 13, 15));
    assert(app_meow_asleep_at(14, 13, 15));
    assert(!app_meow_asleep_at(15, 13, 15));
    assert(app_meow_asleep_at(23, 23, 9));
    assert(app_meow_asleep_at(0, 23, 9));
    assert(!app_meow_asleep_at(9, 23, 9));
    pet.sleeping = 0;
    app_meow_advance_night(&pet, pet.last_sec + APP_MEOW_SEC_PER_MIN, 22, 23, 8);
    assert(!pet.sleeping);
    app_meow_advance_night(&pet, pet.last_sec + APP_MEOW_SEC_PER_MIN, 23, 23, 8);
    assert(pet.sleeping);
    pet.sleeping = 0;

    int saw_win = 0, saw_lose = 0;
    for (int i = 0; i < 32; i++) {
        pet.happy = 2;
        pet.sleeping = 0;
        pet.stage = APP_MEOW_BABY;
        int r = app_meow_play(&pet, i & 1);
        assert(r == 0 || r == 1);
        if (r) saw_win++;
        else saw_lose++;
    }
    assert(saw_win && saw_lose);

    pet.happy = 20;
    pet.sleeping = 0;
    pet.stage = APP_MEOW_BABY;
    assert(app_meow_play_apply(&pet, 1) == 1);
    assert(pet.happy == 34);
    assert(app_meow_play_apply(&pet, 0) == 0);
    assert(pet.happy == 26);
    assert(app_meow_play_tier(0) == 0);
    assert(app_meow_play_tier(99) == 0);
    assert(app_meow_play_tier(100) == 1);
    assert(app_meow_play_tier(499) == 4);
    assert(app_meow_play_tier(500) == 5);
    assert(app_meow_play_tier(900) == 5);
    assert(app_meow_play_swim_pct(200) == 0);
    assert(app_meow_play_swim_pct(300) == 30);
    assert(app_meow_play_swim_pct(400) == 60);
    assert(app_meow_play_swim_pct(500) == 90);
    assert(app_meow_play_swim_pct(900) == 90);
    assert(app_meow_play_koi_pct(299) == 0);
    assert(app_meow_play_koi_pct(300) == 30);
    assert(app_meow_play_koi_pct(400) == 60);
    assert(app_meow_play_koi_pct(500) == 90);
    assert(app_meow_play_koi_pct(900) == 90);
    assert(app_meow_play_prize(&pet, 0) == 0);
    assert(app_meow_play_prize(&pet, 40) == 0);
    assert(app_meow_play_prize(&pet, 100) == 1);
    assert(app_meow_play_prize(&pet, 250) == 2);
    {
        uint8_t got[APP_MEOW_G_N];
        int n = app_meow_play_prize(&pet, 500);
        int sum = 0, i;
        app_meow_last_prizes(got);
        for (i = 0; i < APP_MEOW_G_N; i++) sum += got[i];
        assert(n == 5);
        assert(sum == n);
    }
    {
        uint8_t take[APP_MEOW_G_N];
        uint8_t prize[APP_MEOW_G_N];
        uint16_t rice = app_meow_inv(&pet, APP_MEOW_G_ONIGIRI);
        int n, i, sum = 0;

        memset(take, 0, sizeof(take));
        take[APP_MEOW_G_ONIGIRI] = 2;
        take[APP_MEOW_G_WATER] = 1;
        n = app_meow_run_prize(&pet, take);
        app_meow_last_prizes(prize);
        for (i = 0; i < APP_MEOW_G_N; i++) sum += prize[i];
        assert(n == 3);
        assert(sum == 3);
        assert(prize[APP_MEOW_G_ONIGIRI] == 2);
        assert(prize[APP_MEOW_G_WATER] == 1);
        assert(app_meow_inv(&pet, APP_MEOW_G_ONIGIRI) == rice + 2);
        assert(app_meow_roll(&pet, APP_MEOW_LOOT_PLAY) >= 0);
    }

    /* Hunger tick: 20 awake minutes with empty stomach cuts health. */
    reset_at(5000, 0x22);
    app_meow_advance(&pet, 5000 + APP_MEOW_HATCH_SEC, 12);
    pet.hunger = 0;
    pet.health = 1;
    pet.hunger_acc = APP_MEOW_HUNGER_EVERY - 1;
    pet.sleeping = 0;
    app_meow_advance(&pet, pet.last_sec + APP_MEOW_SEC_PER_MIN, 12);
    assert(pet.stage == APP_MEOW_DEAD);
    assert(app_meow_act(&pet, APP_MEOW_FEED) == APP_MEOW_GONE);
    assert(app_meow_act(&pet, APP_MEOW_RESET) == APP_MEOW_OK);
    assert(pet.stage == APP_MEOW_EGG);

    /* 再来一颗必须用当前时间,否则会把旧 last_sec 再补 8 小时,新蛋立刻离开。 */
    pet.last_sec = 1000;
    pet.stage = APP_MEOW_DEAD;
    pet.health = 0;
    app_meow_reset(&pet, APP_MEOW_WALL_SEC + 60, (uint8_t)(pet.rng + 1));
    app_meow_advance(&pet, APP_MEOW_WALL_SEC + 72, 16);
    assert(pet.stage == APP_MEOW_EGG);
    assert(pet.hatch_min == 12);

    /* 开机单调秒切到墙上时钟时只对齐,不补时。 */
    reset_at(200, 0x66);
    app_meow_advance(&pet, 200 + 20, 12);
    assert(pet.stage == APP_MEOW_EGG);
    assert(pet.hatch_min == 20);
    app_meow_advance(&pet, APP_MEOW_WALL_SEC + 200, 16);
    assert(pet.stage == APP_MEOW_EGG);
    assert(pet.hatch_min == 20);

    /* Catch-up is capped at 8 hours of real time. */
    reset_at(8000, 0x33);
    app_meow_advance(&pet, 8000 + APP_MEOW_MAX_CATCHUP_SEC + 3600, 12);
    uint32_t advanced = pet.last_sec - 8000;
    assert(advanced <= APP_MEOW_MAX_CATCHUP_SEC);
    assert(advanced + APP_MEOW_SEC_PER_MIN > APP_MEOW_MAX_CATCHUP_SEC);
    assert(app_meow_valid(&pet));

    /* Visit / battle need a living, awake pet. */
    reset_at(9000, 0x44);
    assert(!app_meow_can_link(&pet));
    app_meow_advance(&pet, 9000 + APP_MEOW_HATCH_SEC, 12);
    assert(app_meow_can_link(&pet));
    pet.sleeping = 1;
    assert(!app_meow_can_link(&pet));
    pet.sleeping = 0;
    pet.happy = 20;
    assert(app_meow_visit(&pet) == APP_MEOW_OK);
    assert(pet.happy == 30);
    pet.happy = APP_MEOW_STAT_MAX;
    assert(app_meow_visit(&pet) == APP_MEOW_OK);
    assert(pet.happy == APP_MEOW_STAT_MAX);

    app_meow_t other;
    app_meow_reset(&other, 9000, 0x55);
    app_meow_advance(&other, 9000 + APP_MEOW_HATCH_SEC, 12);
    pet.stage = APP_MEOW_ADULT;
    pet.health = 100;
    pet.happy = 90;
    pet.hunger = 100;
    pet.sick = 0;
    pet.form = 0;
    other.stage = APP_MEOW_BABY;
    other.health = 20;
    other.happy = 20;
    other.hunger = 20;
    other.sick = 1;
    other.form = 1;
    app_meow_snap_t you;
    app_meow_snap(&other, &you);
    uint8_t h = pet.happy;
    assert(app_meow_fight(&pet, &you) == 1);
    assert(pet.happy == 100);
    assert(pet.happy >= h);

    app_meow_snap(&pet, &you);
    other.happy = 20;
    assert(app_meow_fight(&other, &you) == -1);
    assert(other.happy == 10);

    you = (app_meow_snap_t){ 0 };
    pet.happy = 20;
    app_meow_snap(&pet, &you);
    you.rng = pet.rng;
    assert(app_meow_fight(&pet, &you) == 0);
    assert(pet.happy == 20);

    pet.hunger = 20;
    for (int i = 0; i < APP_MEOW_G_N; i++) {
        int use = app_meow_good_use(i);
        if (use == APP_MEOW_USE_MEAL || use == APP_MEOW_USE_DRINK) {
            pet.inv_n[i] = 0;
            pet.inv_d[i] = 0;
        }
    }
    assert(app_meow_act(&pet, APP_MEOW_FEED) == APP_MEOW_EMPTY);
    assert(pet.hunger == 20);
    pet.inv_n[APP_MEOW_G_BURGER] = 1;
    pet.inv_d[APP_MEOW_G_BURGER] = 1;
    assert(app_meow_act(&pet, APP_MEOW_FEED) == APP_MEOW_OK);
    assert(app_meow_inv(&pet, APP_MEOW_G_BURGER) == 0);
    assert(pet.hunger == 52);

    pet.poop = 1;
    pet.inv_n[APP_MEOW_G_TRASH] = 0;
    pet.inv_n[APP_MEOW_G_TISSUE] = 0;
    assert(app_meow_act(&pet, APP_MEOW_CLEAN) == APP_MEOW_EMPTY);
    assert(pet.poop == 1);

    pet.inv_n[APP_MEOW_G_WATER] = 8;
    pet.inv_d[APP_MEOW_G_WATER] = 3;
    assert(app_meow_give(&pet, APP_MEOW_G_WATER, 5) == 5);
    assert(app_meow_inv(&pet, APP_MEOW_G_WATER) == 13);
    pet.inv_n[APP_MEOW_G_WATER] = APP_MEOW_INV_MAX - 1;
    assert(app_meow_give(&pet, APP_MEOW_G_WATER, 5) == 1);
    assert(app_meow_inv(&pet, APP_MEOW_G_WATER) == APP_MEOW_INV_MAX);
    assert(app_meow_give(&pet, APP_MEOW_G_WATER, 1) == 0);

    int sum0 = 0;
    for (int i = 0; i < APP_MEOW_G_N; i++) sum0 += app_meow_inv(&pet, i);
    int found = app_meow_loot(&pet, APP_MEOW_LOOT_WALK);
    assert(found >= 0 && found < APP_MEOW_G_N);
    int sum1 = 0;
    for (int i = 0; i < APP_MEOW_G_N; i++) sum1 += app_meow_inv(&pet, i);
    assert(sum1 == sum0 + 1);

    pet.inv_n[APP_MEOW_G_SOAP] = 1;
    pet.inv_d[APP_MEOW_G_SOAP] = 4;
    pet.poop = 1;
    pet.happy = 20;
    pet.dirt = 40;
    assert(app_meow_act(&pet, APP_MEOW_BATH) == APP_MEOW_OK);
    assert(app_meow_inv(&pet, APP_MEOW_G_SOAP) == 1);
    assert(app_meow_dur(&pet, APP_MEOW_G_SOAP) == 3);
    assert(pet.dirt == 0);

    pet.sick = 1;
    pet.ailment = APP_MEOW_AI_WORM;
    pet.health = 20;
    pet.inv_n[APP_MEOW_G_WORM] = 1;
    pet.inv_d[APP_MEOW_G_WORM] = 2;
    pet.inv_n[APP_MEOW_G_STOMACH] = 0;
    pet.inv_n[APP_MEOW_G_COLD] = 0;
    assert(app_meow_pick(&pet, APP_MEOW_HEAL) == APP_MEOW_G_WORM);
    assert(app_meow_act(&pet, APP_MEOW_HEAL) == APP_MEOW_OK);
    assert(pet.sick == 0);
    assert(pet.ailment == 0);
    assert(pet.health >= 38);

    pet.inv_n[APP_MEOW_G_SHOVEL] = 1;
    pet.inv_d[APP_MEOW_G_SHOVEL] = 8;
    sum0 = 0;
    for (int i = 0; i < APP_MEOW_G_N; i++) sum0 += app_meow_inv(&pet, i);
    found = app_meow_loot(&pet, APP_MEOW_LOOT_WALK);
    assert(found >= 0);
    sum1 = 0;
    for (int i = 0; i < APP_MEOW_G_N; i++) sum1 += app_meow_inv(&pet, i);
    assert(sum1 >= sum0 + 1);
    assert(app_meow_dur(&pet, APP_MEOW_G_SHOVEL) == 7 ||
           app_meow_inv(&pet, APP_MEOW_G_SHOVEL) == 0);

    reset_at(11000, 0x66);
    app_meow_advance(&pet, 11000 + APP_MEOW_HATCH_SEC, 12);
    pet.ver = 2;
    uint8_t raw[37];
    memcpy(raw, &pet, 32);
    app_meow_t q;
    assert(app_meow_import(&q, raw, 32));
    assert(q.ver == APP_MEOW_VER);
    assert(app_meow_inv(&q, APP_MEOW_G_ONIGIRI) == 2);

    reset_at(12000, 0x77);
    pet.ver = 2;
    memcpy(raw, &pet, 32);
    assert(app_meow_import(&q, raw, 32));
    assert(q.stage == APP_MEOW_EGG);
    assert(app_meow_inv(&q, APP_MEOW_G_ONIGIRI) == 0);

    reset_at(13000, 0x88);
    app_meow_advance(&pet, 13000 + APP_MEOW_HATCH_SEC, 12);
    memcpy(raw, &pet, 32);
    raw[12] = 3;
    raw[32] = 4;
    raw[33] = 4;
    raw[34] = 2;
    raw[35] = 2;
    raw[36] = 1;
    assert(app_meow_import(&q, raw, 37));
    assert(q.ver == APP_MEOW_VER);
    assert(app_meow_inv(&q, APP_MEOW_G_BURGER) == 4);
    assert(app_meow_inv(&q, APP_MEOW_G_WATER) == 4);
    assert(app_meow_inv(&q, APP_MEOW_G_SOAP) == 2);
    assert(app_meow_inv(&q, APP_MEOW_G_TRASH) == 2);
    assert(app_meow_inv(&q, APP_MEOW_G_COLD) == 1);

    uint8_t owned[APP_MEOW_G_N];
    assert(app_meow_owned_n(&q, APP_MEOW_CAT_FOOD) >= 2);
    assert(app_meow_owned_list(&q, APP_MEOW_CAT_FOOD, owned, APP_MEOW_G_N) >= 2);

    /* v4 的 44 种按同类并进 16 格,上限 999。 */
    reset_at(14000, 0x99);
    app_meow_advance(&pet, 14000 + APP_MEOW_HATCH_SEC, 12);
    {
        size_t inv_off = APP_MEOW_VER3_SIZE;
        uint8_t v4[256];
        size_t n = inv_off + (size_t)APP_MEOW_VER4_N * 2;
        assert(n <= sizeof(v4));
        memset(v4, 0, sizeof(v4));
        memcpy(v4, &pet, inv_off);
        v4[12] = 4;
        v4[inv_off + 0] = 3;   /* burger */
        v4[inv_off + 1] = 2;   /* trotter -> burger */
        v4[inv_off + 12] = 2;  /* onigiri */
        v4[inv_off + 16] = 2;  /* water */
        v4[inv_off + 40] = 1;  /* shovel */
        v4[inv_off + APP_MEOW_VER4_N + 40] = 5;
        assert(app_meow_import(&q, v4, n));
        assert(q.ver == APP_MEOW_VER);
        assert(app_meow_inv(&q, APP_MEOW_G_BURGER) == 5);
        assert(app_meow_inv(&q, APP_MEOW_G_ONIGIRI) == 2);
        assert(app_meow_inv(&q, APP_MEOW_G_WATER) == 2);
        assert(app_meow_inv(&q, APP_MEOW_G_SHOVEL) == 1);
        assert(app_meow_dur(&q, APP_MEOW_G_SHOVEL) == 5);
    }

    /* v5: 8 位数量升到 16 位,上限 999。 */
    reset_at(14500, 0xab);
    app_meow_advance(&pet, 14500 + APP_MEOW_HATCH_SEC, 12);
    {
        uint8_t v5[APP_MEOW_VER3_SIZE + APP_MEOW_G_N * 2];
        memset(v5, 0, sizeof(v5));
        memcpy(v5, &pet, APP_MEOW_VER3_SIZE);
        v5[12] = 5;
        v5[APP_MEOW_VER3_SIZE + APP_MEOW_G_WATER] = 200;
        v5[APP_MEOW_VER3_SIZE + APP_MEOW_G_N + APP_MEOW_G_WATER] = 3;
        assert(app_meow_import(&q, v5, sizeof(v5)));
        assert(q.ver == APP_MEOW_VER);
        assert(app_meow_inv(&q, APP_MEOW_G_WATER) == 200);
        assert(app_meow_dur(&q, APP_MEOW_G_WATER) == 3);
    }

    /* 主页快捷按目录轮流,喂食含饮料、洗澡含清理,并跳过当前用不成的。 */
    reset_at(15000, 0xaa);
    app_meow_advance(&pet, 15000 + APP_MEOW_HATCH_SEC, 12);
    for (int i = 0; i < APP_MEOW_G_N; i++) {
        pet.inv_n[i] = 0;
        pet.inv_d[i] = 0;
    }
    pet.inv_n[APP_MEOW_G_BURGER] = 1;
    pet.inv_d[APP_MEOW_G_BURGER] = 1;
    pet.inv_n[APP_MEOW_G_ONIGIRI] = 1;
    pet.inv_d[APP_MEOW_G_ONIGIRI] = 1;
    pet.inv_n[APP_MEOW_G_SALAD] = 1;
    pet.inv_d[APP_MEOW_G_SALAD] = 1;
    pet.inv_n[APP_MEOW_G_WATER] = 1;
    pet.inv_d[APP_MEOW_G_WATER] = 1;
    pet.hunger = 0;
    assert(app_meow_pick(&pet, APP_MEOW_FEED) == APP_MEOW_G_BURGER);
    assert(app_meow_act(&pet, APP_MEOW_FEED) == APP_MEOW_OK);
    assert(pet.hunger == 32);
    assert(app_meow_pick(&pet, APP_MEOW_FEED) == APP_MEOW_G_ONIGIRI);
    assert(app_meow_act(&pet, APP_MEOW_FEED) == APP_MEOW_OK);
    assert(pet.hunger == 54);
    assert(app_meow_pick(&pet, APP_MEOW_FEED) == APP_MEOW_G_SALAD);
    assert(app_meow_act(&pet, APP_MEOW_FEED) == APP_MEOW_OK);
    assert(pet.hunger == 68);
    /* 还没饱,矿泉水接着用。 */
    assert(app_meow_pick(&pet, APP_MEOW_FEED) == APP_MEOW_G_WATER);
    assert(app_meow_act(&pet, APP_MEOW_FEED) == APP_MEOW_OK);
    assert(pet.hunger == 76);
    pet.hunger = APP_MEOW_STAT_MAX;
    assert(app_meow_pick(&pet, APP_MEOW_FEED) < 0 ||
           app_meow_act(&pet, APP_MEOW_FEED) == APP_MEOW_FULL);
    pet.hunger = 20;
    pet.inv_n[APP_MEOW_G_WATER] = 1;
    pet.inv_d[APP_MEOW_G_WATER] = 3;
    assert(app_meow_pick(&pet, APP_MEOW_FEED) == APP_MEOW_G_WATER);
    assert(app_meow_act(&pet, APP_MEOW_FEED) == APP_MEOW_OK);
    assert(pet.hunger == 28);

    pet.inv_n[APP_MEOW_G_SOAP] = 1;
    pet.inv_d[APP_MEOW_G_SOAP] = 4;
    pet.inv_n[APP_MEOW_G_TRASH] = 1;
    pet.inv_d[APP_MEOW_G_TRASH] = 3;
    pet.inv_n[APP_MEOW_G_TOWEL] = 1;
    pet.inv_d[APP_MEOW_G_TOWEL] = 4;
    pet.poop = 1;
    pet.dirt = 40;
    pet.happy = 20;
    assert(app_meow_pick(&pet, APP_MEOW_BATH) == APP_MEOW_G_SOAP);
    assert(app_meow_act(&pet, APP_MEOW_BATH) == APP_MEOW_OK);
    /* 便便已清,垃圾袋跳过,轮到毛巾。 */
    assert(pet.poop == 0);
    assert(app_meow_pick(&pet, APP_MEOW_BATH) == APP_MEOW_G_TOWEL);
    pet.happy = APP_MEOW_STAT_MAX;
    pet.dirt = 0;
    pet.poop = 1;
    assert(app_meow_pick(&pet, APP_MEOW_BATH) == APP_MEOW_G_TRASH);

    pet.sick = 1;
    pet.health = 20;
    pet.inv_n[APP_MEOW_G_WORM] = 1;
    pet.inv_d[APP_MEOW_G_WORM] = 2;
    pet.inv_n[APP_MEOW_G_VITAMIN] = 1;
    pet.inv_d[APP_MEOW_G_VITAMIN] = 4;
    assert(app_meow_pick(&pet, APP_MEOW_HEAL) == APP_MEOW_G_WORM);
    assert(app_meow_act(&pet, APP_MEOW_HEAL) == APP_MEOW_OK);
    assert(app_meow_pick(&pet, APP_MEOW_HEAL) == APP_MEOW_G_VITAMIN);

    /* 背包旅行:时长跟单次耐久收益走,饭团对齐旧的 3 游戏分钟。 */
    {
        uint8_t take[APP_MEOW_G_N] = { 0 };
        int g1;
        int g4;

        assert(app_meow_trip_mins(0) == 0);
        take[APP_MEOW_G_ONIGIRI] = 1;
        g1 = app_meow_trip_take_gain(take);
        assert(g1 == APP_MEOW_TRIP_GAIN_REF);
        assert(app_meow_trip_mins(g1) == (int)APP_MEOW_TRIP_MIN_PER);
        take[APP_MEOW_G_ONIGIRI] = 4;
        g4 = app_meow_trip_take_gain(take);
        assert(g4 == 4 * APP_MEOW_TRIP_GAIN_REF);
        assert(app_meow_trip_mins(g4) == 4 * (int)APP_MEOW_TRIP_MIN_PER);
        assert(app_meow_trip_sec(0) == 0);
        assert(app_meow_trip_sec(g1) ==
               (int)APP_MEOW_TRIP_MIN_PER * (int)APP_MEOW_SEC_PER_MIN);
        assert(app_meow_trip_sec(g4) ==
               4 * (int)APP_MEOW_TRIP_MIN_PER * (int)APP_MEOW_SEC_PER_MIN);
        take[APP_MEOW_G_ONIGIRI] = 0;
        take[APP_MEOW_G_BURGER] = 1;
        assert(app_meow_trip_mins(app_meow_trip_take_gain(take)) >
               (int)APP_MEOW_TRIP_MIN_PER);
    }
    assert(app_meow_trip_rewards(0) == 0);
    assert(app_meow_trip_rewards(APP_MEOW_TRIP_GAIN_REF) == 2);
    assert(app_meow_trip_rewards(2 * APP_MEOW_TRIP_GAIN_REF) == 3);
    assert(app_meow_trip_rewards(4 * APP_MEOW_TRIP_GAIN_REF) == 6);
    assert(app_meow_trip_rewards(8 * APP_MEOW_TRIP_GAIN_REF) == 11);
    assert(app_meow_trip_rewards(20 * APP_MEOW_TRIP_GAIN_REF) == 26);
    assert(app_meow_trip_rewards(app_meow_good_gain(APP_MEOW_G_WATER) *
                                app_meow_good_dur_max(APP_MEOW_G_WATER)) >
           app_meow_trip_rewards(APP_MEOW_TRIP_GAIN_REF));
    assert(app_meow_trip_rewards(4 * app_meow_good_gain(APP_MEOW_G_BURGER)) >
           app_meow_trip_rewards(4 * APP_MEOW_TRIP_GAIN_REF));
    assert(app_meow_trip_rewards(20 * APP_MEOW_TRIP_GAIN_REF) * 4 <=
           app_meow_trip_rewards(4 * APP_MEOW_TRIP_GAIN_REF) * 20);
    assert(app_meow_trip_souv_pct(5) == 0);
    assert(app_meow_trip_souv_pct(6) > 0);
    assert(app_meow_trip_souv_pct(20) == 40);

    reset_at(16000, 0xcc);
    {
        uint8_t take[APP_MEOW_G_N] = { 0 };
        take[APP_MEOW_G_ONIGIRI] = 1;
        assert(app_meow_trip_start(&pet, take) == APP_MEOW_EGG_WAIT);
    }
    app_meow_advance(&pet, 16000 + APP_MEOW_HATCH_SEC, 12);
    assert(pet.stage == APP_MEOW_BABY);
    {
        uint8_t take[APP_MEOW_G_N] = { 0 };
        uint16_t food = app_meow_inv(&pet, APP_MEOW_G_ONIGIRI);
        uint16_t water = app_meow_inv(&pet, APP_MEOW_G_WATER);
        take[APP_MEOW_G_ONIGIRI] = 1;
        take[APP_MEOW_G_WATER] = 1;
        assert(app_meow_trip_can(&pet));
        assert(app_meow_trip_start(&pet, take) == APP_MEOW_OK);
        assert(pet.trip_st == APP_MEOW_TRIP_AWAY);
        assert(pet.trip_pack == 2);
        assert(pet.trip_gain == (uint16_t)app_meow_trip_take_gain(take));
        assert(pet.trip_left ==
               app_meow_trip_mins(app_meow_trip_take_gain(take)));
        assert(app_meow_inv(&pet, APP_MEOW_G_ONIGIRI) == food - 1);
        assert(app_meow_inv(&pet, APP_MEOW_G_WATER) == water - 1);
        assert(!app_meow_can(&pet, APP_MEOW_FEED));
        assert(app_meow_act(&pet, APP_MEOW_FEED) == APP_MEOW_NONE);
        assert(app_meow_use(&pet, APP_MEOW_G_FRUIT) == APP_MEOW_NONE);
        assert(app_meow_can(&pet, APP_MEOW_PLAY));
        assert(app_meow_play_apply(&pet, 1) == 1);
        assert(!app_meow_trip_can(&pet));
        assert(app_meow_trip_claim(&pet) == 0);
        app_meow_advance(&pet,
                         pet.last_sec + (uint32_t)pet.trip_left *
                         APP_MEOW_SEC_PER_MIN, 12);
        assert(pet.trip_st == APP_MEOW_TRIP_BACK);
        {
            int got = app_meow_trip_claim(&pet);
            uint8_t prize[APP_MEOW_G_N];
            int sum = 0;
            app_meow_last_prizes(prize);
            for (int i = 0; i < APP_MEOW_G_N; i++) sum += prize[i];
            assert(pet.trip_st == APP_MEOW_TRIP_IDLE);
            assert(got == app_meow_trip_rewards(app_meow_trip_take_gain(take)));
            assert(sum == got);
        }
        assert(app_meow_can(&pet, APP_MEOW_FEED));
    }

    pet.found = 0x5;
    app_meow_reset(&pet, pet.last_sec, (uint8_t)(pet.rng + 1));
    assert(pet.found == 0x5);
    assert(app_meow_souv_on(&pet, 0));
    assert(!app_meow_souv_on(&pet, 1));
    assert(app_meow_souv_on(&pet, 2));

    /* 经验升级,阶段内封顶;饱食按幼体间隔掉。 */
    reset_at(18000, 0xee);
    app_meow_advance(&pet, 18000 + APP_MEOW_HATCH_SEC, 12);
    assert(app_meow_level(&pet) == 1);
    pet.xp = (uint16_t)(app_meow_xp_need(1) - 1);
    pet.hunger = 40;
    pet.inv_n[APP_MEOW_G_ONIGIRI] = 1;
    pet.inv_d[APP_MEOW_G_ONIGIRI] = 1;
    assert(app_meow_act(&pet, APP_MEOW_FEED) == APP_MEOW_OK);
    assert(app_meow_level(&pet) == 2);
    assert(pet.xp == 3);
    pet.level = 5;
    pet.xp = 0;
    pet.hunger = 40;
    pet.inv_n[APP_MEOW_G_ONIGIRI] = 1;
    pet.inv_d[APP_MEOW_G_ONIGIRI] = 1;
    assert(app_meow_act(&pet, APP_MEOW_FEED) == APP_MEOW_OK);
    assert(app_meow_level(&pet) == 5);
    assert(pet.xp == 0);
    pet.level = 1;
    pet.hunger = 50;
    pet.hunger_acc = (uint8_t)(app_meow_hunger_every(&pet) - 1);
    pet.sleeping = 0;
    app_meow_advance(&pet, pet.last_sec + APP_MEOW_SEC_PER_MIN, 12);
    assert(pet.hunger < 50);

    {
        app_meow_t old;
        uint8_t raw[sizeof(app_meow_t)];
        reset_at(17000, 0xdd);
        pet.found = 0;
        memcpy(&old, &pet, sizeof(old));
        old.ver = APP_MEOW_VER6;
        memcpy(raw, &old, sizeof(raw));
        assert(app_meow_import(&q, raw, sizeof(raw) - 8));
        assert(q.ver == APP_MEOW_VER);
        assert(q.trip_st == APP_MEOW_TRIP_IDLE);
        assert(q.found == 0);
        assert(q.named == 0);
        assert(q.name[0] == 0);
    }

    app_meow_set_name(&pet, "Pip");
    pet.named = 1;
    {
        app_meow_t q2;
        assert(app_meow_import(&q2, &pet, sizeof(pet)));
        assert(strcmp(app_meow_name(&q2), "Pip") == 0);
        assert(q2.named == 1);
    }

    {
        app_meow_t src, q9;
        uint8_t raw[128];
        size_t name_off = offsetof(app_meow_t, name);
        size_t old_gain_off = name_off + APP_MEOW_NAME_MAX_V9 + 1;
        size_t old_n = old_gain_off + 2;

        memcpy(&src, &pet, sizeof(src));
        src.ver = APP_MEOW_VER9;
        src.named = 1;
        memset(src.name, 0, sizeof(src.name));
        memcpy(src.name, "团团", 7);
        src.trip_gain = 48;
        memset(raw, 0, sizeof(raw));
        memcpy(raw, &src, name_off);
        memcpy(raw + name_off, src.name, APP_MEOW_NAME_MAX_V9 + 1);
        memcpy(raw + old_gain_off, &src.trip_gain, 2);
        assert(app_meow_import(&q9, raw, old_n));
        assert(strcmp(app_meow_name(&q9), "团团") == 0);
        assert(q9.named == 1);
        assert(q9.trip_gain == 48);
    }

    pet.found = 0x7;
    pet.inv_n[APP_MEOW_G_ONIGIRI] = 3;
    pet.named = 1;
    memcpy(pet.name, "Pip", 4);
    pet.stage = APP_MEOW_ADULT;
    pet.species = 5;
    app_meow_wipe(&pet, pet.last_sec, (uint8_t)(pet.rng + 1));
    assert(app_meow_valid(&pet));
    assert(pet.stage == APP_MEOW_DEAD);
    assert(pet.found == 0);
    assert(app_meow_inv(&pet, APP_MEOW_G_ONIGIRI) == 0);
    assert(pet.named == 0);
    assert(pet.name[0] == 0);
    assert(pet.species == 0);
    assert(pet.trip_st == APP_MEOW_TRIP_IDLE);

    puts("ok");
    return 0;
}
