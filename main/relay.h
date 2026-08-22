#pragma once
#include <stdbool.h>

// Onboard relay GPIOs (ESP32-4848S040 H1 header): relay1/2/3.
// relay1 physically drives the hallway light.
#define RELAY1_GPIO 40
#define RELAY2_GPIO 2
#define RELAY3_GPIO 1

#define RELAY_HALLWAY 0  // index of relay1

void relay_init(void);
void relay_set(int idx, bool on);
bool relay_get(int idx);
