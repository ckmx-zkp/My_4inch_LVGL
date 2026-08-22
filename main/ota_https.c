#include "ota_https.h"
#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_https_ota.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_system.h"
#include "mqtt_home.h"

static const char *TAG = "ota";

#define URL_MAX 384

static char s_url[URL_MAX];
static volatile int s_progress = -1;
static char s_error[64];
static volatile bool s_running;

int ota_https_progress(void) { return s_progress; }
const char *ota_https_error(void) { return s_error; }

void ota_https_confirm_app(void)
{
    esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "mark app valid: %s", esp_err_to_name(err));
    }
}

static void report(const char *state, int progress, const char *err)
{
    s_progress = progress;
    if (err) {
        strncpy(s_error, err, sizeof(s_error) - 1);
        s_error[sizeof(s_error) - 1] = '\0';
    } else {
        s_error[0] = '\0';
    }
    mqtt_home_publish_ota_status(state, progress, err);
}

static void ota_task(void *arg)
{
    (void)arg;
    report("downloading", 0, NULL);
    ESP_LOGI(TAG, "HTTPS OTA %s", s_url);

    esp_http_client_config_t http = {
        .url = s_url,
        .timeout_ms = 60000,
        .keep_alive_enable = true,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .buffer_size = 4096,
        .buffer_size_tx = 1024,
    };
    esp_https_ota_config_t cfg = {
        .http_config = &http,
    };

    esp_https_ota_handle_t handle = NULL;
    esp_err_t err = esp_https_ota_begin(&cfg, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "begin failed: %s", esp_err_to_name(err));
        report("fail", -1, esp_err_to_name(err));
        s_running = false;
        vTaskDelete(NULL);
        return;
    }

    while (1) {
        err = esp_https_ota_perform(handle);
        if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) break;
        int got = esp_https_ota_get_image_len_read(handle);
        int size = esp_https_ota_get_image_size(handle);
        int pct = 0;
        if (size > 0) {
            pct = (int)((got * 100LL) / size);
            if (pct > 99) pct = 99;
        }
        if (pct != s_progress) {
            s_progress = pct;
            mqtt_home_publish_ota_status("downloading", pct, NULL);
        }
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "perform failed: %s", esp_err_to_name(err));
        esp_https_ota_abort(handle);
        report("fail", -1, esp_err_to_name(err));
        s_running = false;
        vTaskDelete(NULL);
        return;
    }

    if (!esp_https_ota_is_complete_data_received(handle)) {
        ESP_LOGE(TAG, "incomplete image");
        esp_https_ota_abort(handle);
        report("fail", -1, "incomplete");
        s_running = false;
        vTaskDelete(NULL);
        return;
    }

    err = esp_https_ota_finish(handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "finish failed: %s", esp_err_to_name(err));
        report("fail", -1, esp_err_to_name(err));
        s_running = false;
        vTaskDelete(NULL);
        return;
    }

    report("rebooting", 101, NULL);
    ESP_LOGI(TAG, "OTA ok, reboot");
    vTaskDelay(pdMS_TO_TICKS(800));
    esp_restart();
}

bool ota_https_start(const char *url)
{
    if (!url || strncmp(url, "https://", 8) != 0) {
        ESP_LOGW(TAG, "reject url (need https://)");
        report("fail", -1, "need https");
        return false;
    }
    if (s_running) {
        ESP_LOGW(TAG, "OTA already running");
        return false;
    }
    strncpy(s_url, url, sizeof(s_url) - 1);
    s_url[sizeof(s_url) - 1] = '\0';
    s_running = true;
    s_error[0] = '\0';
    s_progress = 0;
    BaseType_t ok = xTaskCreate(ota_task, "ota", 12288, NULL, 5, NULL);
    if (ok != pdPASS) {
        s_running = false;
        report("fail", -1, "no task");
        return false;
    }
    return true;
}
