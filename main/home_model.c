#include "home_model.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static hm_device_t s_dev[HM_MAX_DEVICES];
static hm_scene_t s_scene[HM_MAX_SCENES];
static bool s_bridge_online = false;
static volatile uint32_t s_version = 1;
static SemaphoreHandle_t s_mtx;

void hm_init(void)
{
    memset(s_dev, 0, sizeof(s_dev));
    memset(s_scene, 0, sizeof(s_scene));
    s_mtx = xSemaphoreCreateRecursiveMutex();
    hm_seed_default_scenes();
}

void hm_seed_default_scenes(void)
{
    static const hm_scene_t fallback[] = {
        { .valid = true, .id = "914d8a96-f4ca-43ed-ab8e-37e7aa440a37", .name = "全开", .last_ok = -1 },
        { .valid = true, .id = "3f70f6b3-95aa-4181-a6ba-fc402085ab49", .name = "全关", .last_ok = -1 },
        { .valid = true, .id = "8fccb5da-3bca-40d9-a03e-dd895f739299", .name = "上厕所", .last_ok = -1 },
        { .valid = true, .id = "e194a2c2-4cf7-42c4-957c-8c4fba595059", .name = "回家", .last_ok = -1 },
    };
    hm_set_scenes(fallback, sizeof(fallback) / sizeof(fallback[0]));
}

void hm_lock(void)   { xSemaphoreTakeRecursive(s_mtx, portMAX_DELAY); }
void hm_unlock(void) { xSemaphoreGiveRecursive(s_mtx); }

uint32_t hm_version(void) { return s_version; }
void hm_bump(void) { s_version++; }

void hm_set_bridge_online(bool online)
{
    hm_lock();
    if (s_bridge_online != online) { s_bridge_online = online; hm_bump(); }
    hm_unlock();
}

bool hm_bridge_online(void)
{
    hm_lock();
    bool v = s_bridge_online;
    hm_unlock();
    return v;
}

hm_device_t *hm_find_device(const char *id)
{
    for (int i = 0; i < HM_MAX_DEVICES; i++) {
        if (s_dev[i].valid && strcmp(s_dev[i].id, id) == 0) return &s_dev[i];
    }
    return NULL;
}

hm_device_t *hm_get_device(const char *id)
{
    hm_device_t *d = hm_find_device(id);
    if (d) return d;
    for (int i = 0; i < HM_MAX_DEVICES; i++) {
        if (!s_dev[i].valid) {
            memset(&s_dev[i], 0, sizeof(s_dev[i]));
            s_dev[i].valid = true;
            s_dev[i].presence = -1;
            strncpy(s_dev[i].id, id, HM_ID_LEN - 1);
            return &s_dev[i];
        }
    }
    return NULL;
}

hm_device_t *hm_device_at(int index)
{
    if (index < 0 || index >= HM_MAX_DEVICES) return NULL;
    return s_dev[index].valid ? &s_dev[index] : NULL;
}

static hm_switch_t *find_switch(hm_device_t *d, const char *sw_id)
{
    for (int i = 0; i < d->nswitch; i++) {
        if (strcmp(d->sw[i].id, sw_id) == 0) return &d->sw[i];
    }
    if (d->nswitch < HM_MAX_SWITCHES) {
        hm_switch_t *s = &d->sw[d->nswitch++];
        memset(s, 0, sizeof(*s));
        strncpy(s->id, sw_id, HM_ID_LEN - 1);
        s->on = -1;
        return s;
    }
    return NULL;
}

void hm_set_switch(const char *dev_id, const char *sw_id, int8_t on, const char *label)
{
    hm_lock();
    hm_device_t *d = hm_get_device(dev_id);
    if (d) {
        hm_switch_t *s = find_switch(d, sw_id);
        if (s) {
            s->on = on;
            if (label && label[0]) strncpy(s->label, label, HM_LABEL_LEN - 1);
            hm_bump();
        }
    }
    hm_unlock();
}

void hm_set_scenes(const hm_scene_t *src, int count)
{
    if (!src || count < 0) return;
    if (count > HM_MAX_SCENES) count = HM_MAX_SCENES;
    hm_lock();
    memset(s_scene, 0, sizeof(s_scene));
    for (int i = 0; i < count; i++) {
        s_scene[i] = src[i];
        s_scene[i].valid = true;
        s_scene[i].last_ok = -1;
        s_scene[i].last_error[0] = '\0';
    }
    hm_bump();
    hm_unlock();
}

int hm_scene_count(void)
{
    int n = 0;
    hm_lock();
    for (int i = 0; i < HM_MAX_SCENES; i++) {
        if (s_scene[i].valid) n++;
    }
    hm_unlock();
    return n;
}

hm_scene_t *hm_scene_at(int index)
{
    if (index < 0 || index >= HM_MAX_SCENES) return NULL;
    return s_scene[index].valid ? &s_scene[index] : NULL;
}

hm_scene_t *hm_find_scene(const char *id)
{
    if (!id) return NULL;
    for (int i = 0; i < HM_MAX_SCENES; i++) {
        if (s_scene[i].valid && strcmp(s_scene[i].id, id) == 0) return &s_scene[i];
    }
    return NULL;
}

void hm_mark_scene_pending(const char *id)
{
    hm_lock();
    hm_scene_t *s = hm_find_scene(id);
    if (s) {
        s->last_ok = -1;
        strncpy(s->last_error, "等待", sizeof(s->last_error) - 1);
        hm_bump();
    }
    hm_unlock();
}

void hm_set_scene_result(const char *id, bool ok, const char *error)
{
    hm_lock();
    hm_scene_t *s = hm_find_scene(id);
    if (s) {
        s->last_ok = ok ? 1 : 0;
        if (!ok && error && error[0]) {
            strncpy(s->last_error, error, sizeof(s->last_error) - 1);
        } else {
            s->last_error[0] = '\0';
        }
        hm_bump();
    }
    hm_unlock();
}
