#pragma once

#include "bsp_button.h"
#include <stdbool.h>

// Simulator-only controls used by the SDL input adapter.
void bsp_mock_button_set_pressed(bsp_btn_t btn, bool pressed);
void bsp_mock_button_emit(bsp_btn_t btn, bsp_btn_ev_t ev);
