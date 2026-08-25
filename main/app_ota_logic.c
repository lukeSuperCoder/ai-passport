#include "app_ota_logic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void skip_ws(const char **p)
{
    const char *s = *p;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    *p = s;
}

static const char *after_key(const char *json, const char *key)
{
    char pat[24];
    const char *p;
    int n;

    n = snprintf(pat, sizeof(pat), "\"%s\"", key);
    if (n < 0 || n >= (int)sizeof(pat)) return NULL;
    p = strstr(json, pat);
    if (!p) return NULL;
    p += (size_t)n;
    skip_ws(&p);
    if (*p != ':') return NULL;
    p++;
    skip_ws(&p);
    return p;
}

static bool copy_quoted(const char *p, char *out, size_t n)
{
    size_t i = 0;

    if (!p || *p != '"') return false;
    p++;
    while (*p && *p != '"') {
        char c = *p++;
        if (c == '\\' && *p) c = *p++;
        if (i + 1 < n) out[i++] = c;
    }
    if (*p != '"') return false;
    out[i] = 0;
    return true;
}

bool app_ota_parse_ver(const char *s, app_ota_ver_t *out)
{
    int a = 0, b = 0, c = 0;
    char extra = 0;
    int n;

    if (!s || !out) return false;
    if (*s == 'v' || *s == 'V') s++;
    if (*s < '0' || *s > '9') return false;
    n = sscanf(s, "%d.%d.%d%c", &a, &b, &c, &extra);
    if (n < 3) {
        extra = 0;
        n = sscanf(s, "%d.%d%c", &a, &b, &extra);
        if (n < 2) return false;
        c = 0;
    }
    if (a < 0 || b < 0 || c < 0) return false;
    if (n > 2 && extra && extra != '-' && extra != '+' && extra != '.') return false;
    out->major = a;
    out->minor = b;
    out->patch = c;
    return true;
}

int app_ota_cmp_ver(const app_ota_ver_t *a, const app_ota_ver_t *b)
{
    if (!a || !b) return 0;
    if (a->major != b->major) return a->major < b->major ? -1 : 1;
    if (a->minor != b->minor) return a->minor < b->minor ? -1 : 1;
    if (a->patch != b->patch) return a->patch < b->patch ? -1 : 1;
    return 0;
}

bool app_ota_is_newer(const char *cur, const char *next)
{
    app_ota_ver_t a, b;

    if (!app_ota_parse_ver(cur, &a) || !app_ota_parse_ver(next, &b)) return false;
    return app_ota_cmp_ver(&b, &a) > 0;
}

bool app_ota_url_ok(const char *url)
{
    if (!url || strncmp(url, "https://", 8) != 0 || url[8] == 0) return false;
    /* 工厂整片不能写进 OTA 槽。 */
    if (strstr(url, "-factory.bin")) return false;
    return true;
}

bool app_ota_sha_match(const char *hex, const uint8_t digest[32])
{
    static const char *const H = "0123456789abcdef";
    int i;

    if (!hex || !digest || strlen(hex) != APP_OTA_SHA_HEX) return false;
    for (i = 0; i < 32; i++) {
        unsigned char hi, lo, b;
        const char *p;
        char c0 = hex[i * 2], c1 = hex[i * 2 + 1];

        if (c0 >= 'A' && c0 <= 'F') c0 = (char)(c0 - 'A' + 'a');
        if (c1 >= 'A' && c1 <= 'F') c1 = (char)(c1 - 'A' + 'a');
        p = strchr(H, c0);
        if (!p) return false;
        hi = (unsigned char)(p - H);
        p = strchr(H, c1);
        if (!p) return false;
        lo = (unsigned char)(p - H);
        b = (unsigned char)((hi << 4) | lo);
        if (b != digest[i]) return false;
    }
    return true;
}

bool app_ota_parse_manifest(const char *json, app_ota_manifest_t *out)
{
    const char *p;
    unsigned long size = 0;

    if (!json || !out) return false;
    memset(out, 0, sizeof(*out));

    p = after_key(json, "channel");
    if (!copy_quoted(p, out->channel, sizeof(out->channel))) return false;

    p = after_key(json, "version");
    if (!copy_quoted(p, out->version, sizeof(out->version))) return false;
    {
        app_ota_ver_t v;
        if (!app_ota_parse_ver(out->version, &v)) return false;
    }

    p = after_key(json, "ota_url");
    if (!p || !copy_quoted(p, out->url, sizeof(out->url))) {
        p = after_key(json, "url");
        if (!copy_quoted(p, out->url, sizeof(out->url))) return false;
    }
    if (!app_ota_url_ok(out->url)) return false;

    p = after_key(json, "ota_sha256");
    if (!p || !copy_quoted(p, out->sha256, sizeof(out->sha256))) {
        p = after_key(json, "sha256");
        if (!copy_quoted(p, out->sha256, sizeof(out->sha256))) return false;
    }
    if (strlen(out->sha256) != APP_OTA_SHA_HEX) return false;

    p = after_key(json, "ota_size");
    if (!p || *p < '0' || *p > '9') p = after_key(json, "size");
    if (p && *p >= '0' && *p <= '9') {
        size = strtoul(p, NULL, 10);
        if (size > 0x3F0000u) return false;
        out->size = (uint32_t)size;
    }
    return true;
}

bool app_ota_channel_ok(const char *got, const char *want)
{
    return got && want && got[0] && want[0] && strcmp(got, want) == 0;
}
