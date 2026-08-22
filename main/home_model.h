#pragma once
#include <stdbool.h>
#include <stdint.h>

// Shared in-memory model of home-mqtt/v1 state.
// Written by the MQTT task, read by the LVGL UI timer. Guarded by a mutex;
// a version counter lets the UI cheaply detect changes.

#define HM_MAX_DEVICES   8
#define HM_MAX_SWITCHES  3
#define HM_MAX_SCENES    8
#define HM_ID_LEN        24
#define HM_NAME_LEN      40
#define HM_LABEL_LEN     24
#define HM_SCENE_ID_LEN  40   // UUID is 36 chars
#define HM_SCENE_NAME_LEN 40

typedef struct {
    char id[HM_ID_LEN];
    char label[HM_LABEL_LEN];
    int8_t on;          // 1=on, 0=off, -1=unknown
} hm_switch_t;

typedef struct {
    char id[HM_ID_LEN];
    char name[HM_NAME_LEN];
    bool valid;
    int nswitch;
    hm_switch_t sw[HM_MAX_SWITCHES];
    int8_t presence;    // 1/0/-1(none)
} hm_device_t;

void hm_init(void);
void hm_lock(void);
void hm_unlock(void);

// Version bumps whenever any state changes. UI compares to detect refresh need.
uint32_t hm_version(void);
void hm_bump(void);     // call while holding the lock after mutating

// Bridge (gateway) online flag.
void hm_set_bridge_online(bool online);
bool hm_bridge_online(void);

// Find or create a device slot by id. Returns NULL if full. Lock held by caller.
hm_device_t *hm_get_device(const char *id);
hm_device_t *hm_find_device(const char *id);   // no create
hm_device_t *hm_device_at(int index);          // index < HM_MAX_DEVICES

// Set one switch value on a device (creates device/switch as needed).
void hm_set_switch(const char *dev_id, const char *sw_id, int8_t on, const char *label);

typedef struct {
    bool valid;
    char id[HM_SCENE_ID_LEN];
    char name[HM_SCENE_NAME_LEN];
    int8_t last_ok;          // 1=ok, 0=fail, -1=idle/pending
    char last_error[48];
} hm_scene_t;

// Replace the scene catalog (from home/v1/scenes). Lock held internally.
void hm_seed_default_scenes(void);  // protocol §8.1 current online scenes
void hm_set_scenes(const hm_scene_t *src, int count);
int hm_scene_count(void);
hm_scene_t *hm_scene_at(int index);          // lock held by caller
hm_scene_t *hm_find_scene(const char *id);   // lock held by caller
void hm_mark_scene_pending(const char *id);
void hm_set_scene_result(const char *id, bool ok, const char *error);
