#include "nvs.h"

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#define SAVE_MAGIC 0x3156534eu
#define ENTRY_COUNT 64
#define KEY_CAP 16
#define VALUE_CAP 4096

typedef struct {
    char key[KEY_CAP];
    uint32_t size;
    uint8_t value[VALUE_CAP];
} save_entry_t;

typedef struct {
    uint32_t magic;
    save_entry_t entries[ENTRY_COUNT];
} save_file_t;

static save_file_t s_save;
static bool s_loaded;
static const char *s_path = "meow-simulator-save.bin";

static void load_once(void)
{
    if (s_loaded) return;
    s_loaded = true;
    FILE *file = fopen(s_path, "rb");
    if (file) {
        if (fread(&s_save, 1, sizeof(s_save), file) != sizeof(s_save) ||
            s_save.magic != SAVE_MAGIC) {
            memset(&s_save, 0, sizeof(s_save));
        }
        fclose(file);
    }
    s_save.magic = SAVE_MAGIC;
}

static save_entry_t *entry_for(const char *key, bool create)
{
    load_once();
    save_entry_t *empty = NULL;
    for (size_t i = 0; i < ENTRY_COUNT; i++) {
        if (s_save.entries[i].key[0] == '\0' && !empty) empty = &s_save.entries[i];
        if (strncmp(s_save.entries[i].key, key, KEY_CAP) == 0) return &s_save.entries[i];
    }
    if (!create || !empty || strlen(key) >= KEY_CAP) return NULL;
    strcpy(empty->key, key);
    return empty;
}

static esp_err_t set_value(const char *key, const void *value, size_t length)
{
    if (!value || length > VALUE_CAP) return ESP_ERR_INVALID_ARG;
    save_entry_t *entry = entry_for(key, true);
    if (!entry) return ESP_ERR_NO_MEM;
    memcpy(entry->value, value, length);
    entry->size = (uint32_t)length;
    return ESP_OK;
}

static esp_err_t get_value(const char *key, void *value, size_t *length)
{
    if (!length) return ESP_ERR_INVALID_ARG;
    save_entry_t *entry = entry_for(key, false);
    if (!entry) return ESP_ERR_NOT_FOUND;
    if (!value) {
        *length = entry->size;
        return ESP_OK;
    }
    if (*length < entry->size) return ESP_ERR_NO_MEM;
    memcpy(value, entry->value, entry->size);
    *length = entry->size;
    return ESP_OK;
}

esp_err_t nvs_open(const char *name, nvs_open_mode_t mode, nvs_handle_t *handle)
{
    (void)name;
    (void)mode;
    load_once();
    if (!handle) return ESP_ERR_INVALID_ARG;
    *handle = 1;
    return ESP_OK;
}

void nvs_close(nvs_handle_t handle) { (void)handle; }

esp_err_t nvs_commit(nvs_handle_t handle)
{
    (void)handle;
    FILE *file = fopen(s_path, "wb");
    if (!file) return ESP_FAIL;
    size_t written = fwrite(&s_save, 1, sizeof(s_save), file);
    fclose(file);
    return written == sizeof(s_save) ? ESP_OK : ESP_FAIL;
}

esp_err_t nvs_set_blob(nvs_handle_t h, const char *k, const void *v, size_t n) { (void)h; return set_value(k, v, n); }
esp_err_t nvs_get_blob(nvs_handle_t h, const char *k, void *v, size_t *n) { (void)h; return get_value(k, v, n); }
#define NVS_SCALAR(type, suffix) \
    esp_err_t nvs_set_##suffix(nvs_handle_t h, const char *k, type v) { (void)h; return set_value(k, &v, sizeof(v)); } \
    esp_err_t nvs_get_##suffix(nvs_handle_t h, const char *k, type *v) { size_t n = sizeof(*v); (void)h; return get_value(k, v, &n); }
NVS_SCALAR(uint8_t, u8)
NVS_SCALAR(uint16_t, u16)
NVS_SCALAR(int32_t, i32)

esp_err_t nvs_set_str(nvs_handle_t h, const char *k, const char *v) { (void)h; return set_value(k, v, strlen(v) + 1); }
esp_err_t nvs_get_str(nvs_handle_t h, const char *k, char *v, size_t *n) { (void)h; return get_value(k, v, n); }

esp_err_t nvs_erase_key(nvs_handle_t handle, const char *key)
{
    (void)handle;
    save_entry_t *entry = entry_for(key, false);
    if (!entry) return ESP_ERR_NOT_FOUND;
    memset(entry, 0, sizeof(*entry));
    return ESP_OK;
}
