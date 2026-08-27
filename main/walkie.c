#include "walkie.h"

#include "app_prefs.h"
#include "bsp_audio.h"
#include "bsp_ble.h"
#include "bsp_wifi.h"
#include "walkie_ble.h"
#include "walkie_rtc.h"

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <stdint.h>
#include <string.h>

static const char *TAG = "walkie";

#define Q_N         6
#define PEER_MAX    4
#define PEER_TTL    5000
#define LISTEN_MS   400
#define STACK_BYTES 4096
#define CODEC_HZ    16000
#define CODEC_N     (WALKIE_SAMPLES * CODEC_HZ / WALKIE_HZ)

typedef struct {
    uint8_t data[WALKIE_FRAME_N];
    uint8_t n;
} pkt_t;

static TaskHandle_t s_task;
static QueueHandle_t s_q;
static SemaphoreHandle_t s_mu;
static volatile bool s_run;
static volatile bool s_ptt;
static volatile walkie_err_t s_err;
static walkie_mode_t s_mode;
static int s_ch = 1;
static uint8_t s_seq;
static uint32_t s_rx_ms;
static char s_peer[PEER_MAX][24];
static uint32_t s_peer_ms[PEER_MAX];

static StaticQueue_t s_qctl;
static uint8_t s_qstore[Q_N * sizeof(pkt_t)];
static StaticSemaphore_t s_mu_buf;
static int16_t s_pcm[WALKIE_SAMPLES];
static int16_t s_out[WALKIE_SAMPLES];
static int16_t s_raw[CODEC_N];
static uint8_t s_adp[WALKIE_ADPCM_N];
static int s_dc_x, s_dc_y;

