#pragma once
#include "lvgl.h"

// Initialize backlight, ST7701 RGB panel (480x480) and LVGL display driver.
// Returns the registered LVGL display.
lv_disp_t *board_display_init(void);

// Periodic diagnostic: flush Y range, heap, LVGL mem.
void board_display_dump_stats(void);
