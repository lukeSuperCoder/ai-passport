#include "walkie_codec.h"

#include <string.h>

static const int16_t STEP[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230,
    253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963,
    1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327,
    3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442,
    11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794,
    32767
};

static const int8_t IDX[8] = { -1, -1, -1, -1, 2, 4, 6, 8 };

static int16_t sat16(int v)
{
    if (v > 32767) return 32767;
    if (v < -32768) return -32768;
    return (int16_t)v;
}

static void clamp_index(walkie_adpcm_t *s)
{
    if (s->index < 0) s->index = 0;
    if (s->index > 88) s->index = 88;
}

static uint8_t encode_nibble(walkie_adpcm_t *s, int16_t sample)
{
    int step = STEP[s->index];
    int diff = (int)sample - (int)s->pred;
    uint8_t n = 0;
    if (diff < 0) {
        n = 8;
        diff = -diff;
    }
    int mask = 4;
    int d = step;
    for (int i = 0; i < 3; i++) {
        if (diff >= d) {
            n |= (uint8_t)mask;
            diff -= d;
        }
        d >>= 1;
        mask >>= 1;
    }
    int delta = ((n & 7) * 2 + 1) * step >> 3;
    if (n & 8) s->pred = sat16((int)s->pred - delta);
    else s->pred = sat16((int)s->pred + delta);
    s->index = (int8_t)(s->index + IDX[n & 7]);
    clamp_index(s);
    return n;
}

static int16_t decode_nibble(walkie_adpcm_t *s, uint8_t n)
{
    int step = STEP[s->index];
    int diff = step >> 3;
    if (n & 4) diff += step;
    if (n & 2) diff += step >> 1;
    if (n & 1) diff += step >> 2;
    if (n & 8) s->pred = sat16((int)s->pred - diff);
    else s->pred = sat16((int)s->pred + diff);
    s->index = (int8_t)(s->index + IDX[n & 7]);
    clamp_index(s);
    return s->pred;
}

void walkie_adpcm_init(walkie_adpcm_t *s)
{
    if (!s) return;
    s->pred = 0;
    s->index = 0;
}

void walkie_adpcm_encode(walkie_adpcm_t *s, const int16_t *pcm, int n, uint8_t *out)
{
    if (!s || !pcm || !out || n <= 0) return;
    int bytes = n / 2;
    for (int i = 0; i < bytes; i++) {
        uint8_t lo = encode_nibble(s, pcm[i * 2]);
        uint8_t hi = encode_nibble(s, pcm[i * 2 + 1]);
        out[i] = (uint8_t)(lo | (hi << 4));
    }
    if (n & 1) {
        uint8_t lo = encode_nibble(s, pcm[n - 1]);
        out[bytes] = lo;
    }
}

void walkie_adpcm_decode(walkie_adpcm_t *s, const uint8_t *in, int n_bytes, int16_t *pcm)
{
    if (!s || !in || !pcm || n_bytes <= 0) return;
    int o = 0;
    for (int i = 0; i < n_bytes; i++) {
        pcm[o++] = decode_nibble(s, (uint8_t)(in[i] & 0x0F));
        pcm[o++] = decode_nibble(s, (uint8_t)(in[i] >> 4));
    }
}

int walkie_pack(uint8_t *out, size_t cap, const walkie_meta_t *m, const uint8_t *adpcm, int adpcm_n)
{
    if (!out || !m) return -1;
    if (adpcm_n < 0) adpcm_n = 0;
    if (adpcm_n && !adpcm) return -1;
    int need = WALKIE_HDR_N + adpcm_n;
    if ((int)cap < need) return -1;
    out[0] = WALKIE_MAGIC0;
    out[1] = WALKIE_MAGIC1;
    out[2] = WALKIE_VER;
    out[3] = m->ch;
    out[4] = m->seq;
    out[5] = m->flags;
    out[6] = (uint8_t)(m->pred & 0xFF);
    out[7] = (uint8_t)((m->pred >> 8) & 0xFF);
    out[8] = (uint8_t)m->index;
    out[9] = 0;
    if (adpcm_n) memcpy(out + WALKIE_HDR_N, adpcm, (size_t)adpcm_n);
    return need;
}

bool walkie_unpack(const uint8_t *in, size_t n, walkie_meta_t *m, const uint8_t **adpcm, int *adpcm_n)
{
    if (!in || !m || n < WALKIE_HDR_N) return false;
    if (in[0] != WALKIE_MAGIC0 || in[1] != WALKIE_MAGIC1) return false;
    if (in[2] != WALKIE_VER) return false;
    if (!walkie_ch_ok(in[3])) return false;
    m->ch = in[3];
    m->seq = in[4];
    m->flags = in[5];
    m->pred = (int16_t)((uint16_t)in[6] | ((uint16_t)in[7] << 8));
    m->index = (int8_t)in[8];
    int an = (int)n - WALKIE_HDR_N;
    if (adpcm) *adpcm = an > 0 ? in + WALKIE_HDR_N : NULL;
    if (adpcm_n) *adpcm_n = an > 0 ? an : 0;
    return true;
}

bool walkie_ch_ok(int ch)
{
    return ch >= WALKIE_CH_MIN && ch <= WALKIE_CH_MAX;
}

int walkie_ch_clamp(int ch)
{
    if (ch < WALKIE_CH_MIN) return WALKIE_CH_MIN;
    if (ch > WALKIE_CH_MAX) return WALKIE_CH_MAX;
    return ch;
}
