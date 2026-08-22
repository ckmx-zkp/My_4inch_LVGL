#include "mqtt_home.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "mqtt_client.h"
#include "cJSON.h"
#include "home_model.h"
#include "relay.h"

static const char *TAG = "mqtt_home";

#define DEV_ID     CONFIG_MQTT_DEVICE_ID   // "hallway"
#define SW_ID      CONFIG_MQTT_SWITCH_ID   // "main"
#define CLIENT_ID  CONFIG_MQTT_CLIENT_ID   // "board-hallway"

#define TOPIC_BRIDGE  "home/v1/bridge/state"
#define TOPIC_DEVICES "home/v1/devices"
#define TOPIC_ALL_STATE "home/v1/+/state"
#define TOPIC_SCENES  "home/v1/scenes"
#define TOPIC_SCENE_RESULT "home/v1/scenes/+/result"

static esp_mqtt_client_handle_t s_client;
static volatile bool s_connected = false;
static int64_t s_local_override_us;

static char s_status_topic[64];

// ---- helpers --------------------------------------------------------------

// Parse a switch "on" JSON value: true/false/null, "ON"/"OFF", 1/0. -> 1/0/-1.
static int8_t parse_on(const cJSON *v)
{
    if (!v || cJSON_IsNull(v)) return -1;
    if (cJSON_IsBool(v)) return cJSON_IsTrue(v) ? 1 : 0;
    if (cJSON_IsNumber(v)) return v->valuedouble != 0 ? 1 : 0;
    if (cJSON_IsString(v)) {
        const char *s = v->valuestring;
        if (!strcasecmp(s, "on") || !strcmp(s, "1") || !strcasecmp(s, "true")) return 1;
        return 0;
    }
    if (cJSON_IsObject(v)) {   // switches.{id} = { "on": ..., "label": ... }
        const cJSON *on = cJSON_GetObjectItem(v, "on");
        return parse_on(on);
    }
    return -1;
}

// Handle home/v1/{deviceId}/state payloads (protocol §5).
static void handle_state(const char *dev_id, const char *data, int len)
{
    cJSON *root = cJSON_ParseWithLength(data, len);
    if (!root) return;

    const cJSON *name = cJSON_GetObjectItem(root, "name");
    const cJSON *switches = cJSON_GetObjectItem(root, "switches");

    hm_lock();
    hm_device_t *d = hm_get_device(dev_id);
    if (d) {
        if (name && cJSON_IsString(name)) strncpy(d->name, name->valuestring, HM_NAME_LEN - 1);
        if (switches && cJSON_IsObject(switches)) {
            const cJSON *sw = NULL;
            cJSON_ArrayForEach(sw, switches) {
                int8_t on = parse_on(sw);
                const cJSON *lbl = cJSON_IsObject(sw) ? cJSON_GetObjectItem(sw, "label") : NULL;
                hm_set_switch(dev_id, sw->string, on,
                              (lbl && cJSON_IsString(lbl)) ? lbl->valuestring : NULL);
            }
        }
        // presence sensor
        const cJSON *sensors = cJSON_GetObjectItem(root, "sensors");
        const cJSON *presence = sensors ? cJSON_GetObjectItem(sensors, "presence") : NULL;
        const cJSON *pval = presence ? cJSON_GetObjectItem(presence, "value") : NULL;
        if (pval) d->presence = cJSON_IsTrue(pval) ? 1 : (cJSON_IsFalse(pval) ? 0 : -1);
        hm_bump();
    }
    hm_unlock();

    // Own device: scenes/remote may drive the GPIO, but a recent local
    // toggle wins so reconnect retain cannot undo an offline change.
    if (strcmp(dev_id, DEV_ID) == 0 && switches && cJSON_IsObject(switches)) {
        const cJSON *mine = cJSON_GetObjectItem(switches, SW_ID);
        int8_t on = parse_on(mine);
        bool recent_local = (esp_timer_get_time() - s_local_override_us) < 3000000LL;
        if (on >= 0 && !recent_local) relay_set(RELAY_HALLWAY, on == 1);
    }

    cJSON_Delete(root);
}

static void handle_bridge(const char *data, int len)
{
    cJSON *root = cJSON_ParseWithLength(data, len);
    if (!root) return;
    const cJSON *online = cJSON_GetObjectItem(root, "online");
    bool was = hm_bridge_online();
    bool now = online && cJSON_IsTrue(online);
    hm_set_bridge_online(now);
    cJSON_Delete(root);
    // When the gateway comes back, push local GPIO so the catalog follows us.
    if (now && !was) mqtt_home_publish_hallway(relay_get(RELAY_HALLWAY));
}

