#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* 1 游戏分钟 = 60 秒真实时间。蛋孵化按真实秒,不跟游戏分钟走。 */
#define APP_MEOW_SEC_PER_MIN     60u
#define APP_MEOW_HATCH_SEC       60u
#define APP_MEOW_BABY_MIN        40u
#define APP_MEOW_CHILD_MIN       120u
#define APP_MEOW_TEEN_MIN        240u
#define APP_MEOW_HUNGER_EVERY    18u /* 幼体基准;实际随阶段/等级变 */
#define APP_MEOW_HAPPY_EVERY     30u
#define APP_MEOW_DIRT_EVERY      22u /* 幼体清洁基准 */
#define APP_MEOW_POOP_DELAY      8u
#define APP_MEOW_STAT_MAX        100u
#define APP_MEOW_POOP_MAX        2u
#define APP_MEOW_DIRT_MAX        100u
#define APP_MEOW_LV_MAX          99u
#define APP_MEOW_BABY_LV         4u
#define APP_MEOW_CHILD_LV        8u
#define APP_MEOW_TEEN_LV         14u
#define APP_MEOW_MAX_CATCHUP_SEC (8u * 3600u)
/* 与 now_sec() 一致:大于此值视为墙上时钟,否则是开机单调秒。 */
#define APP_MEOW_WALL_SEC        1700000000u
#define APP_MEOW_BED_HOUR        21
#define APP_MEOW_WAKE_HOUR       8
#define APP_MEOW_MAGIC           0x314F454Du /* MEO1 */
#define APP_MEOW_VER             10u
#define APP_MEOW_VER9            9u
#define APP_MEOW_VER8            8u
#define APP_MEOW_VER7            7u
#define APP_MEOW_VER6            6u
#define APP_MEOW_NAME_MAX        15 /* 5 个中文 */
#define APP_MEOW_NAME_CHARS      10 /* 或 10 个字母 */
#define APP_MEOW_NAME_MAX_V9     12
#define APP_MEOW_VER2_SIZE       32u
#define APP_MEOW_VER3_SIZE       37u
#define APP_MEOW_VER4_N          44
#define APP_MEOW_INV_MAX         999
#define APP_MEOW_SOUV_N          8
#define APP_MEOW_TRIP_IDLE       0
#define APP_MEOW_TRIP_AWAY       1
#define APP_MEOW_TRIP_BACK       2
#define APP_MEOW_TRIP_MIN_PER    3u
#define APP_MEOW_TRIP_GAIN_REF   24 /* 饭团单次耐久收益,时长锚点 */
#define APP_MEOW_TRIP_PACK_MAX   20
#define APP_MEOW_TRIP_SOUV_MIN   6

/* 背包分类:食物(含饮料)、药品、道具。 */
typedef enum {
    APP_MEOW_CAT_FOOD = 0,
    APP_MEOW_CAT_MED,
    APP_MEOW_CAT_GEAR,
    APP_MEOW_CAT_N
} app_meow_cat_t;

typedef enum {
    APP_MEOW_USE_MEAL = 0,
    APP_MEOW_USE_DRINK,
    APP_MEOW_USE_MED,
    APP_MEOW_USE_WASH,
    APP_MEOW_USE_SWEEP,
    APP_MEOW_USE_GROOM,
    APP_MEOW_USE_DIG
} app_meow_use_t;

#define APP_MEOW_AI_NONE  0
#define APP_MEOW_AI_WORM  1
#define APP_MEOW_AI_TUMMY 2
#define APP_MEOW_AI_COLD  3
#define APP_MEOW_AI_COUGH 4

#define APP_MEOW_GF_JUNK    1u
#define APP_MEOW_GF_HEALTHY 2u
#define APP_MEOW_GF_SHOVEL  4u

/* 只留能对应护理动作的商品。 */
typedef enum {
    APP_MEOW_G_BURGER = 0, /* 喂食,油腻 */
    APP_MEOW_G_ONIGIRI,    /* 喂食 */
    APP_MEOW_G_SALAD,      /* 喂食,清淡 */
    APP_MEOW_G_FRUIT,      /* 喂食,清淡 */
    APP_MEOW_G_WATER,      /* 喝水,清淡 */
    APP_MEOW_G_COLA,       /* 喝水,油腻 */
    APP_MEOW_G_WORM,       /* 治疗寄生虫 */
    APP_MEOW_G_STOMACH,    /* 治疗肠胃 */
    APP_MEOW_G_COLD,       /* 治疗感冒 */
    APP_MEOW_G_COUGH,      /* 治疗咳嗽 */
    APP_MEOW_G_VITAMIN,    /* 通用补体力 */
    APP_MEOW_G_SOAP,       /* 洗澡 */
    APP_MEOW_G_TRASH,      /* 清理便便 */
    APP_MEOW_G_TISSUE,     /* 清理 */
    APP_MEOW_G_SHOVEL,     /* 挖掘 / 遛弯多捡 */
    APP_MEOW_G_TOWEL,      /* 洗澡 */
    APP_MEOW_G_N
} app_meow_good_t;

