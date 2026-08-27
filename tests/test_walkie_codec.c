#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "walkie_codec.h"

int main(void)
{
    assert(walkie_ch_ok(1) && walkie_ch_ok(8));
    assert(!walkie_ch_ok(0) && !walkie_ch_ok(9));
    assert(walkie_ch_clamp(0) == 1);
    assert(walkie_ch_clamp(99) == 8);

    int16_t pcm[WALKIE_SAMPLES];
    for (int i = 0; i < WALKIE_SAMPLES; i++) {
        pcm[i] = (int16_t)((i & 1) ? 4000 : -4000);
    }

    walkie_adpcm_t enc, dec;
    walkie_adpcm_init(&enc);
    walkie_adpcm_init(&dec);
    walkie_meta_t meta = {
        .ch = 3, .seq = 7, .flags = WALKIE_F_AUDIO | WALKIE_F_PTT,
        .pred = enc.pred, .index = enc.index,
    };
    uint8_t adpcm[WALKIE_ADPCM_N];
    walkie_adpcm_encode(&enc, pcm, WALKIE_SAMPLES, adpcm);

    uint8_t frame[WALKIE_FRAME_N];
    int n = walkie_pack(frame, sizeof(frame), &meta, adpcm, WALKIE_ADPCM_N);
    assert(n == WALKIE_FRAME_N);

    walkie_meta_t got;
    const uint8_t *payload = NULL;
    int pn = -1;
    assert(walkie_unpack(frame, (size_t)n, &got, &payload, &pn));
    assert(got.ch == 3 && got.seq == 7);
    assert(got.flags & WALKIE_F_AUDIO);
    assert(pn == WALKIE_ADPCM_N && payload);

    walkie_adpcm_t rst = { .pred = got.pred, .index = got.index };
    int16_t out[WALKIE_SAMPLES];
    walkie_adpcm_decode(&rst, payload, pn, out);

    long err = 0;
    for (int i = 0; i < WALKIE_SAMPLES; i++) {
        int d = (int)out[i] - (int)pcm[i];
        if (d < 0) d = -d;
        err += d;
    }
    assert(err / WALKIE_SAMPLES < 800);

    uint8_t bad[WALKIE_HDR_N];
    memset(bad, 0, sizeof(bad));
    assert(!walkie_unpack(bad, sizeof(bad), &got, NULL, NULL));
    assert(!walkie_unpack(frame, 4, &got, NULL, NULL));

    walkie_meta_t hello = { .ch = 1, .seq = 0, .flags = WALKIE_F_HELLO };
    uint8_t hbuf[WALKIE_HDR_N];
    assert(walkie_pack(hbuf, sizeof(hbuf), &hello, NULL, 0) == WALKIE_HDR_N);
    assert(walkie_unpack(hbuf, sizeof(hbuf), &got, &payload, &pn));
    assert(got.flags == WALKIE_F_HELLO && pn == 0);

    puts("ok");
    return 0;
}