static void handle_scenes(const char *data, int len)
{
    cJSON *root = cJSON_ParseWithLength(data, len);
    if (!root) {
        ESP_LOGE(TAG, "scenes JSON parse fail (%d bytes)", len);
        return;
    }
    const cJSON *arr = cJSON_GetObjectItem(root, "scenes");
    hm_scene_t tmp[HM_MAX_SCENES];
    memset(tmp, 0, sizeof(tmp));
    int n = 0;
    if (arr && cJSON_IsArray(arr)) {
        const cJSON *it = NULL;
        cJSON_ArrayForEach(it, arr) {
            if (n >= HM_MAX_SCENES) break;
            const cJSON *id = cJSON_GetObjectItem(it, "id");
            const cJSON *name = cJSON_GetObjectItem(it, "name");
            if (!id || !cJSON_IsString(id) || !id->valuestring[0]) continue;
            strncpy(tmp[n].id, id->valuestring, HM_SCENE_ID_LEN - 1);
            if (name && cJSON_IsString(name)) {
                strncpy(tmp[n].name, name->valuestring, HM_SCENE_NAME_LEN - 1);
            }
            tmp[n].valid = true;
            tmp[n].last_ok = -1;
            n++;
        }
    } else {
        ESP_LOGW(TAG, "scenes payload has no scenes[] (%d bytes)", len);
    }
    if (n > 0) {
        hm_set_scenes(tmp, n);
        ESP_LOGI(TAG, "scenes catalog: %d", n);
    } else {
        ESP_LOGW(TAG, "scenes catalog empty, keeping current list");
    }
    cJSON_Delete(root);
}

// home/v1/scenes/{sceneId}/result
static bool topic_scene_result(const char *topic, int tlen, char *out, int out_sz)
{
    const char *prefix = "home/v1/scenes/";
    const char *suffix = "/result";
    int plen = (int)strlen(prefix), slen = (int)strlen(suffix);
    if (tlen <= plen + slen) return false;
    if (strncmp(topic, prefix, plen) != 0) return false;
    if (strncmp(topic + tlen - slen, suffix, slen) != 0) return false;
    int idlen = tlen - plen - slen;
    if (idlen <= 0 || idlen >= out_sz) return false;
    memcpy(out, topic + plen, idlen);
    out[idlen] = '\0';
    return true;
}

static void handle_scene_result(const char *scene_id, const char *data, int len)
{
    cJSON *root = cJSON_ParseWithLength(data, len);
    if (!root) return;
    const cJSON *ok = cJSON_GetObjectItem(root, "ok");
    const cJSON *err = cJSON_GetObjectItem(root, "error");
    bool good = ok && cJSON_IsTrue(ok);
    const char *emsg = (err && cJSON_IsString(err)) ? err->valuestring : NULL;
    hm_set_scene_result(scene_id, good, emsg);
    ESP_LOGI(TAG, "scene %s result ok=%d", scene_id, good);
    cJSON_Delete(root);
}

// Extract {deviceId} from "home/v1/{deviceId}/state". Returns false if not that shape.
static bool topic_device_state(const char *topic, int tlen, char *out, int out_sz)
{
    const char *prefix = "home/v1/";
    const char *suffix = "/state";
    int plen = strlen(prefix), slen = strlen(suffix);
    if (tlen <= plen + slen) return false;
    if (strncmp(topic, prefix, plen) != 0) return false;
    if (strncmp(topic + tlen - slen, suffix, slen) != 0) return false;
    int idlen = tlen - plen - slen;
    if (idlen <= 0 || idlen >= out_sz) return false;
    memcpy(out, topic + plen, idlen);
    out[idlen] = '\0';
    if (strcmp(out, "bridge") == 0) return false;   // bridge handled separately
    return true;
}

// ---- publishing -----------------------------------------------------------

void mqtt_home_note_local_override(void)
{
    s_local_override_us = esp_timer_get_time();
}

void mqtt_home_publish_set(const char *device_id, const char *switch_id, bool on)
{
    if (!s_client || !s_connected) return;
    if (!hm_bridge_online()) {
        ESP_LOGW(TAG, "bridge offline, not sending set");
        return;
    }
    char topic[64];
    char payload[48];
    snprintf(topic, sizeof(topic), "home/v1/%s/set", device_id);
    snprintf(payload, sizeof(payload), "{\"%s\":%s}", switch_id, on ? "true" : "false");
    esp_mqtt_client_publish(s_client, topic, payload, 0, 1, 0);
    ESP_LOGI(TAG, "PUB %s %s", topic, payload);
}

void mqtt_home_publish_hallway(bool on)
{
    mqtt_home_note_local_override();
    mqtt_home_publish_set(DEV_ID, SW_ID, on);
}

void mqtt_home_apply_scene(const char *scene_id)
{
    if (!scene_id || !scene_id[0]) return;
    if (!s_client || !s_connected) {
        ESP_LOGW(TAG, "MQTT down, not applying scene");
        return;
    }
    if (!hm_bridge_online()) {
        ESP_LOGW(TAG, "bridge offline, not applying scene");
        return;
    }
    char topic[80];
    snprintf(topic, sizeof(topic), "home/v1/scenes/%s/apply", scene_id);
    hm_mark_scene_pending(scene_id);
    esp_mqtt_client_publish(s_client, topic, "{}", 2, 1, 0);
    ESP_LOGI(TAG, "PUB %s {}", topic);
}

