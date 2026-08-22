#include "page_scenes.h"
#include <string.h>
#include <stdio.h>
#include "home_model.h"
#include "mqtt_home.h"
#include "fonts/font_cn.h"

#define MAX_BTNS HM_MAX_SCENES

typedef struct {
    lv_obj_t *btn;
    lv_obj_t *label;
    char id[HM_SCENE_ID_LEN];
} scene_btn_t;

static scene_btn_t s_btn[MAX_BTNS];
static lv_obj_t *s_hint;
static lv_obj_t *s_status;

static void scene_click_cb(lv_event_t *e)
{
    scene_btn_t *b = lv_event_get_user_data(e);
    if (!b || !b->id[0]) return;
    if (!mqtt_home_connected() || !hm_bridge_online()) {
        lv_label_set_text(s_status, "MQTT: 未连接");
        return;
    }
    mqtt_home_apply_scene(b->id);
    lv_label_set_text(s_status, "等待");
}

lv_obj_t *page_scenes_create(lv_obj_t *parent)
{
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(parent, 8, 0);

    s_hint = lv_label_create(parent);
    lv_label_set_text(s_hint, "场景");
    lv_obj_set_style_text_font(s_hint, &font_cn_28, 0);

    for (int i = 0; i < MAX_BTNS; i++) {
        s_btn[i].btn = lv_btn_create(parent);
        lv_obj_set_size(s_btn[i].btn, LV_PCT(90), 56);
        lv_obj_add_event_cb(s_btn[i].btn, scene_click_cb, LV_EVENT_CLICKED, &s_btn[i]);
        s_btn[i].label = lv_label_create(s_btn[i].btn);
        lv_label_set_text(s_btn[i].label, "");
        lv_obj_set_style_text_font(s_btn[i].label, &font_cn_22, 0);
        lv_obj_center(s_btn[i].label);
        lv_obj_add_flag(s_btn[i].btn, LV_OBJ_FLAG_HIDDEN);
        s_btn[i].id[0] = '\0';
    }

    s_status = lv_label_create(parent);
    lv_label_set_text(s_status, "MQTT: 未连接");
    lv_obj_set_style_text_font(s_status, &font_cn_22, 0);

    return parent;
}

void page_scenes_sync(void)
{
    bool net_ok = mqtt_home_connected() && hm_bridge_online();
    int shown = 0;
    char status[96] = {0};

    hm_lock();
    for (int i = 0; i < MAX_BTNS; i++) {
        hm_scene_t *s = hm_scene_at(i);
        if (!s) {
            lv_obj_add_flag(s_btn[i].btn, LV_OBJ_FLAG_HIDDEN);
            s_btn[i].id[0] = '\0';
            continue;
        }
        strncpy(s_btn[i].id, s->id, HM_SCENE_ID_LEN - 1);
        lv_label_set_text(s_btn[i].label, s->name[0] ? s->name : s->id);
        lv_obj_clear_flag(s_btn[i].btn, LV_OBJ_FLAG_HIDDEN);
        if (net_ok) lv_obj_clear_state(s_btn[i].btn, LV_STATE_DISABLED);
        else lv_obj_add_state(s_btn[i].btn, LV_STATE_DISABLED);
        shown++;
        if (s->last_ok == 1) {
            snprintf(status, sizeof(status), "%s: 成功", s->name[0] ? s->name : s->id);
        } else if (s->last_ok == 0) {
            snprintf(status, sizeof(status), "%s: 失败", s->name[0] ? s->name : s->id);
        } else if (s->last_error[0]) {
            snprintf(status, sizeof(status), "%s: %s", s->name[0] ? s->name : s->id, s->last_error);
        }
    }
    hm_unlock();

    if (!mqtt_home_connected()) {
        lv_label_set_text(s_status, "MQTT: 未连接");
    } else if (!hm_bridge_online()) {
        lv_label_set_text(s_status, "MQTT: 已连接(网关离线)");
    } else if (shown == 0) {
        lv_label_set_text(s_status, "等待场景目录");
    } else if (status[0]) {
        lv_label_set_text(s_status, status);
    } else {
        lv_label_set_text(s_status, "MQTT: 已连接");
    }
}
