#include "save_service.h"

#include <string.h>

#define SAVE_MAGIC 0x5453544EU /* TSTN */
#define SAVE_FORMAT_VERSION 13U
#define SAVE_HEADER_SIZE 28U
#define SAVE_COMMIT_MARKER 0x434F4D4DU /* COMM */
#define SAVE_PAYLOAD_V1_SIZE 36U
#define SAVE_PAYLOAD_V2_SIZE 64U
#define SAVE_PAYLOAD_V3_SIZE 104U
#define SAVE_PAYLOAD_V4_SIZE 168U
#define SAVE_PAYLOAD_V5_SIZE 184U
#define SAVE_PAYLOAD_V6_SIZE 184U
#define SAVE_PAYLOAD_V7_SIZE 204U
#define SAVE_PAYLOAD_V8_SIZE 256U
#define SAVE_PAYLOAD_V9_SIZE 292U
#define SAVE_PAYLOAD_V10_SIZE 344U
#define SAVE_PAYLOAD_V11_SIZE 348U
#define SAVE_PAYLOAD_V12_SIZE 448U
#define SAVE_PAYLOAD_SIZE 472U

typedef struct {
    uint32_t sequence;
    uint32_t payload_length;
    uint32_t payload_crc;
    uint16_t format_version;
    bool valid;
} slot_info_t;

static uint16_t read_u16(const uint8_t *data)
{
    return (uint16_t)data[0] | (uint16_t)((uint16_t)data[1] << 8U);
}

static uint32_t read_u32(const uint8_t *data)
{
    return (uint32_t)data[0] |
           ((uint32_t)data[1] << 8U) |
           ((uint32_t)data[2] << 16U) |
           ((uint32_t)data[3] << 24U);
}

static void write_u16(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8U);
}

static void write_u32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8U);
    data[2] = (uint8_t)(value >> 16U);
    data[3] = (uint8_t)(value >> 24U);
}

static uint32_t crc32(const uint8_t *data, size_t size)
{
    uint32_t crc = 0xFFFFFFFFU;
    for (size_t i = 0; i < size; i++) {
        crc ^= data[i];
        for (unsigned bit = 0; bit < 8U; bit++) {
            uint32_t mask = (uint32_t)-(int32_t)(crc & 1U);
            crc = (crc >> 1U) ^ (0xEDB88320U & mask);
        }
    }
    return ~crc;
}

