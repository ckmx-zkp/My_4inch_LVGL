#include "page_ota.h"
#include <stdio.h>
#include <string.h>
#include "esp_app_desc.h"
#include "ota_https.h"
#include "fonts/font_cn.h"
#include "ui_theme.h"

static lv_obj_t *s_bar;
static lv_obj_t *s_pct;
static lv_obj_t *s_status;
static int s_shown = -2;

lv_obj_t *page_ota_create(lv_obj_t *parent)
{
    ui_theme_page(parent);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, "OTA");
    ui_theme_title(title);

    const esp_app_desc_t *app = esp_app_get_description();
    lv_obj_t *ver = lv_label_create(parent);
    lv_label_set_text_fmt(ver, "v%s", app && app->version[0] ? app->version : "?");
    ui_theme_title(ver);

    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, LV_PCT(92), 88);
    ui_theme_card(card);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(card, 8, 0);

    s_bar = lv_bar_create(card);
    lv_obj_set_size(s_bar, LV_PCT(100), 22);
    lv_bar_set_range(s_bar, 0, 100);
    lv_bar_set_value(s_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_bar, lv_color_hex(0xE0D6C8), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(s_bar, 11, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_bar, ui_col_gold(), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s_bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(s_bar, 11, LV_PART_INDICATOR);

    s_pct = lv_label_create(parent);
    lv_label_set_text(s_pct, "0%");
    ui_theme_title(s_pct);

    s_status = lv_label_create(parent);
    lv_label_set_text(s_status, "空闲");
    ui_theme_muted(s_status);

    return parent;
}

bool page_ota_active(void)
{
    return ota_https_progress() >= 0;
}

void page_ota_sync(void)
{
    int p = ota_https_progress();
    const char *err = ota_https_error();
    if (p == s_shown) return;
    s_shown = p;

    if (p < 0) {
        lv_bar_set_value(s_bar, 0, LV_ANIM_OFF);
        lv_label_set_text(s_pct, "0%");
        lv_label_set_text(s_status, (err && err[0]) ? "失败" : "空闲");
        return;
    }
    if (p >= 101) {
        lv_bar_set_value(s_bar, 100, LV_ANIM_OFF);
        lv_label_set_text(s_pct, "100%");
        lv_label_set_text(s_status, "重启");
        return;
    }
    lv_bar_set_value(s_bar, p, LV_ANIM_OFF);
    char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", p);
    lv_label_set_text(s_pct, buf);
    lv_label_set_text(s_status, "OTA");
}
