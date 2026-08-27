#include "app.h"
#include "app_i18n.h"
#include "app_prefs.h"
#include "app_time.h"
#include "app_ui.h"
#include "bsp_display.h"
#include "app_tone.h"
#include "ui_pixel.h"

#include <stdio.h>

/* ---------- Date & Time ---------- */

static lv_obj_t *s_clk_body;
static lv_obj_t *s_clk_hint;
static lv_timer_t *s_clk_timer;
static int s_clk_sel;
static int s_y, s_mo, s_d, s_h, s_mi;

static int clk_rows(void)
{
    return app_prefs()->ntp_on ? 2 : 7;
}

static void clk_paint(void)
{
    if (!s_clk_body) return;
    char now[24];
    app_time_now_text(now, sizeof(now));
    const app_prefs_t *p = app_prefs();
    char buf[420];
    if (s_clk_sel >= clk_rows()) s_clk_sel = clk_rows() - 1;
    if (p->ntp_on) {
        snprintf(buf, sizeof(buf),
                 "%s %s\n"
                 "%s %s  %s%s\n"
                 "%s %s  %s\n",
                 app_str(APP_STR_NOW), now,
                 s_clk_sel == 0 ? ">" : " ", app_str(APP_STR_NTP),
                 app_str_onoff(p->ntp_on),
                 app_time_ntp_synced() ? app_str(APP_STR_SYNC) : "",
                 s_clk_sel == 1 ? ">" : " ", app_str(APP_STR_SERVER),
                 app_ntp_server(p->ntp_server));
    } else {
        snprintf(buf, sizeof(buf),
                 "%s %s\n"
                 "%s %s  %s\n"
                 "%s %s  %d\n"
                 "%s %s  %d\n"
                 "%s %s  %d\n"
                 "%s %s  %d\n"
                 "%s %s  %02d\n"
                 "%s %s\n",
                 app_str(APP_STR_NOW), now,
                 s_clk_sel == 0 ? ">" : " ", app_str(APP_STR_NTP),
                 app_str_onoff(p->ntp_on),
                 s_clk_sel == 1 ? ">" : " ", app_str(APP_STR_YEAR), s_y,
                 s_clk_sel == 2 ? ">" : " ", app_str(APP_STR_MONTH), s_mo,
                 s_clk_sel == 3 ? ">" : " ", app_str(APP_STR_DAY), s_d,
                 s_clk_sel == 4 ? ">" : " ", app_str(APP_STR_HOUR), s_h,
                 s_clk_sel == 5 ? ">" : " ", app_str(APP_STR_MINUTE), s_mi,
                 s_clk_sel == 6 ? ">" : " ", app_str(APP_STR_SET_CLOCK));
    }
    lv_label_set_text(s_clk_body, buf);
    if (s_clk_hint) {
        lv_label_set_text(s_clk_hint,
                          app_str(p->ntp_on ? APP_STR_CLOCK_HINT : APP_STR_CLOCK_SET_HINT));
    }
}

static void clk_tick(lv_timer_t *t)
{
    (void)t;
    clk_paint();
}

void app_clock_enter(lv_obj_t *p)
{
    app_time_get(&s_y, &s_mo, &s_d, &s_h, &s_mi);
    if (s_y < 2024) s_y = 2026;
    s_clk_sel = 0;
    lv_obj_t *card = app_ui_card(p);
    app_ui_title(card, app_str(APP_STR_DATETIME));
    s_clk_hint = app_ui_hint(card);
    s_clk_body = app_ui_body(card, 44);
    s_clk_timer = lv_timer_create(clk_tick, 500, NULL);
    clk_paint();
}

void app_clock_exit(void)
{
    if (s_clk_timer) { lv_timer_delete(s_clk_timer); s_clk_timer = NULL; }
    s_clk_body = NULL;
    s_clk_hint = NULL;
}

