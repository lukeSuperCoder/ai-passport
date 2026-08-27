#include "freertos/task.h"
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

struct sim_task {
    pthread_t thread;
    TaskFunction_t fn;
    void *arg;
};

static void *task_entry(void *arg)
{
    struct sim_task *task = arg;
    task->fn(task->arg);
    return NULL;
}

int xTaskCreate(TaskFunction_t fn, const char *name, uint32_t stack_depth,
                void *arg, unsigned priority, TaskHandle_t *handle)
{
    (void)name;
    (void)stack_depth;
    (void)priority;
    struct sim_task *task = calloc(1, sizeof(*task));
    if (!task) return 0;
    task->fn = fn;
    task->arg = arg;
    if (pthread_create(&task->thread, NULL, task_entry, task) != 0) {
        free(task);
        return 0;
    }
    *handle = task;
    return 1;
}

void vTaskDelete(TaskHandle_t task)
{
    if (!task) pthread_exit(NULL);
    pthread_cancel(task->thread);
    pthread_join(task->thread, NULL);
    free(task);
}

void vTaskDelay(TickType_t ticks)
{
    usleep((useconds_t)ticks * 1000U);
}
