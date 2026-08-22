#include "ui_theme.h"
#include "fonts/font_cn.h"

#define HEX(c) lv_color_hex(c)

lv_color_t ui_col_bg(void)       { return HEX(0xF3EBE0); }
lv_color_t ui_col_card(void)     { return HEX(0xFAF6F0); }
lv_color_t ui_col_gold(void)     { return HEX(0xE4B04A); }
lv_color_t ui_col_gold_dim(void) { return HEX(0x8B7355); }
lv_color_t ui_col_text(void)     { return HEX(0x3D2E22); }
lv_color_t ui_col_muted(void)    { return HEX(0xB5A898); }

void ui_theme_init(lv_disp_t *disp)
{
    // Color the screen only. Do not call lv_theme_default_init() with the
    // CJK subset font — that left the panel at backlight-only after reboot.
    lv_disp_set_bg_color(disp, ui_col_bg());
}

void ui_theme_page(lv_obj_t *page)
{
    lv_obj_set_style_bg_color(page, ui_col_bg(), 0);
    lv_obj_set_style_bg_opa(page, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(page, 0, 0);
    lv_obj_set_style_pad_all(page, 12, 0);
    lv_obj_set_style_pad_row(page, 10, 0);
    lv_obj_set_scrollbar_mode(page, LV_SCROLLBAR_MODE_OFF);
}

void ui_theme_title(lv_obj_t *label)
{
    lv_obj_set_style_text_color(label, ui_col_text(), 0);
    lv_obj_set_style_text_font(label, &font_cn_28, 0);
}

void ui_theme_muted(lv_obj_t *label)
{
    lv_obj_set_style_text_color(label, ui_col_muted(), 0);
    lv_obj_set_style_text_font(label, &font_cn_22, 0);
}

void ui_theme_switch(lv_obj_t *sw)
{
    lv_obj_set_style_radius(sw, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_radius(sw, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
    lv_obj_set_style_radius(sw, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    lv_obj_set_style_bg_color(sw, HEX(0xE0D6C8), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(sw, 0, LV_PART_MAIN);

    lv_obj_set_style_bg_color(sw, ui_col_gold(), LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, LV_PART_INDICATOR | LV_STATE_CHECKED);

    lv_obj_set_style_bg_color(sw, HEX(0xC4B8A8), LV_PART_KNOB);
    lv_obj_set_style_bg_color(sw, HEX(0xFFFFFF), LV_PART_KNOB | LV_STATE_CHECKED);
    lv_obj_set_style_border_width(sw, 0, LV_PART_KNOB);
}

void ui_theme_card(lv_obj_t *obj)
{
    lv_obj_set_style_bg_color(obj, ui_col_card(), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(obj, 8, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_hor(obj, 16, 0);
    lv_obj_set_style_pad_ver(obj, 10, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

void ui_theme_scene_btn(lv_obj_t *btn)
{
    ui_theme_card(btn);
    lv_obj_set_style_bg_color(btn, ui_col_card(), 0);
    lv_obj_set_style_bg_color(btn, HEX(0xEDE4D4), LV_STATE_PRESSED);
    lv_obj_set_style_text_color(btn, ui_col_text(), 0);
}

void ui_theme_scene_btn_accent(lv_obj_t *btn)
{
    ui_theme_card(btn);
    lv_obj_set_style_bg_color(btn, ui_col_gold(), 0);
    lv_obj_set_style_bg_color(btn, HEX(0xD49A30), LV_STATE_PRESSED);
    lv_obj_set_style_text_color(btn, ui_col_text(), 0);
}

void ui_theme_tabview(lv_obj_t *tv)
{
    lv_obj_set_style_bg_color(tv, ui_col_bg(), 0);
    lv_obj_set_style_bg_opa(tv, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(tv, 0, 0);
    lv_obj_set_style_text_font(tv, &font_cn_22, 0);

    lv_obj_t *btns = lv_tabview_get_tab_btns(tv);
    lv_obj_set_style_bg_color(btns, HEX(0xEDE6DA), 0);
    lv_obj_set_style_bg_opa(btns, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(btns, 0, 0);
    lv_obj_set_style_border_width(btns, 0, 0);
    lv_obj_set_style_text_font(btns, &font_cn_22, 0);

    lv_obj_set_style_text_color(btns, ui_col_gold_dim(), LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(btns, LV_OPA_TRANSP, LV_PART_ITEMS);
    lv_obj_set_style_border_width(btns, 0, LV_PART_ITEMS);

    lv_obj_set_style_text_color(btns, ui_col_text(), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(btns, ui_col_card(), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_bg_opa(btns, LV_OPA_COVER, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_border_width(btns, 3, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_border_side(btns, LV_BORDER_SIDE_BOTTOM, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_border_color(btns, ui_col_gold(), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_border_opa(btns, LV_OPA_COVER, LV_PART_ITEMS | LV_STATE_CHECKED);

    lv_obj_t *content = lv_tabview_get_content(tv);
    lv_obj_set_style_bg_color(content, ui_col_bg(), 0);
    lv_obj_set_style_bg_opa(content, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(content, 0, 0);
}
