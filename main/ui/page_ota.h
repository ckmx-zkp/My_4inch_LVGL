#pragma once
#include <stdbool.h>
#include "lvgl.h"

lv_obj_t *page_ota_create(lv_obj_t *parent);
void page_ota_sync(void);
bool page_ota_active(void);
