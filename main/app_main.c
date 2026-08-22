#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_log.h"

#include "lvgl.h"
#include "relay.h"
#include "board_display.h"
#include "board_touch.h"
#include "ui/ui.h"
#include "home_model.h"
#include "wifi.h"
#include "mqtt_home.h"

static const char *TAG = "app_main";

// WiFi/MQTT run off the LVGL thread so the local relay page is usable
// even if the AP is down or the first DHCP takes a long time.
static void net_task(void *arg)
{
    (void)arg;
    wifi_init_sta();
    mqtt_home_start();
    vTaskDelete(NULL);
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    relay_init();
    hm_init();

    ESP_LOGI(TAG, "init display...");
    lv_disp_t *disp = board_display_init();

    ESP_LOGI(TAG, "init touch...");
    board_touch_init(disp);

    ui_init();

    xTaskCreate(net_task, "net", 6144, NULL, 5, NULL);

    ESP_LOGI(TAG, "entering lvgl loop");
    while (1) {
        uint32_t ms = lv_timer_handler();
        if (ms < 5) ms = 5;
        if (ms > 50) ms = 50;
        vTaskDelay(pdMS_TO_TICKS(ms));
    }
}
