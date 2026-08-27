#pragma once

#include "FreeRTOS.h"

typedef struct sim_task *TaskHandle_t;
typedef void (*TaskFunction_t)(void *);

int xTaskCreate(TaskFunction_t task, const char *name, uint32_t stack_depth,
                void *arg, unsigned priority, TaskHandle_t *handle);
void vTaskDelete(TaskHandle_t task);
void vTaskDelay(TickType_t ticks);
