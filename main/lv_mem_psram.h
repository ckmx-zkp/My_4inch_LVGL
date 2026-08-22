#pragma once
#include <stddef.h>

// One-shot PSRAM pool for LVGL tlsf (see LV_MEM_POOL_ALLOC).
void *lv_psram_pool_alloc(size_t n);
