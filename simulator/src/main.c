#include "app_i18n.h"
#include "app_meow_ui.h"
#include "app_prefs.h"
#include "ui_pixel.h"
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
    app_meow_on_key(btn, ev);
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
            bsp_mock_button_emit(key->button, BSP_BTN_RELEASE);
            bsp_mock_button_emit(key->button,
                now - key->down_at >= LONG_PRESS_MS ? BSP_BTN_LONG : BSP_BTN_CLICK);
        }
    }
}

int main(void)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "Failed to initialize SDL: %s\n", SDL_GetError());
        return 1;
    }
    lv_init();
    lv_display_t *display = lv_sdl_window_create(240, 320);
    if (!display) {
        fprintf(stderr, "Failed to create SDL display\n");
        SDL_Quit();
        return 1;
    }
    lv_sdl_window_set_title(display, "FoloToy AI Passport Simulator");
    lv_sdl_window_set_zoom(display, 2.0f);

    bsp_button_init(on_button, NULL);
    bsp_audio_init();
    bsp_battery_init();
    app_prefs_load();
    ui_pixel_fonts_init();

    if (bsp_lvgl_lock(1000)) {
        app_meow_start();
        bsp_lvgl_unlock();
    }

    puts("Meow controls: Up/Down arrows, Enter; hold a key for 700ms for long press.");
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
