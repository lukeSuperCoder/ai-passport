#include "app_ui.h"
#include "bsp_audio.h"
#include "bsp_battery.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "bsp_mock.h"
#include "lvgl.h"
#include "src/drivers/sdl/lv_sdl_window.h"
#include <SDL.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define LONG_PRESS_MS 700U

typedef struct {
    SDL_Scancode scan;
    bsp_btn_t button;
    bool down;
    uint32_t down_at;
} sim_key_t;

static sim_key_t s_keys[] = {
    { SDL_SCANCODE_UP, BSP_BTN_UP, false, 0 },
    { SDL_SCANCODE_DOWN, BSP_BTN_DOWN, false, 0 },
    { SDL_SCANCODE_RETURN, BSP_BTN_OK, false, 0 },
};

static void on_button(bsp_btn_t btn, bsp_btn_ev_t ev, void *user)
{
    (void)user;
    if (!bsp_lvgl_lock(500)) return;
    app_ui_handle_key(btn, ev);
    bsp_lvgl_unlock();
}

static void poll_keys(void)
{
    const uint8_t *state = SDL_GetKeyboardState(NULL);
    uint32_t now = SDL_GetTicks();
    for (size_t i = 0; i < sizeof(s_keys) / sizeof(s_keys[0]); i++) {
        sim_key_t *key = &s_keys[i];
        bool down = state[key->scan] != 0;
        if (down && !key->down) {
            key->down = true;
            key->down_at = now;
            bsp_mock_button_set_pressed(key->button, true);
            bsp_mock_button_emit(key->button, BSP_BTN_PRESS);
        } else if (!down && key->down) {
            key->down = false;
            bsp_mock_button_set_pressed(key->button, false);
            bsp_mock_button_emit(key->button,
                now - key->down_at >= LONG_PRESS_MS ? BSP_BTN_LONG : BSP_BTN_CLICK);
        }
    }
}

int main(void)
{
    lv_init();
    lv_display_t *display = lv_sdl_window_create(240, 320);
    if (!display) {
        fprintf(stderr, "Failed to create SDL display\n");
        return 1;
    }
    lv_sdl_window_set_title(display, "FoloToy AI Passport Simulator");
    lv_sdl_window_set_zoom(display, 2.0f);

    bool ok[APP_UI_DEMO_COUNT] = {
        true,
        bsp_button_init(on_button, NULL) == ESP_OK,
        bsp_audio_init() == ESP_OK,
        bsp_battery_init() == ESP_OK,
    };

    if (bsp_lvgl_lock(1000)) {
        app_ui_start(ok);
        bsp_lvgl_unlock();
    }

    puts("Controls: Up/Down arrows, Enter; hold Enter for 700ms to return.");
    while (SDL_WasInit(SDL_INIT_VIDEO) && lv_display_get_default()) {
        if (bsp_lvgl_lock(1000)) {
            lv_timer_handler();
            bsp_lvgl_unlock();
        }
        if (!SDL_WasInit(SDL_INIT_VIDEO) || !lv_display_get_default()) break;
        poll_keys();
        SDL_Delay(5);
    }
    // SDL_QUIT deinitializes LVGL in the driver. A window-close event only
    // deletes the display, so finish the remaining teardown here.
    if (SDL_WasInit(SDL_INIT_VIDEO)) {
        lv_sdl_quit();
        lv_deinit();
    }
    return 0;
}
