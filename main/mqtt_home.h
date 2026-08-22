#pragma once
#include <stdbool.h>

void mqtt_home_start(void);

// Local GPIO was just changed on the panel. Incoming retained hallway
// state is ignored for a few seconds so reconnect cannot undo it.
void mqtt_home_note_local_override(void);

// Publish a set command for the hallway (local) switch: home/v1/{dev}/set {"{sw}":on}
// Best-effort: no-op if MQTT is down. Never blocks local GPIO control.
void mqtt_home_publish_hallway(bool on);

// Generic: publish set to any device switch. Blocked silently if MQTT/bridge offline.
void mqtt_home_publish_set(const char *device_id, const char *switch_id, bool on);

// Trigger a scene from the catalog (home/v1/scenes/{id}/apply).
void mqtt_home_apply_scene(const char *scene_id);

bool mqtt_home_connected(void);
