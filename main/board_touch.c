#include "board_touch.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_io_i2c.h"

static const char *TAG = "touch";

#define I2C_PORT     I2C_NUM_0
#define PIN_SCL      45
#define PIN_SDA      19

static esp_lcd_touch_handle_t s_tp;

static void touchpad_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    esp_lcd_touch_point_data_t pts[1];
    uint8_t n = 0;
    esp_lcd_touch_read_data(s_tp);
    esp_lcd_touch_get_data(s_tp, pts, &n, 1);
    if (n > 0) {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = pts[0].x;
        data->point.y = pts[0].y;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

void board_touch_init(lv_disp_t *disp)
{
    i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = PIN_SDA,
        .scl_io_num = PIN_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_PORT, &i2c_conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_PORT, i2c_conf.mode, 0, 0, 0));

    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t io_cfg = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    io_cfg.scl_speed_hz = 0;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c((esp_lcd_i2c_bus_handle_t)I2C_PORT, &io_cfg, &io_handle));

    esp_lcd_touch_config_t tp_cfg = {
        .x_max = 480,
        .y_max = 480,
        .rst_gpio_num = -1,
        .int_gpio_num = -1,
        .flags = { .swap_xy = 0, .mirror_x = 0, .mirror_y = 0 },
    };
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_gt911(io_handle, &tp_cfg, &s_tp));

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touchpad_read_cb;
    indev_drv.disp = disp;
    lv_indev_drv_register(&indev_drv);

    ESP_LOGI(TAG, "GT911 touch ready (SDA=%d SCL=%d)", PIN_SDA, PIN_SCL);
}
