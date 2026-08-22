#include "lv_mem_psram.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

static const char *TAG = "lv_mem";

void *lv_psram_pool_alloc(size_t n)
{
    void *p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!p) {
        ESP_LOGE(TAG, "PSRAM pool alloc %u failed", (unsigned)n);
        return NULL;
    }
    ESP_LOGI(TAG, "LVGL pool %u bytes in PSRAM at %p", (unsigned)n, p);
    return p;
}
