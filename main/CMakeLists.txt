idf_component_register(
    SRCS
        "app_main.c"
        "lv_mem_psram.c"
        "wifi.c"
        "board_display.c"
        "board_touch.c"
        "relay.c"
        "home_model.c"
        "mqtt_home.c"
        "ota_https.c"
        "ui/ui.c"
        "ui/ui_theme.c"
        "ui/page_relay.c"
        "ui/page_scenes.c"
        "ui/page_devices.c"
        "ui/page_ota.c"
        "ui/fonts/font_cn_22.c"
        "ui/fonts/font_cn_28.c"
    INCLUDE_DIRS
        "."
        "ui"
    REQUIRES
        esp_wifi
        nvs_flash
        esp_lcd
        esp_driver_gpio
        esp_driver_i2c
        esp_driver_ledc
        mqtt
        json
        app_update
        esp_https_ota
        esp_http_client
        mbedtls
        lvgl
        esp_lcd_st7701
        esp_lcd_touch_gt911
)

# Built-in LVGL tlsf pool is otherwise a .bss array (Kconfig max 128KB, internal RAM).
# Allocate 512KB from the 8MB OPI PSRAM instead.
idf_component_get_property(lvgl_lib lvgl__lvgl COMPONENT_LIB)
target_compile_definitions(${lvgl_lib} PUBLIC
    LV_MEM_SIZE=524288U
    LV_MEM_POOL_INCLUDE="lv_mem_psram.h"
    LV_MEM_POOL_ALLOC=lv_psram_pool_alloc
)
target_include_directories(${lvgl_lib} PUBLIC ${CMAKE_CURRENT_LIST_DIR})
# Keep the pool allocator when --gc-sections runs (LVGL is a later archive).
target_link_options(${COMPONENT_LIB} INTERFACE -u lv_psram_pool_alloc)

