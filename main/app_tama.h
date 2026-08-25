#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* 1 游戏分钟 = 60 秒真实时间。蛋孵化按真实秒,不跟游戏分钟走。 */
#define APP_TAMA_SEC_PER_MIN     60u
#define APP_TAMA_HATCH_SEC       60u
#define APP_TAMA_BABY_MIN        40u
#define APP_TAMA_CHILD_MIN       120u
#define APP_TAMA_TEEN_MIN        240u
#define APP_TAMA_HUNGER_EVERY    18u /* 幼体基准;实际随阶段/等级变 */
#define APP_TAMA_HAPPY_EVERY     30u
#define APP_TAMA_DIRT_EVERY      22u /* 幼体清洁基准 */
#define APP_TAMA_POOP_DELAY      8u
#define APP_TAMA_STAT_MAX        100u
#define APP_TAMA_POOP_MAX        2u
#define APP_TAMA_DIRT_MAX        100u
#define APP_TAMA_LV_MAX          99u
#define APP_TAMA_BABY_LV         4u
#define APP_TAMA_CHILD_LV        8u
#define APP_TAMA_TEEN_LV         14u
#define APP_TAMA_MAX_CATCHUP_SEC (8u * 3600u)
/* 与 now_sec() 一致:大于此值视为墙上时钟,否则是开机单调秒。 */
#define APP_TAMA_WALL_SEC        1700000000u
#define APP_TAMA_MAGIC           0x31414D54u /* TMA1 */
#define APP_TAMA_VER             9u
#define APP_TAMA_VER8            8u
#define APP_TAMA_VER7            7u
#define APP_TAMA_VER6            6u
#define APP_TAMA_NAME_MAX        12
#define APP_TAMA_VER2_SIZE       32u
#define APP_TAMA_VER3_SIZE       37u
#define APP_TAMA_VER4_N          44
#define APP_TAMA_INV_MAX         999
#define APP_TAMA_SOUV_N          8
#define APP_TAMA_TRIP_IDLE       0
#define APP_TAMA_TRIP_AWAY       1
#define APP_TAMA_TRIP_BACK       2
#define APP_TAMA_TRIP_MIN_PER    3u
#define APP_TAMA_TRIP_GAIN_REF   24 /* 饭团单次耐久收益,时长锚点 */
#define APP_TAMA_TRIP_PACK_MAX   20
#define APP_TAMA_TRIP_SOUV_MIN   6

/* 背包分类:食物(含饮料)、药品、道具。 */
typedef enum {
    APP_TAMA_CAT_FOOD = 0,
    APP_TAMA_CAT_MED,
    APP_TAMA_CAT_GEAR,
    APP_TAMA_CAT_N
} app_tama_cat_t;

typedef enum {
    APP_TAMA_USE_MEAL = 0,
    APP_TAMA_USE_DRINK,
    APP_TAMA_USE_MED,
    APP_TAMA_USE_WASH,
    APP_TAMA_USE_SWEEP,
    APP_TAMA_USE_GROOM,
    APP_TAMA_USE_DIG
} app_tama_use_t;

#define APP_TAMA_AI_NONE  0
#define APP_TAMA_AI_WORM  1
#define APP_TAMA_AI_TUMMY 2
#define APP_TAMA_AI_COLD  3
#define APP_TAMA_AI_COUGH 4

#define APP_TAMA_GF_JUNK    1u
#define APP_TAMA_GF_HEALTHY 2u
#define APP_TAMA_GF_SHOVEL  4u

/* 只留能对应护理动作的商品。 */
typedef enum {
    APP_TAMA_G_BURGER = 0, /* 喂食,油腻 */
    APP_TAMA_G_ONIGIRI,    /* 喂食 */
    APP_TAMA_G_SALAD,      /* 喂食,清淡 */
    APP_TAMA_G_FRUIT,      /* 喂食,清淡 */
    APP_TAMA_G_WATER,      /* 喝水,清淡 */
    APP_TAMA_G_COLA,       /* 喝水,油腻 */
    APP_TAMA_G_WORM,       /* 治疗寄生虫 */
    APP_TAMA_G_STOMACH,    /* 治疗肠胃 */
    APP_TAMA_G_COLD,       /* 治疗感冒 */
    APP_TAMA_G_COUGH,      /* 治疗咳嗽 */
    APP_TAMA_G_VITAMIN,    /* 通用补体力 */
    APP_TAMA_G_SOAP,       /* 洗澡 */
    APP_TAMA_G_TRASH,      /* 清理便便 */
    APP_TAMA_G_TISSUE,     /* 清理 */
    APP_TAMA_G_SHOVEL,     /* 挖掘 / 遛弯多捡 */
    APP_TAMA_G_TOWEL,      /* 洗澡 */
    APP_TAMA_G_N
} app_tama_good_t;

typedef enum {
    APP_TAMA_LOOT_WALK = 0,
    APP_TAMA_LOOT_VISIT,
    APP_TAMA_LOOT_WIN,
    APP_TAMA_LOOT_DRAW,
    APP_TAMA_LOOT_PLAY,
    APP_TAMA_LOOT_TRIP
} app_tama_loot_t;