void app_clock_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    if (ev != BSP_BTN_CLICK) return;
    if (btn == BSP_BTN_UP) { app_ui_move(&s_clk_sel, clk_rows(), -1); clk_paint(); return; }
    if (btn == BSP_BTN_DOWN) { app_ui_move(&s_clk_sel, clk_rows(), 1); clk_paint(); return; }
    if (btn != BSP_BTN_OK) return;
    app_prefs_t *p = app_prefs();
    if (s_clk_sel == 0) {
        p->ntp_on = !p->ntp_on;
        app_prefs_save();
        app_time_ntp_restart();
        s_clk_sel = 0;
    } else if (p->ntp_on) {
        if (s_clk_sel == 1) {
            p->ntp_server = (uint8_t)((p->ntp_server + 1) % APP_NTP_SERVER_N);
            app_prefs_save();
            app_time_ntp_restart();
        }
    } else {
        switch (s_clk_sel) {
        case 1: s_y++; if (s_y > 2038) s_y = 2024; break;
        case 2: s_mo = s_mo >= 12 ? 1 : s_mo + 1; break;
        case 3: s_d = s_d >= 31 ? 1 : s_d + 1; break;
        case 4: s_h = (s_h + 1) % 24; break;
        case 5: s_mi = (s_mi + 1) % 60; break;
        case 6: app_time_set(s_y, s_mo, s_d, s_h, s_mi); break;
        default: break;
        }
    }
    clk_paint();
}

/* ---------- Screen ---------- */

static lv_obj_t *s_scr_body;
static int s_scr_sel;

static const uint16_t SLEEP_OPTS[] = { 0, 15, 30, 60, 120 };
#define SLEEP_N 5

static const char *sleep_text(uint16_t s)
{
    if (s == 0) return app_str(APP_STR_NEVER);
    static char buf[12];
    snprintf(buf, sizeof(buf), "%ds", (int)s);
    return buf;
}

static int sleep_idx(uint16_t s)
{
    for (int i = 0; i < SLEEP_N; i++) if (SLEEP_OPTS[i] == s) return i;
    return 2;
}

static void scr_paint(void)
{
    if (!s_scr_body) return;
    app_prefs_t *p = app_prefs();
    char buf[280];
    snprintf(buf, sizeof(buf),
             "%s %s  %d%%\n"
             "%s %s  %s\n"
             "%s %s  %s\n"
             "%s %s  %s\n",
             s_scr_sel == 0 ? ">" : " ", app_str(APP_STR_BRIGHTNESS),
             (int)p->brightness,
             s_scr_sel == 1 ? ">" : " ", app_str(APP_STR_SLEEP),
             sleep_text(p->sleep_sec),
             s_scr_sel == 2 ? ">" : " ", app_str(APP_STR_LOCK),
             app_str_onoff(p->lock_on),
             s_scr_sel == 3 ? ">" : " ", app_str(APP_STR_LOCK_STAY),
             app_str_onoff(p->lock_stay));
    lv_label_set_text(s_scr_body, buf);
}

void app_screen_enter(lv_obj_t *p)
{
    s_scr_sel = 0;
    lv_obj_t *card = app_ui_card(p);
    app_ui_title(card, app_str(APP_STR_SCREEN));
    lv_obj_t *h = app_ui_hint(card);
    lv_label_set_text(h, app_str(APP_STR_SCREEN_HINT));
    s_scr_body = app_ui_body(card, 48);
    scr_paint();
}

void app_screen_exit(void)
{
    s_scr_body = NULL;
}

void app_screen_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    if (ev != BSP_BTN_CLICK) return;
    if (btn == BSP_BTN_UP) { app_ui_move(&s_scr_sel, 4, -1); scr_paint(); return; }
    if (btn == BSP_BTN_DOWN) { app_ui_move(&s_scr_sel, 4, 1); scr_paint(); return; }
    if (btn != BSP_BTN_OK) return;
    app_prefs_t *p = app_prefs();
    if (s_scr_sel == 0) {
        int b = (int)p->brightness + 10;
        if (b > 100) b = 10;
        p->brightness = (uint8_t)b;
        bsp_display_backlight(p->brightness);
    } else if (s_scr_sel == 1) {
        int i = sleep_idx(p->sleep_sec) + 1;
        if (i >= SLEEP_N) i = 0;
        p->sleep_sec = SLEEP_OPTS[i];
    } else if (s_scr_sel == 2) {
        p->lock_on = !p->lock_on;
        if (!p->lock_on) app_lock_hide();
    } else {
        p->lock_stay = !p->lock_stay;
    }
    app_prefs_save();
    scr_paint();
}

/* ---------- Sound ---------- */

