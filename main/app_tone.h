#pragma once

#include <stdbool.h>

void app_tone_start(void);
void app_tone_play(int id);
/* 短方波,给节奏游戏用。codec 已由 app_tone_gate(true) 打开时不再反复开关。 */
void app_tone_note(int hz, int ms);
void app_tone_gate(bool on);
