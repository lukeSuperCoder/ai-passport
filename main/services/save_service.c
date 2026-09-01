#include "save_service.h"

#include <string.h>

#define SAVE_MAGIC 0x5453544EU /* TSTN */
#define SAVE_FORMAT_VERSION 3U
#define SAVE_HEADER_SIZE 28U
#define SAVE_COMMIT_MARKER 0x434F4D4DU /* COMM */
#define SAVE_PAYLOAD_V1_SIZE 36U
#define SAVE_PAYLOAD_V2_SIZE 64U
#define SAVE_PAYLOAD_SIZE 104U

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

    if (decoded.momo.job > GAME_JOB_KITCHEN ||
        decoded.amai.job > GAME_JOB_KITCHEN ||
        decoded.atuan.job > GAME_JOB_KITCHEN ||
        decoded.forest.kind > GAME_TASK_HOT_BREAD ||
        decoded.kitchen.kind > GAME_TASK_HOT_BREAD ||
        decoded.forest.actor > GAME_ACTOR_ATUAN ||
        decoded.kitchen.actor > GAME_ACTOR_ATUAN ||
        decoded.momo.stamina > 100U || decoded.momo.mood > 100U ||
        decoded.amai.stamina > 100U || decoded.amai.mood > 100U ||
        decoded.atuan.stamina > 100U || decoded.atuan.mood > 100U ||
        (decoded.forest.active && decoded.forest.ends_at < decoded.forest.started_at) ||
        (decoded.kitchen.active && decoded.kitchen.ends_at < decoded.kitchen.started_at)) {
        return false;
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
                 (version == 3U && info.payload_length == SAVE_PAYLOAD_SIZE);
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