typedef enum {
    APP_MEOW_LOOT_WALK = 0,
    APP_MEOW_LOOT_VISIT,
    APP_MEOW_LOOT_WIN,
    APP_MEOW_LOOT_DRAW,
    APP_MEOW_LOOT_PLAY,
    APP_MEOW_LOOT_TRIP
} app_meow_loot_t;

typedef enum {
    APP_MEOW_EGG = 0,
    APP_MEOW_BABY,
    APP_MEOW_CHILD,
    APP_MEOW_TEEN,
    APP_MEOW_ADULT,
    APP_MEOW_DEAD
} app_meow_stage_t;

typedef enum {
    APP_MEOW_FEED = 0,
    APP_MEOW_PLAY,
    APP_MEOW_CLEAN,
    APP_MEOW_HEAL,
    APP_MEOW_LIGHT,
    APP_MEOW_RESET,
    APP_MEOW_DRINK,
    APP_MEOW_BATH,
    APP_MEOW_WALK,
    APP_MEOW_PET,
    APP_MEOW_BED
} app_meow_act_t;

typedef enum {
    APP_MEOW_OK = 0,
    APP_MEOW_SLEEP,
    APP_MEOW_FULL,
    APP_MEOW_NONE,
    APP_MEOW_EGG_WAIT,
    APP_MEOW_GONE,
    APP_MEOW_EMPTY
} app_meow_res_t;

/* 物种:蛋=0,幼=1,童=2,少=3/4,成=5..10。护理好偏向低编号,仍有随机。 */
#define APP_MEOW_SP_NONE    0
#define APP_MEOW_SP_BABY    1
#define APP_MEOW_SP_CHILD   2
#define APP_MEOW_SP_TEEN_A  3
#define APP_MEOW_SP_TEEN_B  4
#define APP_MEOW_SP_ADULT_0 5
#define APP_MEOW_SP_MAX     10

#define APP_MEOW_ALERT_OK   0
#define APP_MEOW_ALERT_WARN 1  /* ≤50% */
#define APP_MEOW_ALERT_HIT  2  /* ≤30% */
#define APP_MEOW_ALERT_CRIT 3  /* ≤10% */
#define APP_MEOW_ALERT_PCT_WARN 50
#define APP_MEOW_ALERT_PCT_HIT  30
#define APP_MEOW_ALERT_PCT_CRIT 10

#define APP_MEOW_D_HUNGER 0
#define APP_MEOW_D_HAPPY  1
#define APP_MEOW_D_HEALTH 2
#define APP_MEOW_D_SICK   3
#define APP_MEOW_D_POOP   4
#define APP_MEOW_D_LIGHT  5
#define APP_MEOW_D_N      6

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
    uint8_t hatch_min;  /* 蛋:已过真实秒,满 APP_MEOW_HATCH_SEC 孵化 */
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
    uint16_t inv_n[APP_MEOW_G_N]; /* 每种数量,最多 999 */
    uint8_t inv_d[APP_MEOW_G_N];  /* 当前打开那份的剩余耐久 */
    uint8_t trip_st;              /* 0 在家,1 旅行中,2 已回来待领 */
    uint8_t trip_pack;            /* 带上的食物份数 */
    uint16_t trip_left;           /* 剩余游戏分钟 */
    uint16_t found;               /* 收藏纪念品 bitmask */
    uint8_t level;                /* 1..99,蛋为 0 */
    uint16_t xp;                  /* 当前级经验 */
    uint8_t named;                /* 1=已确认过名字,空名字表示用默认 */
    char name[APP_MEOW_NAME_MAX + 1];
    uint16_t trip_gain;           /* 出发时的食物收益,回来发奖用 */
} app_meow_t;

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
} app_meow_snap_t;

#define APP_MEOW_KIND_VISIT 1
#define APP_MEOW_KIND_FIGHT 2

