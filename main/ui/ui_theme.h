#pragma once
#include "lvgl.h"

lv_color_t ui_col_bg(void);
lv_color_t ui_col_card(void);
lv_color_t ui_col_gold(void);
lv_color_t ui_col_gold_dim(void);
lv_color_t ui_col_text(void);
lv_color_t ui_col_muted(void);

void ui_theme_init(lv_disp_t *disp);

void ui_theme_page(lv_obj_t *page);
void ui_theme_title(lv_obj_t *label);
void ui_theme_muted(lv_obj_t *label);
void ui_theme_switch(lv_obj_t *sw);
void ui_theme_card(lv_obj_t *obj);
void ui_theme_scene_btn(lv_obj_t *btn);
void ui_theme_scene_btn_accent(lv_obj_t *btn);
void ui_theme_tabview(lv_obj_t *tv);
