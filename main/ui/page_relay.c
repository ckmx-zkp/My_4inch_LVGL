#include "page_relay.h"
#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "relay.h"
#include "home_model.h"
#include "mqtt_home.h"
#include "wifi.h"
#include "fonts/font_cn.h"

static lv_obj_t *s_sw;
static lv_obj_t *s_label_state;
static lv_obj_t *s_label_wifi;
static lv_obj_t *s_label_conn;
static bool s_updating_from_model = false;   // guard to avoid feedback loop

static void switch_event_cb(lv_event_t *e)
{
    if (s_updating_from_model) return;
    lv_obj_t *sw = lv_event_get_target(e);
    bool on = lv_obj_has_state(sw, LV_STATE_CHECKED);

    // Core function: GPIO first, network never required.
    relay_set(RELAY_HALLWAY, on);
    mqtt_home_note_local_override();
    mqtt_home_publish_hallway(on);          // best-effort; no-op if MQTT is down
    hm_set_switch(CONFIG_MQTT_DEVICE_ID, CONFIG_MQTT_SWITCH_ID, on ? 1 : 0, NULL);
}

lv_obj_t *page_relay_create(lv_obj_t *parent)
{
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, "过道灯");
    lv_obj_set_style_text_font(title, &font_cn_28, 0);

    s_sw = lv_switch_create(parent);
    lv_obj_set_size(s_sw, 160, 80);
    lv_obj_set_style_pad_all(s_sw, 6, 0);
    lv_obj_add_event_cb(s_sw, switch_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    s_label_state = lv_label_create(parent);
    lv_label_set_text(s_label_state, "关");
    lv_obj_set_style_text_font(s_label_state, &font_cn_28, 0);

    s_label_wifi = lv_label_create(parent);
    lv_label_set_text(s_label_wifi, "WiFi: 未连接");
    lv_obj_set_style_text_font(s_label_wifi, &font_cn_22, 0);

    s_label_conn = lv_label_create(parent);
    lv_label_set_text(s_label_conn, "MQTT: 未连接");
    lv_obj_set_style_text_font(s_label_conn, &font_cn_22, 0);

    return parent;
}

void page_relay_sync(void)
{
    // This page always follows the onboard GPIO, never MQTT catalog.
    bool on = relay_get(RELAY_HALLWAY);

    s_updating_from_model = true;
    if (on) lv_obj_add_state(s_sw, LV_STATE_CHECKED);
    else lv_obj_clear_state(s_sw, LV_STATE_CHECKED);
    s_updating_from_model = false;

    lv_label_set_text(s_label_state, on ? "开" : "关");

    char wifi_txt[48];
    if (wifi_sta_connected()) {
        snprintf(wifi_txt, sizeof(wifi_txt), "WiFi: 已连接 %ddBm", wifi_sta_rssi());
    } else {
        snprintf(wifi_txt, sizeof(wifi_txt), "WiFi: 未连接");
    }
    lv_label_set_text(s_label_wifi, wifi_txt);

    lv_label_set_text(s_label_conn, mqtt_home_connected() ?
                       (hm_bridge_online() ? "MQTT: 已连接" : "MQTT: 已连接(网关离线)") :
                       "MQTT: 未连接");
}
