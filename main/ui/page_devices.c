#include "page_devices.h"
#include <string.h>
#include <stdio.h>
#include "sdkconfig.h"
#include "home_model.h"
#include "mqtt_home.h"
#include "fonts/font_cn.h"
#include "ui_theme.h"

// One row per (device,switch). We rebuild the list content lazily: a fixed
// pool of row widgets is created up front and shown/hidden as the model
// grows, which keeps this simple without dynamic LVGL object churn on a
// timer tick.
#define MAX_ROWS (HM_MAX_DEVICES * HM_MAX_SWITCHES)

typedef struct {
    lv_obj_t *row;
    lv_obj_t *label;
    lv_obj_t *sw;
    char dev_id[HM_ID_LEN];
    char sw_id[HM_ID_LEN];
    bool updating;
} row_t;

static row_t s_rows[MAX_ROWS];
static lv_obj_t *s_list;
static int s_row_count = 0;

static void row_switch_cb(lv_event_t *e)
{
    row_t *r = lv_event_get_user_data(e);
    if (r->updating) return;
    bool on = lv_obj_has_state(r->sw, LV_STATE_CHECKED);
    mqtt_home_publish_set(r->dev_id, r->sw_id, on);
}

lv_obj_t *page_devices_create(lv_obj_t *parent)
{
    ui_theme_page(parent);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    s_list = parent;

    for (int i = 0; i < MAX_ROWS; i++) {
        row_t *r = &s_rows[i];
        r->row = lv_obj_create(parent);
        lv_obj_set_size(r->row, LV_PCT(100), 64);
        ui_theme_card(r->row);
        lv_obj_set_flex_flow(r->row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(r->row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        r->label = lv_label_create(r->row);
        lv_label_set_text(r->label, "");
        lv_obj_set_style_text_font(r->label, &font_cn_22, 0);
        lv_obj_set_style_text_color(r->label, ui_col_text(), 0);

        r->sw = lv_switch_create(r->row);
        lv_obj_set_size(r->sw, 72, 40);
        ui_theme_switch(r->sw);
        lv_obj_add_event_cb(r->sw, row_switch_cb, LV_EVENT_VALUE_CHANGED, r);

        lv_obj_add_flag(r->row, LV_OBJ_FLAG_HIDDEN);
    }
    return parent;
}

void page_devices_sync(void)
{
    int idx = 0;

    hm_lock();
    for (int di = 0; di < HM_MAX_DEVICES && idx < MAX_ROWS; di++) {
        hm_device_t *d = hm_device_at(di);
        if (!d) continue;
        // Skip our own hallway/main row here; it already lives on page 1.
        for (int si = 0; si < d->nswitch && idx < MAX_ROWS; si++) {
            if (!strcmp(d->id, CONFIG_MQTT_DEVICE_ID) &&
                !strcmp(d->sw[si].id, CONFIG_MQTT_SWITCH_ID)) {
                continue;
            }
            row_t *r = &s_rows[idx];
            strncpy(r->dev_id, d->id, HM_ID_LEN - 1);
            strncpy(r->sw_id, d->sw[si].id, HM_ID_LEN - 1);

            char text[HM_NAME_LEN + HM_LABEL_LEN + 4];
            const char *name = d->name[0] ? d->name : d->id;
            const char *label = d->sw[si].label[0] ? d->sw[si].label : d->sw[si].id;
            snprintf(text, sizeof(text), "%s / %s", name, label);
            lv_label_set_text(r->label, text);

            r->updating = true;
            if (d->sw[si].on == 1) lv_obj_add_state(r->sw, LV_STATE_CHECKED);
            else lv_obj_clear_state(r->sw, LV_STATE_CHECKED);
            r->updating = false;

            lv_obj_clear_flag(r->row, LV_OBJ_FLAG_HIDDEN);
            idx++;
        }
    }
    hm_unlock();

    for (; idx < MAX_ROWS; idx++) {
        lv_obj_add_flag(s_rows[idx].row, LV_OBJ_FLAG_HIDDEN);
    }
    (void)s_row_count;
}
