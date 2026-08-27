#include "esp_event.h"
#include "esp_timer.h"

#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

struct sim_esp_timer {
    esp_timer_cb_t callback;
    void *arg;
    pthread_mutex_t mutex;
    uint64_t generation;
};

typedef struct {
    esp_timer_handle_t timer;
    uint64_t generation;
    uint64_t timeout_us;
} timer_run_t;

typedef struct {
    esp_event_base_t base;
    int32_t id;
    esp_event_handler_t handler;
    void *arg;
} event_slot_t;

static event_slot_t s_events[16];
static size_t s_event_count;

int64_t esp_timer_get_time(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000LL + ts.tv_nsec / 1000;
}

static void *timer_entry(void *opaque)
{
    timer_run_t *run = opaque;
    usleep((useconds_t)run->timeout_us);
    pthread_mutex_lock(&run->timer->mutex);
    bool fire = run->timer->generation == run->generation;
    esp_timer_cb_t callback = run->timer->callback;
    void *arg = run->timer->arg;
    pthread_mutex_unlock(&run->timer->mutex);
    if (fire && callback) callback(arg);
    free(run);
    return NULL;
}

esp_err_t esp_timer_create(const esp_timer_create_args_t *args,
                           esp_timer_handle_t *out_handle)
{
    if (!args || !args->callback || !out_handle) return ESP_ERR_INVALID_ARG;
    esp_timer_handle_t timer = calloc(1, sizeof(*timer));
    if (!timer) return ESP_ERR_NO_MEM;
    timer->callback = args->callback;
    timer->arg = args->arg;
    pthread_mutex_init(&timer->mutex, NULL);
    *out_handle = timer;
    return ESP_OK;
}

esp_err_t esp_timer_start_once(esp_timer_handle_t timer, uint64_t timeout_us)
{
    if (!timer) return ESP_ERR_INVALID_ARG;
    timer_run_t *run = calloc(1, sizeof(*run));
    if (!run) return ESP_ERR_NO_MEM;
    pthread_mutex_lock(&timer->mutex);
    run->generation = ++timer->generation;
    pthread_mutex_unlock(&timer->mutex);
    run->timer = timer;
    run->timeout_us = timeout_us;
    pthread_t thread;
    if (pthread_create(&thread, NULL, timer_entry, run) != 0) {
        free(run);
        return ESP_FAIL;
    }
    pthread_detach(thread);
    return ESP_OK;
}

esp_err_t esp_timer_stop(esp_timer_handle_t timer)
{
    if (!timer) return ESP_ERR_INVALID_ARG;
    pthread_mutex_lock(&timer->mutex);
    timer->generation++;
    pthread_mutex_unlock(&timer->mutex);
    return ESP_OK;
}

esp_err_t esp_event_handler_register(esp_event_base_t base, int32_t id,
                                     esp_event_handler_t handler, void *arg)
{
    if (!handler || s_event_count >= sizeof(s_events) / sizeof(s_events[0])) {
        return ESP_ERR_NO_MEM;
    }
    s_events[s_event_count++] = (event_slot_t){ base, id, handler, arg };
    return ESP_OK;
}

esp_err_t esp_event_post(esp_event_base_t base, int32_t id, const void *data,
                         size_t data_size, uint32_t ticks_to_wait)
{
    (void)data_size;
    (void)ticks_to_wait;
    for (size_t i = 0; i < s_event_count; i++) {
        if (s_events[i].base == base && s_events[i].id == id) {
            s_events[i].handler(s_events[i].arg, base, id, (void *)data);
        }
    }
    return ESP_OK;
}