static void encode_payload(const game_state_t *state, uint8_t payload[SAVE_PAYLOAD_SIZE])
{
    memset(payload, 0, SAVE_PAYLOAD_SIZE);
    write_u32(payload + 0, state->coins);
    write_u32(payload + 4, state->last_trusted_time);
    write_u32(payload + 8, state->last_settled_time);
    write_u32(payload + 12, state->commit_sequence);
    payload[16] = (uint8_t)state->momo.job;
    payload[17] = state->momo.stamina;
    payload[18] = state->momo.mood;
    payload[19] = state->time_anomaly ? 1U : 0U;
    write_u32(payload + 20, state->momo.job_started_at);
    write_u32(payload + 24, state->pending.coins);
    write_u32(payload + 28, state->pending.elapsed_seconds);
    payload[32] = state->pending.available ? 1U : 0U;
    write_u16(payload + 34, state->pending.wood);
    write_u16(payload + 36, state->pending.berries);
    payload[38] = (uint8_t)state->amai.job;
    payload[39] = state->amai.stamina;
    payload[40] = state->amai.mood;
    write_u32(payload + 44, state->amai.job_started_at);
    payload[48] = state->forest.active ? 1U : 0U;
    write_u32(payload + 52, state->forest.task_id);
    write_u32(payload + 56, state->forest.started_at);
    write_u32(payload + 60, state->forest.ends_at);
    write_u16(payload + 64, state->pending.hot_bread);
    payload[66] = (uint8_t)state->atuan.job;
    payload[67] = state->atuan.stamina;
    payload[68] = state->atuan.mood;
    write_u32(payload + 72, state->atuan.job_started_at);
    payload[76] = (uint8_t)state->forest.kind;
    payload[77] = (uint8_t)state->forest.actor;
    payload[78] = state->kitchen.active ? 1U : 0U;
    payload[79] = (uint8_t)state->kitchen.kind;
    payload[80] = (uint8_t)state->kitchen.actor;
    write_u32(payload + 84, state->kitchen.task_id);
    write_u32(payload + 88, state->kitchen.started_at);
    write_u32(payload + 92, state->kitchen.ends_at);
    write_u16(payload + 96, state->inventory_wheat);
    write_u16(payload + 98, state->inventory_wood);
    write_u16(payload + 100, state->inventory_berries);
    write_u16(payload + 102, state->inventory_hot_bread);
    write_u16(payload + 104, state->pending.wheat);
    payload[106] = (uint8_t)state->lulu.job;
    payload[107] = state->lulu.stamina;
    payload[108] = state->lulu.mood;
    write_u32(payload + 112, state->lulu.job_started_at);
    write_u16(payload + 116, state->inventory_wheat_seed);
    for (size_t i = 0; i < GAME_FARM_INITIAL_PLOT_COUNT; i++) {
        size_t offset = 120U + i * 12U;
        payload[offset] = state->farm[i].active ? 1U : 0U;
        payload[offset + 1U] = (uint8_t)state->farm[i].crop;
        write_u32(payload + offset + 4U, state->farm[i].planted_at);
        write_u32(payload + offset + 8U, state->farm[i].matures_at);
    }
    write_u32(payload + 168, state->season_started_at);
    write_u32(payload + 172, state->weather_seed);
    payload[176] = state->spring_day;
    payload[177] = (uint8_t)state->weather;
    payload[178] = state->calendar_milestones;
    payload[179] = state->pending_events;
    payload[180] = state->completed_events;
    write_u16(payload + 182, state->pending.mushrooms);
    write_u16(payload + 184, state->inventory_mushrooms);
    payload[186] = state->travel_journal_count;
    payload[188] = state->travel.active ? 1U : 0U;
    payload[189] = (uint8_t)state->travel.kind;
    write_u32(payload + 192, state->travel.task_id);
    write_u32(payload + 196, state->travel.started_at);
    write_u32(payload + 200, state->travel.ends_at);
    for (size_t i = 0; i < GAME_CROP_COUNT; i++) {
        write_u16(payload + 204U + i * 2U, state->inventory_crops[i]);
        write_u16(payload + 214U + i * 2U, state->inventory_seeds[i]);
        write_u16(payload + 234U + i * 2U, state->pending_crops[i]);
    }
    for (size_t i = 0; i < GAME_RECIPE_COUNT; i++) {
        write_u16(payload + 224U + i * 2U, state->inventory_dishes[i]);
        write_u16(payload + 244U + i * 2U, state->pending_dishes[i]);
    }
    payload[254] = state->unlocked_recipes;
    payload[255] = (uint8_t)state->kitchen.recipe;
    payload[256] = state->quest_stage;
    payload[257] = state->completed_buildings;
    payload[258] = state->reputation;
    payload[259] = state->forest_runs;
    payload[260] = state->road_fragments;
    payload[261] = state->chapter_complete ? 1U : 0U;
    write_u16(payload + 262, state->total_crops_harvested);
    for (size_t i = 0; i < GAME_RECIPE_COUNT; i++) {
        write_u16(payload + 264U + i * 2U, state->cooked_counts[i]);
    }
    payload[274] = state->construction.active ? 1U : 0U;
    payload[275] = (uint8_t)state->construction.kind;
    payload[276] = (uint8_t)state->construction.building;
    write_u32(payload + 280, state->construction.task_id);
    write_u32(payload + 284, state->construction.started_at);
    write_u32(payload + 288, state->construction.ends_at);
    memcpy(payload + 292, state->relationships, GAME_RELATION_COUNT);
    memcpy(payload + 298, state->player_affinity, GAME_PET_COUNT);
    size_t exp_offset = 302U;
    for (size_t pet = 0; pet < GAME_PET_COUNT; pet++) {
        for (size_t job = 0; job < 5U; job++) {
            write_u16(payload + exp_offset, state->job_experience[pet][job]);
            exp_offset += 2U;
        }
    }
    payload[342] = state->companion_actions;
    payload[343] = state->companion_actions_day;
    payload[344] = state->sound_enabled ? 1U : 0U;
    payload[345] = state->night_mute_enabled ? 1U : 0U;
    payload[346] = state->clock_24_hour ? 1U : 0U;
    for (size_t i = 0; i < GAME_EVENT_QUEUE_SIZE; i++) {
        payload[348U + i * 2U] = state->event_queue[i].id;
        payload[349U + i * 2U] = state->event_queue[i].queued_day;
    }
    payload[354] = state->event_queue_count;
    memcpy(payload + 355, state->event_last_day, GAME_CONTENT_EVENT_COUNT);
    memcpy(payload + 420, state->event_seen, sizeof(state->event_seen));
    memcpy(payload + 429, state->event_history, GAME_EVENT_HISTORY_SIZE);
    payload[439] = state->event_history_count;
    memcpy(payload + 440, state->visitor_stages, GAME_VISITOR_COUNT);
    for (size_t i = GAME_FARM_INITIAL_PLOT_COUNT;
         i < GAME_FARM_PLOT_COUNT; i++) {
        size_t offset = 448U + (i - GAME_FARM_INITIAL_PLOT_COUNT) * 12U;
        payload[offset] = state->farm[i].active ? 1U : 0U;
        payload[offset + 1U] = (uint8_t)state->farm[i].crop;
        write_u32(payload + offset + 4U, state->farm[i].planted_at);
        write_u32(payload + offset + 8U, state->farm[i].matures_at);
    }
}

