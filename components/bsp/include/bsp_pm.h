// 自动 Light Sleep + DFS。息屏时打开,保 BLE 连接;Deep Sleep 会掉射频,不用。
#pragma once

#include "esp_err.h"
#include <stdbool.h>

// 配 DFS。默认不进浅睡,由 bsp_pm_set_sleeping() 在息屏时打开。幂等。
esp_err_t bsp_pm_init(void);

// true:允许 tickless Light Sleep(CPU 40MHz 下限)。false:只保留 DFS,方便 UI。
void bsp_pm_set_sleeping(bool on);
