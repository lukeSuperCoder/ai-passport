#include "save_service.h"

#include <string.h>

#define SAVE_MAGIC 0x5453544EU /* TSTN */
#define SAVE_FORMAT_VERSION 1U
#define SAVE_HEADER_SIZE 28U
#define SAVE_COMMIT_MARKER 0x434F4D4DU /* COMM */
#define SAVE_PAYLOAD_SIZE 36U

typedef struct {
    uint32_t sequence;
    uint32_t payload_length;
    uint32_t payload_crc;
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
}

static bool decode_payload(const uint8_t payload[SAVE_PAYLOAD_SIZE], game_state_t *state)
{
    game_state_t decoded;
    memset(&decoded, 0, sizeof(decoded));
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

    if (decoded.momo.job > GAME_JOB_RECEPTION ||
        decoded.momo.stamina > 100U || decoded.momo.mood > 100U) {
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
    if (read_u32(header + 0) != SAVE_MAGIC ||
        read_u16(header + 4) != SAVE_FORMAT_VERSION ||
        read_u16(header + 6) != SAVE_HEADER_SIZE ||
        read_u32(header + 24) != SAVE_COMMIT_MARKER) {
        return info;
    }
    if (read_u32(header + 20) != crc32(header, 20U)) return info;
    info.sequence = read_u32(header + 8);
    info.payload_length = read_u32(header + 12);
    info.payload_crc = read_u32(header + 16);
    info.valid = info.payload_length == SAVE_PAYLOAD_SIZE;
    return info;
}

static bool read_slot(const save_backend_t *backend, unsigned slot,
                      const slot_info_t *info, game_state_t *state)
{
    uint8_t payload[SAVE_PAYLOAD_SIZE];
    size_t offset = (size_t)slot * SAVE_SLOT_SIZE + SAVE_HEADER_SIZE;
    if (!backend->read(backend->context, offset, payload, sizeof(payload))) return false;
    if (crc32(payload, sizeof(payload)) != info->payload_crc) return false;
    return decode_payload(payload, state);
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

