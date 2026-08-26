#include "app_prefs.h"

#include "app_i18n.h"
#include "bsp_audio.h"
#include "bsp_display.h"
#include "nvs.h"

#include <stdlib.h>
#include <string.h>

static const char *TAG_NS = "app";
static app_prefs_t s_p;
static app_totp_list_t s_totp;

typedef struct {
    char name[33];
    char secret[65];
    uint8_t digits;
    uint8_t period;
} totp_v1_t;

static const char *const NTP[] = {
    "pool.ntp.org",
    "time.apple.com",
    "ntp.aliyun.com",
    "time.windows.com",
};

static void set_defaults(void)
{
    memset(&s_p, 0, sizeof(s_p));
    s_p.brightness = 50;
    s_p.sleep_sec = 30;
    s_p.lock_on = 1;
    s_p.lock_stay = 0;
    s_p.volume = 70;
    s_p.muted = 0;
    s_p.tone_msg = APP_TONE_BEEP;
    s_p.tone_alert = APP_TONE_ALARM;
    s_p.ntp_on = 1;
    s_p.ntp_server = 0;
    s_p.auto_hide = 10;
    s_p.lang = APP_LANG_ZH;
    strlcpy(s_p.wx_city, "Shanghai", sizeof(s_p.wx_city));
    s_p.wx_lat_e4 = 312304;
    s_p.wx_lon_e4 = 1214737;
    s_p.wx_interval = 30;
    s_p.wx_imperial = 0;
    s_p.walkie_ch = 1;
    s_p.walkie_mode = 0;
    s_p.meow_bed = 21;
    s_p.meow_wake = 8;
    s_p.ota_auto = 0;
}

void app_prefs_load(void)
{
    set_defaults();
    nvs_handle_t h;
    if (nvs_open(TAG_NS, NVS_READONLY, &h) != ESP_OK) {
        app_lang_set((app_lang_t)s_p.lang);
        return;
    }
    uint8_t u8;
    uint16_t u16;
    if (nvs_get_u8(h, "bl", &u8) == ESP_OK && u8 >= 10 && u8 <= 100) s_p.brightness = u8;
    if (nvs_get_u16(h, "sleep", &u16) == ESP_OK) s_p.sleep_sec = u16;
    if (nvs_get_u8(h, "lck", &u8) == ESP_OK) s_p.lock_on = u8 != 0;
    if (nvs_get_u8(h, "lstay", &u8) == ESP_OK) s_p.lock_stay = u8 != 0;
    if (nvs_get_u8(h, "vol", &u8) == ESP_OK && u8 <= 100) s_p.volume = u8;
    if (nvs_get_u8(h, "mute", &u8) == ESP_OK) s_p.muted = u8 != 0;
    if (nvs_get_u8(h, "tmsg", &u8) == ESP_OK) s_p.tone_msg = u8;
    if (nvs_get_u8(h, "talert", &u8) == ESP_OK) s_p.tone_alert = u8;
    if (nvs_get_u8(h, "ntp", &u8) == ESP_OK) s_p.ntp_on = u8 != 0;
    if (nvs_get_u8(h, "ntps", &u8) == ESP_OK && u8 < APP_NTP_SERVER_N) s_p.ntp_server = u8;
    if (nvs_get_u8(h, "hide", &u8) == ESP_OK) s_p.auto_hide = u8;
    if (nvs_get_u8(h, "lang", &u8) == ESP_OK && u8 < APP_LANG_N) s_p.lang = u8;
    {
        size_t cn = sizeof(s_p.wx_city);
        if (nvs_get_str(h, "wxc", s_p.wx_city, &cn) != ESP_OK) {
            /* keep default */
        }
        int32_t i32;
        if (nvs_get_i32(h, "wxlat", &i32) == ESP_OK) s_p.wx_lat_e4 = i32;
        if (nvs_get_i32(h, "wxlon", &i32) == ESP_OK) s_p.wx_lon_e4 = i32;
        if (nvs_get_u16(h, "wxiv", &u16) == ESP_OK &&
            (u16 == 15 || u16 == 30 || u16 == 60 || u16 == 180)) {
            s_p.wx_interval = u16;
        }
        if (nvs_get_u8(h, "wxu", &u8) == ESP_OK) s_p.wx_imperial = u8 != 0;
        if (nvs_get_u8(h, "wkch", &u8) == ESP_OK && u8 >= 1 && u8 <= 8) s_p.walkie_ch = u8;
        if (nvs_get_u8(h, "wkmd", &u8) == ESP_OK && u8 <= 1) s_p.walkie_mode = u8;
        if (nvs_get_u8(h, "mbed", &u8) == ESP_OK && u8 <= 23) s_p.meow_bed = u8;
        if (nvs_get_u8(h, "mwake", &u8) == ESP_OK && u8 <= 23) s_p.meow_wake = u8;
        if (nvs_get_u8(h, "otaa", &u8) == ESP_OK) s_p.ota_auto = u8 != 0;
    }
    size_t n = sizeof(s_p.kw);
    if (nvs_get_blob(h, "kw", s_p.kw, &n) == ESP_OK) {
        uint8_t kn = 0;
        nvs_get_u8(h, "kwn", &kn);
        if (kn > APP_KW_MAX) kn = APP_KW_MAX;
        s_p.kw_n = kn;
    }
    app_totp_list_clear(&s_totp);
    {
        uint8_t ver = 0;
        nvs_get_u8(h, "totpv", &ver);
        if (ver == 2) {
            uint16_t kn = 0;
            nvs_get_u16(h, "totpn", &kn);
            size_t tn = 0;
            esp_err_t e = nvs_get_blob(h, "totp", NULL, &tn);
            if (kn && e == ESP_OK && tn >= sizeof(app_totp_acct_t)) {
                app_totp_acct_t *raw = malloc(tn);
                if (raw && nvs_get_blob(h, "totp", raw, &tn) == ESP_OK) {
                    uint16_t rec = (uint16_t)(tn / sizeof(app_totp_acct_t));
                    if (kn < rec) rec = kn;
                    for (uint16_t i = 0; i < rec; i++) {
                        app_totp_list_add(&s_totp, &raw[i]);
                    }
                }
                free(raw);
            }
        } else {
            uint8_t kn = 0;
            nvs_get_u8(h, "totpn", &kn);
            totp_v1_t old[8];
            size_t tn = sizeof(old);
            if (kn && nvs_get_blob(h, "totp", old, &tn) == ESP_OK) {
                uint16_t rec = (uint16_t)(tn / sizeof(totp_v1_t));
                if (kn < rec) rec = kn;
                for (uint16_t i = 0; i < rec; i++) {
                    app_totp_acct_t a;
                    memset(&a, 0, sizeof(a));
                    app_totp_split_name(old[i].name, a.issuer, sizeof(a.issuer),
                                        a.label, sizeof(a.label));
                    if (!a.issuer[0] && old[i].name[0]) {
                        size_t k = 0;
                        while (old[i].name[k] && k + 1 < sizeof(a.issuer)) {
                            a.issuer[k] = old[i].name[k];
                            k++;
                        }
                        a.issuer[k] = 0;
                    }
                    strncpy(a.secret, old[i].secret, sizeof(a.secret) - 1);
                    a.digits = old[i].digits;
                    a.period = old[i].period;
                    app_totp_list_add(&s_totp, &a);
                }
            }
        }
        app_totp_list_sort(&s_totp);
    }
    nvs_close(h);
    app_lang_set((app_lang_t)s_p.lang);
}