typedef enum {
    APP_TAMA_EGG = 0,
    APP_TAMA_BABY,
    APP_TAMA_CHILD,
    APP_TAMA_TEEN,
    APP_TAMA_ADULT,
    APP_TAMA_DEAD
} app_tama_stage_t;

typedef enum {
    APP_TAMA_FEED = 0,
    APP_TAMA_PLAY,
    APP_TAMA_CLEAN,
    APP_TAMA_HEAL,
    APP_TAMA_LIGHT,
    APP_TAMA_RESET,
    APP_TAMA_DRINK,
    APP_TAMA_BATH,
    APP_TAMA_WALK,
    APP_TAMA_PET,
    APP_TAMA_BED
} app_tama_act_t;

typedef enum {
    APP_TAMA_OK = 0,
    APP_TAMA_SLEEP,
    APP_TAMA_FULL,
    APP_TAMA_NONE,
    APP_TAMA_EGG_WAIT,
    APP_TAMA_GONE,
    APP_TAMA_EMPTY
} app_tama_res_t;

/* 物种:蛋=0,幼=1,童=2,少=3/4,成=5..10。护理好偏向低编号,仍有随机。 */
#define APP_TAMA_SP_NONE    0
#define APP_TAMA_SP_BABY    1
#define APP_TAMA_SP_CHILD   2
#define APP_TAMA_SP_TEEN_A  3
#define APP_TAMA_SP_TEEN_B  4
#define APP_TAMA_SP_ADULT_0 5
#define APP_TAMA_SP_MAX     10

#define APP_TAMA_ALERT_OK   0
#define APP_TAMA_ALERT_WARN 1  /* ≤50% */
#define APP_TAMA_ALERT_HIT  2  /* ≤30% */
#define APP_TAMA_ALERT_CRIT 3  /* ≤10% */
#define APP_TAMA_ALERT_PCT_WARN 50
#define APP_TAMA_ALERT_PCT_HIT  30
#define APP_TAMA_ALERT_PCT_CRIT 10

#define APP_TAMA_D_HUNGER 0
#define APP_TAMA_D_HAPPY  1
#define APP_TAMA_D_HEALTH 2
#define APP_TAMA_D_SICK   3
#define APP_TAMA_D_POOP   4
#define APP_TAMA_D_LIGHT  5
#define APP_TAMA_D_N      6

typedef struct {
    uint32_t magic;
    uint32_t last_sec;
    uint16_t age_min;
    uint16_t alert_ack; /* 每个危险 2 bit,已播报的最高档 */
    uint8_t ver;
    uint8_t stage;
    uint8_t hunger;
    uint8_t happy;
    uint8_t health;
    uint8_t poop;
    uint8_t sick;
    uint8_t sleeping;
    uint8_t lights_off;
    uint8_t form;       /* 成体: 0 乖巧, 1 被冷落 */
    uint8_t species;
    uint8_t care_good;
    uint8_t care_miss;
    uint8_t weight;
    uint8_t hatch_min;  /* 蛋:已过真实秒,满 APP_TAMA_HATCH_SEC 孵化 */
    uint8_t poop_in;
    uint8_t hunger_acc;
    uint8_t happy_acc;
    uint8_t rng;
    uint8_t miss_light;
    uint8_t ailment;    /* 0 无,1 虫,2 胃,3 感冒,4 咳嗽 */
    uint8_t dirt;       /* 0 干净 .. 100 脏;界面清洁度 = 100-dirt */
    uint8_t diet_good;
    uint8_t diet_junk;
    uint8_t dirt_acc;
    uint16_t inv_n[APP_TAMA_G_N]; /* 每种数量,最多 999 */
    uint8_t inv_d[APP_TAMA_G_N];  /* 当前打开那份的剩余耐久 */
    uint8_t trip_st;              /* 0 在家,1 旅行中,2 已回来待领 */
    uint8_t trip_pack;            /* 带上的食物份数 */
    uint16_t trip_left;           /* 剩余游戏分钟 */
    uint16_t found;               /* 收藏纪念品 bitmask */
    uint8_t level;                /* 1..99,蛋为 0 */
    uint16_t xp;                  /* 当前级经验 */
    uint8_t named;                /* 1=已确认过名字,空名字表示用默认 */
    char name[APP_TAMA_NAME_MAX + 1];
    uint16_t trip_gain;           /* 出发时的食物收益,回来发奖用 */
} app_tama_t;

typedef struct {
    uint8_t stage;
    uint8_t hunger;
    uint8_t happy;
    uint8_t health;
    uint8_t sick;
    uint8_t form;
    uint8_t species;
    uint8_t weight;
    uint8_t rng;
    uint16_t age_min;
} app_tama_snap_t;

#define APP_TAMA_KIND_VISIT 1
#define APP_TAMA_KIND_FIGHT 2

