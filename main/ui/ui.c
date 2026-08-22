#include "ui.h"
#include <stdio.h>
#include "page_relay.h"
#include "page_scenes.h"
#include "page_devices.h"
#include "page_ota.h"
#include "fonts/font_cn.h"
#include "ui_theme.h"
#include "esp_log.h"

static const char *TAG = "ui";
static lv_obj_t *s_tabview;
static int s_ota_switched;

#define TAB_OTA 3

static void sync_timer_cb(lv_timer_t *t)
{
    (void)t;
    page_relay_sync();
    page_scenes_sync();
    page_devices_sync();
    page_ota_sync();
    if (page_ota_active()) {
        if (!s_ota_switched && s_tabview) {
            lv_tabview_set_act(s_tabview, TAB_OTA, LV_ANIM_OFF);
            s_ota_switched = 1;
        }
    } else {
        s_ota_switched = 0;
    }
}

void ui_init(void)
{
    lv_disp_t *disp = lv_disp_get_default();
    ui_theme_init(disp);

    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, ui_col_bg(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(scr, ui_col_text(), 0);
    lv_obj_set_style_text_font(scr, &font_cn_22, 0);

    s_tabview = lv_tabview_create(scr, LV_DIR_TOP, 56);
    lv_obj_set_size(s_tabview, LV_PCT(100), LV_PCT(100));
    ui_theme_tabview(s_tabview);

    lv_obj_t *tab_relay = lv_tabview_add_tab(s_tabview, "过道灯");
    lv_obj_t *tab_scenes = lv_tabview_add_tab(s_tabview, "场景");
    lv_obj_t *tab_devices = lv_tabview_add_tab(s_tabview, "全屋设备");
    lv_obj_t *tab_ota = lv_tabview_add_tab(s_tabview, "OTA");

    page_relay_create(tab_relay);
    page_scenes_create(tab_scenes);
    page_devices_create(tab_devices);
    page_ota_create(tab_ota);

    lv_timer_create(sync_timer_cb, 300, NULL);
    ui_debug_dump();
}

static void dump_obj(const char *name, lv_obj_t *o)
{
    if (!o) {
        ESP_LOGW(TAG, "%s: null", name);
        return;
    }
    lv_area_t a;
    lv_obj_get_coords(o, &a);
    ESP_LOGI(TAG, "%s %dx%d coords (%d,%d)-(%d,%d) child=%d",
             name, (int)lv_obj_get_width(o), (int)lv_obj_get_height(o),
             (int)a.x1, (int)a.y1, (int)a.x2, (int)a.y2,
             (int)lv_obj_get_child_cnt(o));
}

void ui_debug_dump(void)
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_update_layout(scr);
    lv_disp_t *d = lv_disp_get_default();
    ESP_LOGI(TAG, "disp res %dx%d scr %p",
             (int)lv_disp_get_hor_res(d), (int)lv_disp_get_ver_res(d), (void *)scr);
    dump_obj("scr", scr);
    dump_obj("tabview", s_tabview);
    if (s_tabview) {
        dump_obj("tab_btns", lv_tabview_get_tab_btns(s_tabview));
        dump_obj("tab_cont", lv_tabview_get_content(s_tabview));
        lv_obj_t *cont = lv_tabview_get_content(s_tabview);
        if (cont) {
            for (int i = 0; i < (int)lv_obj_get_child_cnt(cont); i++) {
                char n[16];
                snprintf(n, sizeof(n), "tab%d", i);
                dump_obj(n, lv_obj_get_child(cont, i));
            }
        }
    }
    page_relay_debug_dump();
}
