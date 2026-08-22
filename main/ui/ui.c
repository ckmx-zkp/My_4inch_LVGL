#include "ui.h"
#include "page_relay.h"
#include "page_scenes.h"
#include "page_devices.h"
#include "fonts/font_cn.h"

static void sync_timer_cb(lv_timer_t *t)
{
    (void)t;
    page_relay_sync();
    page_scenes_sync();
    page_devices_sync();
}

void ui_init(void)
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_text_font(scr, &font_cn_22, 0);

    lv_obj_t *tv = lv_tabview_create(scr, LV_DIR_TOP, 52);
    lv_obj_set_style_text_font(tv, &font_cn_22, 0);
    lv_obj_t *btns = lv_tabview_get_tab_btns(tv);
    lv_obj_set_style_text_font(btns, &font_cn_22, 0);

    lv_obj_t *tab_relay = lv_tabview_add_tab(tv, "过道灯");
    lv_obj_t *tab_scenes = lv_tabview_add_tab(tv, "场景");
    lv_obj_t *tab_devices = lv_tabview_add_tab(tv, "全屋设备");

    page_relay_create(tab_relay);
    page_scenes_create(tab_scenes);
    page_devices_create(tab_devices);

    lv_timer_create(sync_timer_cb, 300, NULL);
}