static lv_obj_t *s_snd_body;
static int s_snd_sel;

static const char *tone_name(uint8_t id)
{
    switch (id) {
    case APP_TONE_OFF:    return app_str(APP_STR_TONE_OFF);
    case APP_TONE_BEEP:   return app_str(APP_STR_TONE_BEEP);
    case APP_TONE_DOUBLE: return app_str(APP_STR_TONE_DOUBLE);
    case APP_TONE_CHIME:  return app_str(APP_STR_TONE_CHIME);
    case APP_TONE_TRIPLE: return app_str(APP_STR_TONE_TRIPLE);
    case APP_TONE_ALARM:  return app_str(APP_STR_TONE_ALARM);
    default:              return "?";
    }
}

static uint8_t cycle_tone(const uint8_t *seq, int n, uint8_t id, int dir)
{
    int i = 0;
    while (i < n && seq[i] != id) i++;
    if (i == n) i = 0;
    i += dir;
    if (i < 0) i = n - 1;
    if (i >= n) i = 0;
    return seq[i];
}

static uint8_t cycle_msg_tone(uint8_t id, int dir)
{
    static const uint8_t seq[] = { APP_TONE_OFF, APP_TONE_BEEP, APP_TONE_DOUBLE, APP_TONE_CHIME };
    return cycle_tone(seq, 4, id, dir);
}

static uint8_t cycle_alert_tone(uint8_t id, int dir)
{
    static const uint8_t seq[] = { APP_TONE_OFF, APP_TONE_TRIPLE, APP_TONE_ALARM };
    return cycle_tone(seq, 3, id, dir);
}

static void snd_paint(void)
{
    if (!s_snd_body) return;
    app_prefs_t *p = app_prefs();
    char buf[240];
    snprintf(buf, sizeof(buf),
             "%s %s  %s\n"
             "%s %s  %d%%\n"
             "%s %s  %s\n"
             "%s %s  %s\n",
             s_snd_sel == 0 ? ">" : " ", app_str(APP_STR_MUTE),
             app_str_onoff(p->muted),
             s_snd_sel == 1 ? ">" : " ", app_str(APP_STR_VOLUME),
             (int)p->volume,
             s_snd_sel == 2 ? ">" : " ", app_str(APP_STR_MESSAGE),
             tone_name(p->tone_msg),
             s_snd_sel == 3 ? ">" : " ", app_str(APP_STR_ALERT_TONE),
             tone_name(p->tone_alert));
    lv_label_set_text(s_snd_body, buf);
}

void app_sound_enter(lv_obj_t *p)
{
    s_snd_sel = 0;
    lv_obj_t *card = app_ui_card(p);
    app_ui_title(card, app_str(APP_STR_SOUND));
    lv_obj_t *h = app_ui_hint(card);
    lv_label_set_text(h, app_str(APP_STR_SOUND_HINT));
    s_snd_body = app_ui_body(card, 48);
    snd_paint();
}

void app_sound_exit(void)
{
    s_snd_body = NULL;
}

void app_sound_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    if (ev != BSP_BTN_CLICK) return;
    if (btn == BSP_BTN_UP) { app_ui_move(&s_snd_sel, 4, -1); snd_paint(); return; }
    if (btn == BSP_BTN_DOWN) { app_ui_move(&s_snd_sel, 4, 1); snd_paint(); return; }
    if (btn != BSP_BTN_OK) return;
    app_prefs_t *p = app_prefs();
    if (s_snd_sel == 0) {
        p->muted = !p->muted;
        app_prefs_save();
        app_prefs_apply_audio();
        if (!p->muted) app_tone_play(APP_TONE_BEEP);
    } else if (s_snd_sel == 1) {
        int v = (int)p->volume + 10;
        if (v > 100) v = 0;
        p->volume = (uint8_t)v;
        app_prefs_save();
        app_prefs_apply_audio();
        app_tone_play(APP_TONE_BEEP);
    } else if (s_snd_sel == 2) {
        p->tone_msg = cycle_msg_tone(p->tone_msg, 1);
        app_prefs_save();
        app_tone_play(p->tone_msg);
    } else {
        p->tone_alert = cycle_alert_tone(p->tone_alert, 1);
        app_prefs_save();
        app_tone_play(p->tone_alert);
    }
    snd_paint();
}