static bool decode_payload(const uint8_t payload[SAVE_PAYLOAD_SIZE],
                           uint16_t version, game_state_t *state)
{
    game_state_t decoded;
    game_state_init(&decoded, 0U);
    decoded.coins = read_u32(payload + 0);
    decoded.last_trusted_time = read_u32(payload + 4);
    decoded.last_settled_time = read_u32(payload + 8);
    decoded.commit_sequence = read_u32(payload + 12);
    decoded.momo.job = (game_job_t)payload[16];
    decoded.momo.stamina = payload[17];
    decoded.momo.mood = payload[18];
    decoded.time_anomaly = payload[19] != 0U;
    decoded.momo.job_started_at = read_u32(payload + 20);
    decoded.pending.coins = read_u32(payload + 24);
    decoded.pending.elapsed_seconds = read_u32(payload + 28);
    decoded.pending.available = payload[32] != 0U;

    if (version >= 2U) {
        decoded.pending.wood = read_u16(payload + 34);
        decoded.pending.berries = read_u16(payload + 36);
        decoded.amai.job = (game_job_t)payload[38];
        decoded.amai.stamina = payload[39];
        decoded.amai.mood = payload[40];
        decoded.amai.job_started_at = read_u32(payload + 44);
        decoded.forest.active = payload[48] != 0U;
        decoded.forest.task_id = read_u32(payload + 52);
        decoded.forest.started_at = read_u32(payload + 56);
        decoded.forest.ends_at = read_u32(payload + 60);
        if (decoded.forest.active) {
            decoded.forest.kind = GAME_TASK_FOREST_30M;
            decoded.forest.actor = GAME_ACTOR_AMAI;
        }
    }
    if (version >= 3U) {
        decoded.pending.hot_bread = read_u16(payload + 64);
        decoded.atuan.job = (game_job_t)payload[66];
        decoded.atuan.stamina = payload[67];
        decoded.atuan.mood = payload[68];
        decoded.atuan.job_started_at = read_u32(payload + 72);
        decoded.forest.kind = (game_task_kind_t)payload[76];
        decoded.forest.actor = (game_actor_id_t)payload[77];
        decoded.kitchen.active = payload[78] != 0U;
        decoded.kitchen.kind = (game_task_kind_t)payload[79];
        decoded.kitchen.actor = (game_actor_id_t)payload[80];
        decoded.kitchen.task_id = read_u32(payload + 84);
        decoded.kitchen.started_at = read_u32(payload + 88);
        decoded.kitchen.ends_at = read_u32(payload + 92);
        decoded.inventory_wheat = read_u16(payload + 96);
        decoded.inventory_wood = read_u16(payload + 98);
        decoded.inventory_berries = read_u16(payload + 100);
        decoded.inventory_hot_bread = read_u16(payload + 102);
    }
    if (version >= 4U) {
        decoded.pending.wheat = read_u16(payload + 104);
        decoded.lulu.job = (game_job_t)payload[106];
        decoded.lulu.stamina = payload[107];
        decoded.lulu.mood = payload[108];
        decoded.lulu.job_started_at = read_u32(payload + 112);
        decoded.inventory_wheat_seed = read_u16(payload + 116);
        for (size_t i = 0; i < GAME_FARM_INITIAL_PLOT_COUNT; i++) {
            size_t offset = 120U + i * 12U;
            decoded.farm[i].active = payload[offset] != 0U;
            decoded.farm[i].crop = (game_crop_t)payload[offset + 1U];
            decoded.farm[i].planted_at = read_u32(payload + offset + 4U);
            decoded.farm[i].matures_at = read_u32(payload + offset + 8U);
        }
    }
    if (version >= 5U) {
        decoded.season_started_at = read_u32(payload + 168);
        decoded.weather_seed = read_u32(payload + 172);
        decoded.spring_day = payload[176];
        decoded.weather = (game_weather_t)payload[177];
        decoded.calendar_milestones = payload[178];
        if (version >= 6U) {
            decoded.pending_events = payload[179];
            decoded.completed_events = payload[180];
        }
    } else {
        decoded.season_started_at = decoded.last_trusted_time;
        decoded.weather_seed = decoded.last_trusted_time ^ 0x54494D45U;
        decoded.spring_day = 1U;
        decoded.weather = GAME_WEATHER_CLEAR;
        decoded.calendar_milestones = 0U;
    }
    if (version >= 7U) {
        decoded.pending.mushrooms = read_u16(payload + 182);
        decoded.inventory_mushrooms = read_u16(payload + 184);
        decoded.travel_journal_count = payload[186];
        decoded.travel.active = payload[188] != 0U;
        decoded.travel.kind = (game_task_kind_t)payload[189];
        decoded.travel.task_id = read_u32(payload + 192);
        decoded.travel.started_at = read_u32(payload + 196);
        decoded.travel.ends_at = read_u32(payload + 200);
    }
    if (version >= 8U) {
        for (size_t i = 0; i < GAME_CROP_COUNT; i++) {
            decoded.inventory_crops[i] = read_u16(payload + 204U + i * 2U);
            decoded.inventory_seeds[i] = read_u16(payload + 214U + i * 2U);
            decoded.pending_crops[i] = read_u16(payload + 234U + i * 2U);
        }
        for (size_t i = 0; i < GAME_RECIPE_COUNT; i++) {
            decoded.inventory_dishes[i] = read_u16(payload + 224U + i * 2U);
            decoded.pending_dishes[i] = read_u16(payload + 244U + i * 2U);
        }
        decoded.unlocked_recipes = payload[254];
        decoded.kitchen.recipe = (game_recipe_t)payload[255];
    }
    if (version >= 9U) {
        decoded.quest_stage = payload[256];
        decoded.completed_buildings = payload[257];
        decoded.reputation = payload[258];
        decoded.forest_runs = payload[259];
        decoded.road_fragments = payload[260];
        decoded.chapter_complete = payload[261] != 0U;
        decoded.total_crops_harvested = read_u16(payload + 262);
        for (size_t i = 0; i < GAME_RECIPE_COUNT; i++) {
            decoded.cooked_counts[i] = read_u16(payload + 264U + i * 2U);
        }
        decoded.construction.active = payload[274] != 0U;
        decoded.construction.kind = (game_task_kind_t)payload[275];
        decoded.construction.building = (game_building_t)payload[276];
        decoded.construction.task_id = read_u32(payload + 280);
        decoded.construction.started_at = read_u32(payload + 284);
        decoded.construction.ends_at = read_u32(payload + 288);
    }
    if (version >= 10U) {
        memcpy(decoded.relationships, payload + 292, GAME_RELATION_COUNT);
        memcpy(decoded.player_affinity, payload + 298, GAME_PET_COUNT);
        size_t exp_offset = 302U;
        for (size_t pet = 0; pet < GAME_PET_COUNT; pet++) {
            for (size_t job = 0; job < 5U; job++) {
                decoded.job_experience[pet][job] = read_u16(payload + exp_offset);
                exp_offset += 2U;
            }
        }
        decoded.companion_actions = payload[342];
        decoded.companion_actions_day = payload[343];
    }
    if (version >= 11U) {
        decoded.sound_enabled = payload[344] != 0U;
        decoded.night_mute_enabled = payload[345] != 0U;
        decoded.clock_24_hour = payload[346] != 0U;
    }
    if (version >= 12U) {
        memset(decoded.event_queue, 0, sizeof(decoded.event_queue));
        for (size_t i = 0; i < GAME_EVENT_QUEUE_SIZE; i++) {
            decoded.event_queue[i].id = payload[348U + i * 2U];
            decoded.event_queue[i].queued_day = payload[349U + i * 2U];
        }
        decoded.event_queue_count = payload[354];
        memcpy(decoded.event_last_day, payload + 355, GAME_CONTENT_EVENT_COUNT);
        memcpy(decoded.event_seen, payload + 420, sizeof(decoded.event_seen));
        memcpy(decoded.event_history, payload + 429, GAME_EVENT_HISTORY_SIZE);
        decoded.event_history_count = payload[439];
        memcpy(decoded.visitor_stages, payload + 440, GAME_VISITOR_COUNT);
    }
    if (version >= 13U) {
        for (size_t i = GAME_FARM_INITIAL_PLOT_COUNT;
             i < GAME_FARM_PLOT_COUNT; i++) {
            size_t offset = 448U + (i - GAME_FARM_INITIAL_PLOT_COUNT) * 12U;
            decoded.farm[i].active = payload[offset] != 0U;
            decoded.farm[i].crop = (game_crop_t)payload[offset + 1U];
            decoded.farm[i].planted_at = read_u32(payload + offset + 4U);
            decoded.farm[i].matures_at = read_u32(payload + offset + 8U);
        }
    }

    if (decoded.momo.job > GAME_JOB_FARM ||
        decoded.amai.job > GAME_JOB_FARM ||
        decoded.atuan.job > GAME_JOB_FARM ||
        decoded.lulu.job > GAME_JOB_FARM ||
        decoded.forest.kind > GAME_TASK_BUILDING ||
        decoded.kitchen.kind > GAME_TASK_BUILDING ||
        decoded.travel.kind > GAME_TASK_BUILDING ||
        decoded.construction.kind > GAME_TASK_BUILDING ||
        decoded.construction.building >= GAME_BUILD_COUNT ||
        decoded.quest_stage < 2U || decoded.quest_stage > 11U ||
        decoded.companion_actions > 2U ||
        decoded.event_queue_count > GAME_EVENT_QUEUE_SIZE ||
        decoded.event_history_count > GAME_EVENT_HISTORY_SIZE ||
        decoded.companion_actions_day < 1U ||
        decoded.companion_actions_day > GAME_SPRING_DAY_COUNT ||
        (decoded.completed_buildings & ~((1U << GAME_BUILD_COUNT) - 1U)) != 0U ||
        decoded.kitchen.recipe >= GAME_RECIPE_COUNT ||
        (decoded.unlocked_recipes & ~((1U << GAME_RECIPE_COUNT) - 1U)) != 0U ||
        decoded.forest.actor > GAME_ACTOR_ATUAN ||
        decoded.kitchen.actor > GAME_ACTOR_ATUAN ||
        decoded.spring_day < 1U || decoded.spring_day > GAME_SPRING_DAY_COUNT ||
        decoded.weather > GAME_WEATHER_STORM ||
        (decoded.pending_events & ~(GAME_EVENT_MARKET | GAME_EVENT_FESTIVAL)) != 0U ||
        (decoded.completed_events & ~(GAME_EVENT_MARKET | GAME_EVENT_FESTIVAL)) != 0U ||
        decoded.momo.stamina > 100U || decoded.momo.mood > 100U ||
        decoded.amai.stamina > 100U || decoded.amai.mood > 100U ||
        decoded.atuan.stamina > 100U || decoded.atuan.mood > 100U ||
        decoded.lulu.stamina > 100U || decoded.lulu.mood > 100U ||
        (decoded.forest.active && decoded.forest.ends_at < decoded.forest.started_at) ||
        (decoded.kitchen.active && decoded.kitchen.ends_at < decoded.kitchen.started_at) ||
        (decoded.construction.active &&
         decoded.construction.ends_at < decoded.construction.started_at)) {
        return false;
    }
    for (size_t i = 0; i < GAME_FARM_PLOT_COUNT; i++) {
        if (decoded.farm[i].crop >= GAME_CROP_COUNT ||
            (decoded.farm[i].active &&
             decoded.farm[i].matures_at < decoded.farm[i].planted_at)) {
            return false;
        }
    }
    for (size_t i = 0; i < GAME_RELATION_COUNT; i++) {
        if (decoded.relationships[i] > 100U) return false;
    }
    for (size_t i = 0; i < GAME_PET_COUNT; i++) {
        if (decoded.player_affinity[i] > 100U) return false;
    }
    for (size_t i = 0; i < decoded.event_queue_count; i++) {
        if (decoded.event_queue[i].id >= GAME_CONTENT_EVENT_COUNT ||
            decoded.event_queue[i].queued_day < 1U ||
            decoded.event_queue[i].queued_day > GAME_SPRING_DAY_COUNT) {
            return false;
        }
    }
    for (size_t i = 0; i < decoded.event_history_count; i++) {
        if (decoded.event_history[i] >= GAME_CONTENT_EVENT_COUNT) return false;
    }
    for (size_t i = 0; i < GAME_VISITOR_COUNT; i++) {
        if (decoded.visitor_stages[i] > 3U) return false;
    }
    *state = decoded;
    return true;
}

