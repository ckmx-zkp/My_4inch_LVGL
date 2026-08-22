#pragma once
#include "lvgl.h"

// Initialize GT911 touch over I2C and register an LVGL input device.
void board_touch_init(lv_disp_t *disp);
