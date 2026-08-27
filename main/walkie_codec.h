#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define WALKIE_HZ         8000
#define WALKIE_SAMPLES    160          /* 20 ms */
#define WALKIE_ADPCM_N    80
#define WALKIE_HDR_N      10
#define WALKIE_FRAME_N    (WALKIE_HDR_N + WALKIE_ADPCM_N)
#define WALKIE_CH_MIN     1
#define WALKIE_CH_MAX     8
#define WALKIE_VER        1
#define WALKIE_MAGIC0     'W'
#define WALKIE_MAGIC1     'K'

#define WALKIE_F_PTT      0x01
#define WALKIE_F_HELLO    0x02
#define WALKIE_F_AUDIO    0x04

typedef struct {
    int16_t pred;
    int8_t index;
} walkie_adpcm_t;

typedef struct {
    uint8_t ch;
    uint8_t seq;
    uint8_t flags;
    int16_t pred;
    int8_t index;
} walkie_meta_t;

void walkie_adpcm_init(walkie_adpcm_t *s);
void walkie_adpcm_encode(walkie_adpcm_t *s, const int16_t *pcm, int n, uint8_t *out);
void walkie_adpcm_decode(walkie_adpcm_t *s, const uint8_t *in, int n_bytes, int16_t *pcm);

int walkie_pack(uint8_t *out, size_t cap, const walkie_meta_t *m, const uint8_t *adpcm, int adpcm_n);
bool walkie_unpack(const uint8_t *in, size_t n, walkie_meta_t *m, const uint8_t **adpcm, int *adpcm_n);

bool walkie_ch_ok(int ch);
int walkie_ch_clamp(int ch);