void app_tama_reset(app_tama_t *p, uint32_t now_sec, uint8_t rng);
/* 清空背包、收藏、名字；宠物直接进入离开。 */
void app_tama_wipe(app_tama_t *p, uint32_t now_sec, uint8_t rng);
bool app_tama_valid(const app_tama_t *p);
/* 读 NVS blob。支持 v2 的 32 字节、v3 的 37 字节、v4 的 44 种背包、v5 的 8 位数量。 */
bool app_tama_import(app_tama_t *p, const void *raw, size_t n);
uint16_t app_tama_inv(const app_tama_t *p, int good);
uint8_t app_tama_dur(const app_tama_t *p, int good);
int app_tama_give(app_tama_t *p, int good, int n);
/* 按来源抽材料。返回商品 id,没有则 -1。遛弯带金铲铲会多捡一份。 */
int app_tama_loot(app_tama_t *p, app_tama_loot_t src);
/* 喂食含饮料、洗澡含清理、治疗含药品;按目录轮流,并跳过当前用不成的。 */
int app_tama_pick(const app_tama_t *p, app_tama_act_t act);
int app_tama_good_cat(int good);
int app_tama_good_use(int good);
int app_tama_good_dur_max(int good);
/* 单次耐久收益: 饱食+心情+健康-脏度,低于 0 记 0。 */
int app_tama_good_gain(int good);
const char *app_tama_name(const app_tama_t *p);
void app_tama_set_name(app_tama_t *p, const char *name);
int app_tama_owned_n(const app_tama_t *p, int cat);
int app_tama_owned_list(const app_tama_t *p, int cat, uint8_t *out, int max);
app_tama_res_t app_tama_use(app_tama_t *p, int good);
int app_tama_last_got(void);
/* hour 为 0..23; -1 表示时钟未知,不改睡觉状态。 */
void app_tama_advance(app_tama_t *p, uint32_t now_sec, int hour);
app_tama_res_t app_tama_act(app_tama_t *p, app_tama_act_t act);
/* 1 猜中, 0 猜错, -1 当前不能玩。guess 0=左 1=右。 */
int app_tama_play(app_tama_t *p, int guess);
/* 玩耍结算: win 非 0 心情升, 0 心情降。不能玩返回 -1。 */
int app_tama_play_apply(app_tama_t *p, int win);
/* 按积分发奖: 每 100 分随机 1 份。返回发出数量。 */
int app_tama_play_prize(app_tama_t *p, int score);
/* 小猫快跑结算: 本局碰到的物品入包。返回实际发出数量。 */
int app_tama_run_prize(app_tama_t *p, const uint8_t got[APP_TAMA_G_N]);
/* 只抽不发。满员或无效返回 -1。 */
int app_tama_roll(app_tama_t *p, app_tama_loot_t src);
void app_tama_last_prizes(uint8_t out[APP_TAMA_G_N]);
/* 难度档: 每 100 分 +1, 500 分封顶。游动 / 锦鲤概率 300 起 30%, 之后每档 +30%。 */
int app_tama_play_tier(int score);
int app_tama_play_swim_pct(int score);
int app_tama_play_koi_pct(int score);
bool app_tama_can(const app_tama_t *p, app_tama_act_t act);
bool app_tama_can_link(const app_tama_t *p);
void app_tama_snap(const app_tama_t *p, app_tama_snap_t *out);
int app_tama_power(const app_tama_snap_t *s);
app_tama_res_t app_tama_visit(app_tama_t *p);
/* 1 赢, 0 平, -1 输, -2 不能打。 */
int app_tama_fight(app_tama_t *me, const app_tama_snap_t *you);

int app_tama_care_score(const app_tama_t *p);
uint8_t app_tama_danger_lv(const app_tama_t *p, int danger);
uint8_t app_tama_alert_peak(const app_tama_t *p);
/* 若有新的一档提醒要播,返回 1 并写出危险种类和档位。 */
int app_tama_alert_poll(app_tama_t *p, int *danger, int *lv);
bool app_tama_trip_can(const app_tama_t *p);
int app_tama_trip_take_gain(const uint8_t take[APP_TAMA_G_N]);
int app_tama_trip_mins(int gain);
int app_tama_trip_sec(int gain);
int app_tama_trip_rewards(int gain);
int app_tama_trip_souv_pct(int pack);
int app_tama_trip_sec_left(const app_tama_t *p, uint32_t now_sec);
app_tama_res_t app_tama_trip_start(app_tama_t *p, const uint8_t take[APP_TAMA_G_N]);
int app_tama_trip_claim(app_tama_t *p);
int app_tama_last_souv(void);
bool app_tama_souv_on(const app_tama_t *p, int id);

int app_tama_level(const app_tama_t *p);
int app_tama_xp(const app_tama_t *p);
int app_tama_xp_need(int level);
int app_tama_xp_pct(const app_tama_t *p);
int app_tama_clean(const app_tama_t *p);
int app_tama_hunger_every(const app_tama_t *p);
int app_tama_dirt_every(const app_tama_t *p);
int app_tama_good_tier(int good);