void app_meow_reset(app_meow_t *p, uint32_t now_sec, uint8_t rng);
/* 清空背包、收藏、名字；宠物直接进入离开。 */
void app_meow_wipe(app_meow_t *p, uint32_t now_sec, uint8_t rng);
bool app_meow_valid(const app_meow_t *p);
/* 读 NVS blob。支持 v2 的 32 字节、v3 的 37 字节、v4 的 44 种背包、v5 的 8 位数量。 */
bool app_meow_import(app_meow_t *p, const void *raw, size_t n);
uint16_t app_meow_inv(const app_meow_t *p, int good);
uint8_t app_meow_dur(const app_meow_t *p, int good);
int app_meow_give(app_meow_t *p, int good, int n);
/* 按来源抽材料。返回商品 id,没有则 -1。遛弯带金铲铲会多捡一份。 */
int app_meow_loot(app_meow_t *p, app_meow_loot_t src);
/* 喂食含饮料、洗澡含清理、治疗含药品;按目录轮流,并跳过当前用不成的。 */
int app_meow_pick(const app_meow_t *p, app_meow_act_t act);
int app_meow_good_cat(int good);
int app_meow_good_use(int good);
int app_meow_good_dur_max(int good);
/* 单次耐久收益: 饱食+心情+健康-脏度,低于 0 记 0。 */
int app_meow_good_gain(int good);
const char *app_meow_name(const app_meow_t *p);
void app_meow_set_name(app_meow_t *p, const char *name);
/* 按完整码点拷贝,最多 NAME_CHARS 个、NAME_MAX 字节。返回写入字节数。 */
size_t app_meow_name_copy(char *dst, size_t dst_n, const char *src);
int app_meow_owned_n(const app_meow_t *p, int cat);
int app_meow_owned_list(const app_meow_t *p, int cat, uint8_t *out, int max);
app_meow_res_t app_meow_use(app_meow_t *p, int good);
int app_meow_last_got(void);
/* hour 为 0..23; -1 表示时钟未知,不改睡觉状态。默认 21:00–08:00。 */
void app_meow_advance(app_meow_t *p, uint32_t now_sec, int hour);
void app_meow_advance_night(app_meow_t *p, uint32_t now_sec, int hour,
                            int bed, int wake);
/* 时钟未知或 bed==wake 时不睡。bed>wake 为跨夜,wake 整点已醒。 */
bool app_meow_asleep_at(int hour, int bed, int wake);
app_meow_res_t app_meow_act(app_meow_t *p, app_meow_act_t act);
/* 1 猜中, 0 猜错, -1 当前不能玩。guess 0=左 1=右。 */
int app_meow_play(app_meow_t *p, int guess);
/* 玩耍结算: win 非 0 心情升, 0 心情降。不能玩返回 -1。 */
int app_meow_play_apply(app_meow_t *p, int win);
/* 按积分发奖: 每 100 分随机 1 份。返回发出数量。 */
int app_meow_play_prize(app_meow_t *p, int score);
/* 小猫快跑结算: 本局碰到的物品入包。返回实际发出数量。 */
int app_meow_run_prize(app_meow_t *p, const uint8_t got[APP_MEOW_G_N]);
/* 只抽不发。满员或无效返回 -1。 */
int app_meow_roll(app_meow_t *p, app_meow_loot_t src);
void app_meow_last_prizes(uint8_t out[APP_MEOW_G_N]);
/* 难度档: 每 100 分 +1, 500 分封顶。游动 / 锦鲤概率 300 起 30%, 之后每档 +30%。 */
int app_meow_play_tier(int score);
int app_meow_play_swim_pct(int score);
int app_meow_play_koi_pct(int score);
bool app_meow_can(const app_meow_t *p, app_meow_act_t act);
/* 关灯后的睡觉才锁护理;灯还开着时仍可喂饱、清理、治疗。 */
bool app_meow_rest_lock(const app_meow_t *p);
/* 进入睡觉时段且还没安顿好(灯开着,或仍有护理危险)时提醒。 */
bool app_meow_bed_call(const app_meow_t *p);
bool app_meow_can_link(const app_meow_t *p);
void app_meow_snap(const app_meow_t *p, app_meow_snap_t *out);
int app_meow_power(const app_meow_snap_t *s);
app_meow_res_t app_meow_visit(app_meow_t *p);
/* 1 赢, 0 平, -1 输, -2 不能打。 */
int app_meow_fight(app_meow_t *me, const app_meow_snap_t *you);

int app_meow_care_score(const app_meow_t *p);
uint8_t app_meow_danger_lv(const app_meow_t *p, int danger);
uint8_t app_meow_alert_peak(const app_meow_t *p);
/* 若有新的一档提醒要播,返回 1 并写出危险种类和档位。 */
int app_meow_alert_poll(app_meow_t *p, int *danger, int *lv);
bool app_meow_trip_can(const app_meow_t *p);
int app_meow_trip_take_gain(const uint8_t take[APP_MEOW_G_N]);
int app_meow_trip_mins(int gain);
int app_meow_trip_sec(int gain);
int app_meow_trip_rewards(int gain);
int app_meow_trip_souv_pct(int pack);
int app_meow_trip_sec_left(const app_meow_t *p, uint32_t now_sec);
app_meow_res_t app_meow_trip_start(app_meow_t *p, const uint8_t take[APP_MEOW_G_N]);
int app_meow_trip_claim(app_meow_t *p);
int app_meow_last_souv(void);
bool app_meow_souv_on(const app_meow_t *p, int id);

int app_meow_level(const app_meow_t *p);
int app_meow_xp(const app_meow_t *p);
int app_meow_xp_need(int level);
int app_meow_xp_pct(const app_meow_t *p);
int app_meow_clean(const app_meow_t *p);
int app_meow_hunger_every(const app_meow_t *p);
int app_meow_dirt_every(const app_meow_t *p);
int app_meow_good_tier(int good);