static uint32_t now_ms(void)
{
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

void walkie_note_peer(const char *name)
{
    if (!s_mu) return;
    char tmp[24];
    if (!name || !name[0]) strlcpy(tmp, "peer", sizeof(tmp));
    else strlcpy(tmp, name, sizeof(tmp));
    uint32_t t = now_ms();
    xSemaphoreTake(s_mu, portMAX_DELAY);
    int slot = -1;
    for (int i = 0; i < PEER_MAX; i++) {
        if (strcmp(s_peer[i], tmp) == 0) {
            s_peer_ms[i] = t;
            xSemaphoreGive(s_mu);
            return;
        }
        if (slot < 0 && !s_peer[i][0]) slot = i;
    }
    if (slot < 0) {
        uint32_t oldest = UINT32_MAX;
        for (int i = 0; i < PEER_MAX; i++) {
            if (s_peer_ms[i] < oldest) {
                oldest = s_peer_ms[i];
                slot = i;
            }
        }
    }
    if (slot >= 0) {
        strlcpy(s_peer[slot], tmp, sizeof(s_peer[slot]));
        s_peer_ms[slot] = t;
    }
    xSemaphoreGive(s_mu);
}

static void prune_peers(void)
{
    uint32_t t = now_ms();
    xSemaphoreTake(s_mu, portMAX_DELAY);
    for (int i = 0; i < PEER_MAX; i++) {
        if (s_peer[i][0] && (t - s_peer_ms[i]) > PEER_TTL) s_peer[i][0] = 0;
    }
    xSemaphoreGive(s_mu);
}

void walkie_rx_bytes(const uint8_t *p, size_t n)
{
    if (!p || !s_run) return;
    walkie_meta_t m;
    const uint8_t *adp = NULL;
    int an = 0;
    if (!walkie_unpack(p, n, &m, &adp, &an)) return;
    if (m.ch != (uint8_t)s_ch) return;
    if (m.flags & WALKIE_F_HELLO) {
        char name[24];
        int cn = an < (int)sizeof(name) - 1 ? an : (int)sizeof(name) - 1;
        if (cn > 0 && adp) {
            memcpy(name, adp, (size_t)cn);
            name[cn] = 0;
            walkie_note_peer(name);
        } else {
            walkie_note_peer("peer");
        }
        return;
    }
    if (!(m.flags & WALKIE_F_AUDIO) || an <= 0 || s_ptt) return;
    if (!s_q) return;
    pkt_t pkt;
    if (n > sizeof(pkt.data)) n = sizeof(pkt.data);
    memcpy(pkt.data, p, n);
    pkt.n = (uint8_t)n;
    xQueueSend(s_q, &pkt, 0);
}

static void send_frame(uint8_t flags, const uint8_t *adp, int an, const walkie_adpcm_t *st)
{
    walkie_meta_t m = {
        .ch = (uint8_t)s_ch,
        .seq = s_seq++,
        .flags = flags,
        .pred = st ? st->pred : 0,
        .index = st ? st->index : 0,
    };
    uint8_t buf[WALKIE_FRAME_N];
    int n = walkie_pack(buf, sizeof(buf), &m, adp, an);
    if (n < 0) return;
    if (s_mode == WALKIE_MODE_WEBRTC) walkie_rtc_send(buf, (size_t)n);
    else walkie_ble_send(buf, (size_t)n);
}

static void send_hello(void)
{
    const char *name = "Passport";
    if (bsp_ble_name()[0]) name = bsp_ble_name();
    walkie_adpcm_t z;
    walkie_adpcm_init(&z);
    send_frame(WALKIE_F_HELLO, (const uint8_t *)name, (int)strlen(name), &z);
}

static void fail_task(walkie_err_t e)
{
    ESP_LOGE(TAG, "start abort err=%d heap=%u largest=%u",
             (int)e,
             (unsigned)esp_get_free_heap_size(),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    s_err = e;
    s_run = false;
    s_task = NULL;
    walkie_rtc_stop();
    walkie_ble_stop();
    bsp_audio_standby();
    vTaskDelete(NULL);
}

static bool codec_open(void)
{
    /* 整机其它音频都是 16 kHz。8 kHz 重开会在 ES8311 上失败,两种模式一起 start failed。 */
    if (bsp_audio_set_format(CODEC_HZ, 16, 1) != ESP_OK) return false;
    uint8_t vol = app_prefs()->muted ? 0 : app_prefs()->volume;
    bsp_audio_set_volume(vol);
    return true;
}

static int16_t dc_block(int16_t x)
{
    /* y = x - x1 + 0.996 y1,去掉麦头直流,不然 ADPCM 台阶被偏置吃掉、底噪变大。 */
    int y = (int)x - s_dc_x + ((s_dc_y * 255) >> 8);
    s_dc_x = x;
    if (y > 32767) y = 32767;
    if (y < -32768) y = -32768;
    s_dc_y = y;
    return (int16_t)y;
}

static void down_pcm(void)
{
    int prev = s_raw[0];
    for (int i = 0; i < WALKIE_SAMPLES; i++) {
        int a = prev;
        int b = s_raw[i * 2];
        int c = s_raw[i * 2 + 1];
        prev = c;
        s_pcm[i] = dc_block((int16_t)((a + b + b + c) >> 2));
    }
}

static void gate_pcm(void)
{
    int64_t e = 0;
    for (int i = 0; i < WALKIE_SAMPLES; i++) {
        int v = s_pcm[i];
        e += (int64_t)v * v;
    }
    if (e < (int64_t)480 * 480 * WALKIE_SAMPLES) {
        memset(s_pcm, 0, sizeof(s_pcm));
    }
}

static void up_pcm(int n)
{
    if (n > WALKIE_SAMPLES) n = WALKIE_SAMPLES;
    for (int i = 0; i < n; i++) {
        int a = s_out[i];
        int b = (i + 1 < n) ? s_out[i + 1] : a;
        s_raw[i * 2] = (int16_t)a;
        s_raw[i * 2 + 1] = (int16_t)((a + b) / 2);
    }
    for (int i = n * 2; i < CODEC_N; i++) s_raw[i] = 0;
}

static void audio_task(void *arg)
{
    (void)arg;
    walkie_adpcm_t enc, dec;
    walkie_adpcm_init(&enc);
    walkie_adpcm_init(&dec);
    s_dc_x = s_dc_y = 0;
    int hello = 0;

    esp_err_t te = ESP_OK;
    if (s_mode == WALKIE_MODE_WEBRTC) te = walkie_rtc_start(s_ch);
    else te = walkie_ble_start(s_ch);
    if (te != ESP_OK) {
        if (s_mode == WALKIE_MODE_WEBRTC) {
            fail_task(bsp_wifi_state() != BSP_WIFI_CONNECTED ? WALKIE_E_WIFI : WALKIE_E_NET);
        } else {
            fail_task(bsp_ble_enabled() ? WALKIE_E_NET : WALKIE_E_BLE);
        }
        return;
    }

    bool codec = codec_open();
    if (!codec) ESP_LOGW(TAG, "codec 16k retry later");
    send_hello();
    ESP_LOGI(TAG, "running mode=%d ch=%d codec=%d heap=%u",
             (int)s_mode, s_ch, (int)codec, (unsigned)esp_get_free_heap_size());

    while (s_run) {
        if (!codec) {
            codec = codec_open();
            if (!codec) {
                if (s_mode == WALKIE_MODE_WEBRTC) walkie_rtc_poll();
                if (++hello >= 50) {
                    hello = 0;
                    send_hello();
                }
                vTaskDelay(pdMS_TO_TICKS(20));
                continue;
            }
        }
        if (bsp_audio_read(s_raw, sizeof(s_raw)) != ESP_OK) {
            codec = false;
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        down_pcm();
        if (s_mode == WALKIE_MODE_WEBRTC) walkie_rtc_poll();

        memset(s_out, 0, sizeof(s_out));
        if (s_ptt) {
            gate_pcm();
            walkie_adpcm_t snap = enc;
            walkie_adpcm_encode(&enc, s_pcm, WALKIE_SAMPLES, s_adp);
            send_frame(WALKIE_F_AUDIO | WALKIE_F_PTT, s_adp, WALKIE_ADPCM_N, &snap);
        } else {
            pkt_t pkt;
            if (s_q && xQueueReceive(s_q, &pkt, 0) == pdTRUE) {
                walkie_meta_t m;
                const uint8_t *payload = NULL;
                int pn = 0;
                if (walkie_unpack(pkt.data, pkt.n, &m, &payload, &pn) && pn > 0) {
                    walkie_adpcm_t st = { .pred = m.pred, .index = m.index };
                    int samples = pn * 2;
                    if (samples > WALKIE_SAMPLES) samples = WALKIE_SAMPLES;
                    walkie_adpcm_decode(&st, payload, pn, s_out);
                    s_rx_ms = now_ms();
                }
            }
            if (++hello >= 50) {
                hello = 0;
                send_hello();
            }
        }
        /* I2S TX 一直要喂。只在有包时 write 会欠载,喇叭出现咔嗒和底噪。 */
        up_pcm(WALKIE_SAMPLES);
        bsp_audio_write(s_raw, sizeof(s_raw));
    }
    s_task = NULL;
    vTaskDelete(NULL);
}

esp_err_t walkie_start(walkie_mode_t mode, int ch)
{
    if (s_run || s_task) return ESP_OK;
    s_ch = walkie_ch_clamp(ch);
    s_mode = mode;
    s_ptt = false;
    s_seq = 0;
    s_rx_ms = 0;
    s_err = WALKIE_E_OK;
    memset(s_peer, 0, sizeof(s_peer));
    if (!s_mu) s_mu = xSemaphoreCreateMutexStatic(&s_mu_buf);
    if (!s_q) s_q = xQueueCreateStatic(Q_N, sizeof(pkt_t), s_qstore, &s_qctl);
    if (!s_mu || !s_q) {
        s_err = WALKIE_E_MEM;
        return ESP_ERR_NO_MEM;
    }
    pkt_t dump;
    while (xQueueReceive(s_q, &dump, 0) == pdTRUE) {}

    s_run = true;
    if (xTaskCreate(audio_task, "walkie", STACK_BYTES, NULL, 5, &s_task) != pdPASS) {
        s_task = NULL;
        s_run = false;
        s_err = WALKIE_E_MEM;
        ESP_LOGE(TAG, "task create failed heap=%u largest=%u",
                 (unsigned)esp_get_free_heap_size(),
                 (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "task start mode=%d ch=%d heap=%u largest=%u",
             (int)mode, s_ch,
             (unsigned)esp_get_free_heap_size(),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    return ESP_OK;
}

void walkie_stop(void)
{
    s_ptt = false;
    s_run = false;
    for (int i = 0; i < 40 && s_task; i++) vTaskDelay(pdMS_TO_TICKS(20));
    walkie_rtc_stop();
    walkie_ble_stop();
    memset(s_peer, 0, sizeof(s_peer));
    bsp_audio_standby();
}

walkie_err_t walkie_last_err(void)
{
    return s_err;
}

void walkie_clear_err(void)
{
    s_err = WALKIE_E_OK;
}

void walkie_set_ptt(bool on)
{
    s_ptt = on && s_run;
}

bool walkie_busy(void)
{
    return s_run;
}

bool walkie_ptt(void)
{
    return s_ptt;
}

walkie_mode_t walkie_mode(void)
{
    return s_mode;
}

int walkie_channel(void)
{
    return s_ch;
}

walkie_run_t walkie_run(void)
{
    if (!s_run) return WALKIE_OFF;
    if (s_ptt) return WALKIE_TALK;
    uint32_t t = now_ms();
    if (s_rx_ms && (t - s_rx_ms) < LISTEN_MS) return WALKIE_LISTEN;
    if (walkie_peer_n() > 0 || (s_mode == WALKIE_MODE_BLE && walkie_ble_linked())
        || (s_mode == WALKIE_MODE_WEBRTC && walkie_rtc_ws_n() > 0)) {
        return WALKIE_LINK;
    }
    return WALKIE_WAIT;
}

int walkie_peer_n(void)
{
    if (!s_mu) return 0;
    prune_peers();
    int n = 0;
    xSemaphoreTake(s_mu, portMAX_DELAY);
    for (int i = 0; i < PEER_MAX; i++) if (s_peer[i][0]) n++;
    xSemaphoreGive(s_mu);
    return n;
}

const char *walkie_peer_name(int i)
{
    static char buf[24];
    buf[0] = 0;
    if (!s_mu || i < 0) return buf;
    prune_peers();
    xSemaphoreTake(s_mu, portMAX_DELAY);
    int k = 0;
    for (int p = 0; p < PEER_MAX; p++) {
        if (!s_peer[p][0]) continue;
        if (k == i) {
            strlcpy(buf, s_peer[p], sizeof(buf));
            break;
        }
        k++;
    }
    xSemaphoreGive(s_mu);
    return buf;
}
