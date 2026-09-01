#include "telemetry.h"

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"

static const char *TAG = "memory";

void telemetry_log_memory(const char *checkpoint)
{
    ESP_LOGI(TAG,
             "%s free=%u min=%u largest=%u dma_free=%u dma_largest=%u",
             checkpoint ? checkpoint : "unknown",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
             (unsigned)esp_get_minimum_free_heap_size(),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_DMA),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_DMA));
}

