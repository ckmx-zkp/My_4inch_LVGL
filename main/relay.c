#include "relay.h"
#include "sdkconfig.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "relay";

static const int s_gpios[3] = { RELAY1_GPIO, RELAY2_GPIO, RELAY3_GPIO };
static bool s_state[3] = { false, false, false };

#ifdef CONFIG_RELAY_ACTIVE_LOW
#define RELAY_LEVEL(on) ((on) ? 0 : 1)
#else
#define RELAY_LEVEL(on) ((on) ? 1 : 0)
#endif

void relay_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << RELAY1_GPIO) | (1ULL << RELAY2_GPIO) | (1ULL << RELAY3_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);

    // Power-on: all relays off.
    for (int i = 0; i < 3; i++) {
        s_state[i] = false;
        gpio_set_level(s_gpios[i], RELAY_LEVEL(false));
    }
    ESP_LOGI(TAG, "relay init done (relay1=IO%d)", RELAY1_GPIO);
}

void relay_set(int idx, bool on)
{
    if (idx < 0 || idx >= 3) return;
    s_state[idx] = on;
    gpio_set_level(s_gpios[idx], RELAY_LEVEL(on));
    ESP_LOGI(TAG, "relay%d -> %s", idx + 1, on ? "ON" : "OFF");
}

bool relay_get(int idx)
{
    if (idx < 0 || idx >= 3) return false;
    return s_state[idx];
}
