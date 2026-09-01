#pragma once

#include <stdbool.h>
#include <stdint.h>

#define GAME_OFFLINE_CAP_SECONDS (7U * 24U * 60U * 60U)
#define GAME_RECEPTION_CAP_SECONDS (8U * 60U * 60U)

typedef enum {
    GAME_JOB_REST = 0,
    GAME_JOB_RECEPTION,
} game_job_t;

typedef struct {
    game_job_t job;
    uint8_t stamina;
    uint8_t mood;
    uint32_t job_started_at;
} game_pet_state_t;

typedef struct {
    uint32_t coins;
    uint32_t elapsed_seconds;
    bool available;
} game_pending_report_t;

typedef struct {
    uint32_t coins;
    uint32_t last_trusted_time;
    uint32_t last_settled_time;
    uint32_t commit_sequence;
    game_pet_state_t momo;
    game_pending_report_t pending;
    bool time_anomaly;
} game_state_t;

typedef enum {
    GAME_ACTION_ASSIGN_MOMO_RECEPTION = 0,
    GAME_ACTION_SETTLE_TO_TIME,
    GAME_ACTION_CLAIM_REPORT,
} game_action_type_t;

typedef struct {
    game_action_type_t type;
    uint32_t now;
} game_action_t;

void game_state_init(game_state_t *state, uint32_t now);
bool game_reduce(game_state_t *state, game_action_t action);