static slot_info_t inspect_slot(const save_backend_t *backend, unsigned slot)
{
    slot_info_t info = {0};
    uint8_t header[SAVE_HEADER_SIZE];
    size_t offset = (size_t)slot * SAVE_SLOT_SIZE;
    if (!backend->read(backend->context, offset, header, sizeof(header))) return info;
    uint16_t version = read_u16(header + 4);
    if (read_u32(header + 0) != SAVE_MAGIC ||
        (version < 1U || version > SAVE_FORMAT_VERSION) ||
        read_u16(header + 6) != SAVE_HEADER_SIZE ||
        read_u32(header + 24) != SAVE_COMMIT_MARKER) {
        return info;
    }
    if (read_u32(header + 20) != crc32(header, 20U)) return info;
    info.sequence = read_u32(header + 8);
    info.format_version = version;
    info.payload_length = read_u32(header + 12);
    info.payload_crc = read_u32(header + 16);
    info.valid = (version == 1U && info.payload_length == SAVE_PAYLOAD_V1_SIZE) ||
                 (version == 2U && info.payload_length == SAVE_PAYLOAD_V2_SIZE) ||
                 (version == 3U && info.payload_length == SAVE_PAYLOAD_V3_SIZE) ||
                 (version == 4U && info.payload_length == SAVE_PAYLOAD_V4_SIZE) ||
                 (version == 5U && info.payload_length == SAVE_PAYLOAD_V5_SIZE) ||
                 (version == 6U && info.payload_length == SAVE_PAYLOAD_V6_SIZE) ||
                 (version == 7U && info.payload_length == SAVE_PAYLOAD_V7_SIZE) ||
                 (version == 8U && info.payload_length == SAVE_PAYLOAD_V8_SIZE) ||
                 (version == 9U && info.payload_length == SAVE_PAYLOAD_V9_SIZE) ||
                 (version == 10U && info.payload_length == SAVE_PAYLOAD_V10_SIZE) ||
                 (version == 11U && info.payload_length == SAVE_PAYLOAD_V11_SIZE) ||
                 (version == 12U && info.payload_length == SAVE_PAYLOAD_V12_SIZE) ||
                 (version == 13U && info.payload_length == SAVE_PAYLOAD_SIZE);
    return info;
}