bool mqtt_home_connected(void) { return s_connected; }

// ---- event handler --------------------------------------------------------

static void publish_online_status(void)
{
    char payload[64];
    snprintf(payload, sizeof(payload), "{\"online\":true,\"id\":\"%s\"}", CLIENT_ID);
    esp_mqtt_client_publish(s_client, s_status_topic, payload, 0, 1, 1 /*retain*/);
}

static void mqtt_event_handler(void *args, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT connected");
        s_connected = true;
        publish_online_status();
        mqtt_home_note_local_override();
        esp_mqtt_client_subscribe(s_client, TOPIC_BRIDGE, 1);
        esp_mqtt_client_subscribe(s_client, TOPIC_DEVICES, 1);
        esp_mqtt_client_subscribe(s_client, TOPIC_ALL_STATE, 1);
        esp_mqtt_client_subscribe(s_client, TOPIC_SCENES, 1);
        esp_mqtt_client_subscribe(s_client, TOPIC_SCENE_RESULT, 1);
        esp_mqtt_client_publish(s_client, "home/v1/scenes/get", "{}", 2, 1, 0);
        // Push local GPIO so retained hallway state cannot undo an offline toggle.
        mqtt_home_publish_hallway(relay_get(RELAY_HALLWAY));
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "MQTT disconnected");
        s_connected = false;
        break;
    case MQTT_EVENT_DATA: {
        // Retained catalogs (especially scenes with actions[]) can exceed the
        // MQTT chunk size. Reassemble before parsing.
        static char topic_buf[96];
        static char *acc;
        static int acc_cap, acc_total;

        int off = event->current_data_offset;
        int chunk = event->data_len;
        int total = event->total_data_len;
        if (off == 0) {
            int n = event->topic_len;
            if (n >= (int)sizeof(topic_buf)) n = (int)sizeof(topic_buf) - 1;
            memcpy(topic_buf, event->topic, n);
            topic_buf[n] = '\0';
            if (acc && acc_cap < total + 1) {
                free(acc);
                acc = NULL;
                acc_cap = 0;
            }
            if (!acc) {
                acc_cap = total + 1;
                if (acc_cap < 64) acc_cap = 64;
                acc = malloc(acc_cap);
            }
            acc_total = total;
        }
        if (!acc || off + chunk > acc_total) {
            ESP_LOGW(TAG, "MQTT drop %s off=%d chunk=%d total=%d", topic_buf, off, chunk, total);
            break;
        }
        memcpy(acc + off, event->data, chunk);
        if (off + chunk < acc_total) break;
        acc[acc_total] = '\0';
        ESP_LOGI(TAG, "MQTT %s (%d bytes)", topic_buf, acc_total);

        int tlen = (int)strlen(topic_buf);
        if (tlen == (int)strlen(TOPIC_BRIDGE) && strcmp(topic_buf, TOPIC_BRIDGE) == 0) {
            handle_bridge(acc, acc_total);
            break;
        }
        if (tlen == (int)strlen(TOPIC_SCENES) && strcmp(topic_buf, TOPIC_SCENES) == 0) {
            handle_scenes(acc, acc_total);
            break;
        }
        char scene_id[HM_SCENE_ID_LEN];
        if (topic_scene_result(topic_buf, tlen, scene_id, sizeof(scene_id))) {
            handle_scene_result(scene_id, acc, acc_total);
            break;
        }
        char dev[HM_ID_LEN];
        if (topic_device_state(topic_buf, tlen, dev, sizeof(dev))) {
            handle_state(dev, acc, acc_total);
        }
        break;
    }
    default:
        break;
    }
}

void mqtt_home_start(void)
{
    snprintf(s_status_topic, sizeof(s_status_topic),
             "home/v1/clients/%s/status", CLIENT_ID);

    static const char lwt_msg[] = "{\"online\":false}";

    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = CONFIG_MQTT_BROKER_URI,
        .credentials.username = CONFIG_MQTT_USERNAME,
        .credentials.authentication.password = CONFIG_MQTT_PASSWORD,
        .credentials.client_id = CLIENT_ID,
        .session.keepalive = 45,
        .session.last_will.topic = s_status_topic,
        .session.last_will.msg = lwt_msg,
        .session.last_will.msg_len = sizeof(lwt_msg) - 1,
        .session.last_will.qos = 1,
        .session.last_will.retain = 1,
        .buffer.size = 8192,
        .buffer.out_size = 2048,
    };

    s_client = esp_mqtt_client_init(&cfg);
    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(s_client);
    ESP_LOGI(TAG, "MQTT starting -> %s", CONFIG_MQTT_BROKER_URI);
}