static bool totp_write(nvs_handle_t h)
{
    nvs_set_u8(h, "totpv", 2);
    nvs_set_u16(h, "totpn", s_totp.n);
    if (s_totp.n == 0 || !s_totp.items) {
        nvs_erase_key(h, "totp");
        return true;
    }
    size_t bytes = (size_t)s_totp.n * sizeof(app_totp_acct_t);
    return nvs_set_blob(h, "totp", s_totp.items, bytes) == ESP_OK;
}

void app_prefs_save(void)
{
    nvs_handle_t h;
    if (nvs_open(TAG_NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_u8(h, "bl", s_p.brightness);
    nvs_set_u16(h, "sleep", s_p.sleep_sec);
    nvs_set_u8(h, "lck", s_p.lock_on);
    nvs_set_u8(h, "lstay", s_p.lock_stay);
    nvs_set_u8(h, "vol", s_p.volume);
    nvs_set_u8(h, "mute", s_p.muted);
    nvs_set_u8(h, "tmsg", s_p.tone_msg);
    nvs_set_u8(h, "talert", s_p.tone_alert);
    nvs_set_u8(h, "ntp", s_p.ntp_on);
    nvs_set_u8(h, "ntps", s_p.ntp_server);
    nvs_set_u8(h, "hide", s_p.auto_hide);
    nvs_set_u8(h, "lang", s_p.lang);
    nvs_set_str(h, "wxc", s_p.wx_city);
    nvs_set_i32(h, "wxlat", s_p.wx_lat_e4);
    nvs_set_i32(h, "wxlon", s_p.wx_lon_e4);
    nvs_set_u16(h, "wxiv", s_p.wx_interval);
    nvs_set_u8(h, "wxu", s_p.wx_imperial);
    nvs_set_u8(h, "wkch", s_p.walkie_ch);
    nvs_set_u8(h, "wkmd", s_p.walkie_mode);
    nvs_set_u8(h, "mbed", s_p.meow_bed);
    nvs_set_u8(h, "mwake", s_p.meow_wake);
    nvs_set_u8(h, "otaa", s_p.ota_auto ? 1 : 0);
    nvs_set_u8(h, "kwn", s_p.kw_n);
    nvs_set_blob(h, "kw", s_p.kw, sizeof(s_p.kw));
    totp_write(h);
    nvs_commit(h);
    nvs_close(h);
}

void app_prefs_save_lang(void)
{
    nvs_handle_t h;
    if (nvs_open(TAG_NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_u8(h, "lang", s_p.lang);
    nvs_commit(h);
    nvs_close(h);
}

app_prefs_t *app_prefs(void)
{
    return &s_p;
}

app_totp_list_t *app_totp_store(void)
{
    return &s_totp;
}

bool app_totp_persist(void)
{
    nvs_handle_t h;
    if (nvs_open(TAG_NS, NVS_READWRITE, &h) != ESP_OK) return false;
    bool ok = totp_write(h);
    if (ok) nvs_commit(h);
    nvs_close(h);
    return ok;
}

const char *app_ntp_server(int index)
{
    if (index < 0 || index >= APP_NTP_SERVER_N) index = 0;
    return NTP[index];
}

void app_prefs_apply_display(void)
{
    bsp_display_backlight(s_p.brightness);
}

void app_prefs_apply_audio(void)
{
    bsp_audio_set_volume(s_p.muted ? 0 : s_p.volume);
}