static bool read_slot(const save_backend_t *backend, unsigned slot,
                      const slot_info_t *info, game_state_t *state)
{
    uint8_t payload[SAVE_PAYLOAD_SIZE] = {0};
    size_t offset = (size_t)slot * SAVE_SLOT_SIZE + SAVE_HEADER_SIZE;
    if (!backend->read(backend->context, offset, payload, info->payload_length)) return false;
    if (crc32(payload, info->payload_length) != info->payload_crc) return false;
    return decode_payload(payload, info->format_version, state);
}

bool save_service_load(const save_backend_t *backend, game_state_t *state)
{
    if (!backend || !backend->read || !state) return false;
    slot_info_t slots[SAVE_SLOT_COUNT];
    for (unsigned i = 0; i < SAVE_SLOT_COUNT; i++) slots[i] = inspect_slot(backend, i);

    unsigned first = slots[1].valid &&
        (!slots[0].valid || slots[1].sequence > slots[0].sequence) ? 1U : 0U;
    unsigned second = 1U - first;
    if (slots[first].valid && read_slot(backend, first, &slots[first], state)) return true;
    return slots[second].valid && read_slot(backend, second, &slots[second], state);
}

bool save_service_store(const save_backend_t *backend, const game_state_t *state)
{
    if (!backend || !backend->read || !backend->write || !backend->erase || !state) {
        return false;
    }
    slot_info_t slots[SAVE_SLOT_COUNT];
    for (unsigned i = 0; i < SAVE_SLOT_COUNT; i++) slots[i] = inspect_slot(backend, i);

    unsigned target;
    uint32_t sequence = 1U;
    if (!slots[0].valid) {
        target = 0U;
    } else if (!slots[1].valid) {
        target = 1U;
        sequence = slots[0].sequence + 1U;
    } else if (slots[0].sequence <= slots[1].sequence) {
        target = 0U;
        sequence = slots[1].sequence + 1U;
    } else {
        target = 1U;
        sequence = slots[0].sequence + 1U;
    }

    uint8_t payload[SAVE_PAYLOAD_SIZE];
    uint8_t header[SAVE_HEADER_SIZE];
    encode_payload(state, payload);
    memset(header, 0xFF, sizeof(header));
    write_u32(header + 0, SAVE_MAGIC);
    write_u16(header + 4, SAVE_FORMAT_VERSION);
    write_u16(header + 6, SAVE_HEADER_SIZE);
    write_u32(header + 8, sequence);
    write_u32(header + 12, SAVE_PAYLOAD_SIZE);
    write_u32(header + 16, crc32(payload, sizeof(payload)));
    write_u32(header + 20, crc32(header, 20U));

    size_t base = (size_t)target * SAVE_SLOT_SIZE;
    if (!backend->erase(backend->context, base, SAVE_SLOT_SIZE) ||
        !backend->write(backend->context, base, header, sizeof(header)) ||
        !backend->write(backend->context, base + SAVE_HEADER_SIZE,
                        payload, sizeof(payload))) {
        return false;
    }
    uint8_t marker[4];
    write_u32(marker, SAVE_COMMIT_MARKER);
    return backend->write(backend->context, base + 24U, marker, sizeof(marker));
}
