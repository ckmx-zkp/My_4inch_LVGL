#pragma once
#include "lvgl.h"

lv_obj_t *page_relay_create(lv_obj_t *parent);
void page_relay_sync(void);   // called from the UI timer to reflect model state
