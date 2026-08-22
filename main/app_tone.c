#include "app_tone.h"

#include "app_prefs.h"
#include "walkie.h"
#include "bsp_audio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include <stdlib.h>
#include <stdint.h>

#define SAMPLE_RATE 16000
#define CHUNK       256
/* codec open + I2S write needs the same 4 KB stack as demo_audio / walkie */
#define STACK_BYTES 4096

static QueueHandle_t s_q;
static TaskHandle_t s_task;
static int16_t s_pcm[CHUNK];

static void beep(int hz, int ms, int amp)
{
    int period = hz > 0 ? SAMPLE_RATE / hz : 2;
    int total = SAMPLE_RATE * ms / 1000;
    int phase = 0;
    while (total > 0) {
        int n = total < CHUNK ? total : CHUNK;
        for (int i = 0; i < n; i++) {
            s_pcm[i] = (int16_t)((phase < period / 2) ? amp : -amp);
            if (++phase >= period) phase = 0;
        }
        bsp_audio_write(s_pcm, (size_t)n * sizeof(int16_t));
        total -= n;
    }
}

static void silence(int ms)
{
    int total = SAMPLE_RATE * ms / 1000;
    while (total > 0) {
        int n = total < CHUNK ? total : CHUNK;
        for (int i = 0; i < n; i++) s_pcm[i] = 0;
        bsp_audio_write(s_pcm, (size_t)n * sizeof(int16_t));
        total -= n;
    }
}

static void play_id(int id)
{
    if (id == APP_TONE_OFF) return;
    if (app_prefs()->muted || walkie_busy()) return;
    if (bsp_audio_set_format(SAMPLE_RATE, 16, 1) != ESP_OK) return;
    bsp_audio_set_volume(app_prefs()->volume);
    switch (id) {
    case APP_TONE_BEEP:
        beep(880, 120, 5000);
        break;
    case APP_TONE_DOUBLE:
        beep(880, 90, 5000);
        silence(60);
        beep(880, 90, 5000);
        break;
    case APP_TONE_CHIME:
        beep(660, 100, 4500);
        beep(880, 140, 4500);
        break;
    case APP_TONE_TRIPLE:
        for (int i = 0; i < 3; i++) {
            beep(1200, 70, 6000);
            silence(50);
        }
        break;
    case APP_TONE_ALARM:
        beep(440, 160, 7000);
        beep(880, 160, 7000);
        beep(440, 200, 7000);
        break;
    default:
        break;
    }
    if (!walkie_busy()) bsp_audio_standby();
}

static void tone_task(void *arg)
{
    (void)arg;
    for (;;) {
        int id = 0;
        if (xQueueReceive(s_q, &id, portMAX_DELAY) == pdTRUE) play_id(id);
    }
}

void app_tone_start(void)
{
    if (s_q) return;
    s_q = xQueueCreate(4, sizeof(int));
    if (!s_q) return;
    xTaskCreate(tone_task, "app_tone", STACK_BYTES, NULL, 4, &s_task);
}

void app_tone_play(int id)
{
    if (!s_q || id == APP_TONE_OFF || app_prefs()->muted || walkie_busy()) return;
    xQueueSend(s_q, &id, 0);
}
