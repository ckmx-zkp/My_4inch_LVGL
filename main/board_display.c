#include "board_display.h"
#include <assert.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_panel_io_additions.h"
#include "esp_lcd_st7701.h"

static const char *TAG = "display";

#define LCD_H_RES 480
#define LCD_V_RES 480

// Backlight (vendor Arduino demo: GPIO 38)
#define BL_GPIO   38

// ST7701 3-wire SPI init pins
#define PIN_CS   39
#define PIN_SCK  48
#define PIN_SDA  47

// RGB sync pins
#define PIN_DE    18
#define PIN_VSYNC 17
#define PIN_HSYNC 16
#define PIN_PCLK  21

// LVGL draw buffer height (lines) in PSRAM.
#define LVGL_BUF_LINES 60

static esp_lcd_panel_handle_t s_panel;
static lv_disp_drv_t s_disp_drv;
static lv_disp_draw_buf_t s_draw_buf;

// Exact ST7701 init sequence from the vendor Arduino demo
// (st7701_type1_init_operations in Arduino_ST7701_RGBPanel.h). The
// esp_lcd_st7701 component's generic default sequence uses different
// gamma/voltage values and leaves this panel dark.
static const st7701_lcd_init_cmd_t vendor_init_cmds[] = {
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x10}, 5, 0},
    {0xC0, (uint8_t[]){0x3B, 0x00}, 2, 0},
    {0xC1, (uint8_t[]){0x0D, 0x02}, 2, 0},
    {0xC2, (uint8_t[]){0x31, 0x05}, 2, 0},
    {0xCD, (uint8_t[]){0x00}, 1, 0},
    {0xB0, (uint8_t[]){0x00, 0x11, 0x18, 0x0E, 0x11, 0x06, 0x07, 0x08,
                       0x07, 0x22, 0x04, 0x12, 0x0F, 0xAA, 0x31, 0x18}, 16, 0},
    {0xB1, (uint8_t[]){0x00, 0x11, 0x19, 0x0E, 0x12, 0x07, 0x08, 0x08,
                       0x08, 0x22, 0x04, 0x11, 0x11, 0xA9, 0x32, 0x18}, 16, 0},

    // PAGE1
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x11}, 5, 0},
    {0xB0, (uint8_t[]){0x60}, 1, 0},   // Vop = 4.7375V
    {0xB1, (uint8_t[]){0x32}, 1, 0},   // VCOM
    {0xB2, (uint8_t[]){0x07}, 1, 0},   // VGH = 15V
    {0xB3, (uint8_t[]){0x80}, 1, 0},
    {0xB5, (uint8_t[]){0x49}, 1, 0},   // VGL = -10.17V
    {0xB7, (uint8_t[]){0x85}, 1, 0},
    {0xB8, (uint8_t[]){0x21}, 1, 0},   // AVDD = 6.6, AVCL = -4.6
    {0xC1, (uint8_t[]){0x78}, 1, 0},
    {0xC2, (uint8_t[]){0x78}, 1, 0},
    {0xE0, (uint8_t[]){0x00, 0x1B, 0x02}, 3, 0},
    {0xE1, (uint8_t[]){0x08, 0xA0, 0x00, 0x00, 0x07, 0xA0, 0x00, 0x00,
                       0x00, 0x44, 0x44}, 11, 0},
    {0xE2, (uint8_t[]){0x11, 0x11, 0x44, 0x44, 0xED, 0xA0, 0x00, 0x00,
                       0xEC, 0xA0, 0x00, 0x00}, 12, 0},
    {0xE3, (uint8_t[]){0x00, 0x00, 0x11, 0x11}, 4, 0},
    {0xE4, (uint8_t[]){0x44, 0x44}, 2, 0},
    {0xE5, (uint8_t[]){0x0A, 0xE9, 0xD8, 0xA0, 0x0C, 0xEB, 0xD8, 0xA0,
                       0x0E, 0xED, 0xD8, 0xA0, 0x10, 0xEF, 0xD8, 0xA0}, 16, 0},
    {0xE6, (uint8_t[]){0x00, 0x00, 0x11, 0x11}, 4, 0},
    {0xE7, (uint8_t[]){0x44, 0x44}, 2, 0},
    {0xE8, (uint8_t[]){0x09, 0xE8, 0xD8, 0xA0, 0x0B, 0xEA, 0xD8, 0xA0,
                       0x0D, 0xEC, 0xD8, 0xA0, 0x0F, 0xEE, 0xD8, 0xA0}, 16, 0},
    {0xEB, (uint8_t[]){0x02, 0x00, 0xE4, 0xE4, 0x88, 0x00, 0x40}, 7, 0},
    {0xEC, (uint8_t[]){0x3C, 0x00}, 2, 0},
    {0xED, (uint8_t[]){0xAB, 0x89, 0x76, 0x54, 0x02, 0xFF, 0xFF, 0xFF,
                       0xFF, 0xFF, 0xFF, 0x20, 0x45, 0x67, 0x98, 0xBA}, 16, 0},

    // VAP & VAN
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x13}, 5, 0},
    {0xE5, (uint8_t[]){0xE4}, 1, 0},

    // Back to command page 0, then color format, sleep out, display on.
    // The vendor sends COLMOD here, at the end and on page 0 — the component
    // sends its own COLMOD before this sequence, while the command page is
    // still unset.
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x00}, 5, 0},
    {0x3A, (uint8_t[]){0x60}, 1, 10},    // RGB666, matches type1
    {0x11, (uint8_t[]){0x00}, 0, 120},   // Sleep Out
    {0x29, (uint8_t[]){0x00}, 0, 20},    // Display On
    {0x20, (uint8_t[]){0x00}, 0, 0},     // inversion off (Arduino invertDisplay(false) on IPS)
};

static void backlight_init(void)
{
    // Plain digital backlight, matching vendor Arduino demo exactly
    // (pinMode(GFX_BL, OUTPUT); digitalWrite(GFX_BL, HIGH);) to rule out
    // any LEDC/PWM misconfiguration.
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << BL_GPIO,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&io));
    ESP_ERROR_CHECK(gpio_set_level(BL_GPIO, 1));
    ESP_LOGI(TAG, "backlight on (GPIO%d)", BL_GPIO);
}

static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    esp_lcd_panel_draw_bitmap(s_panel, area->x1, area->y1,
                              area->x2 + 1, area->y2 + 1, color_map);
    lv_disp_flush_ready(drv);
}

static void lv_tick_cb(void *arg)
{
    lv_tick_inc(2);
}

lv_disp_t *board_display_init(void)
{
    backlight_init();

    // 3-wire SPI. Vendor Arduino bit-bangs Mode 0 (SCK idle low, sample on
    // rising edge). Espressif's ST7701 example uses the same: scl_active_edge=0.
    spi_line_config_t line_cfg = {
        .cs_io_type = IO_TYPE_GPIO, .cs_gpio_num = PIN_CS,
        .scl_io_type = IO_TYPE_GPIO, .scl_gpio_num = PIN_SCK,
        .sda_io_type = IO_TYPE_GPIO, .sda_gpio_num = PIN_SDA,
        .io_expander = NULL,
    };
    esp_lcd_panel_io_3wire_spi_config_t io_cfg = ST7701_PANEL_IO_3WIRE_SPI_CONFIG(line_cfg, 0);
    esp_lcd_panel_io_handle_t io_handle = NULL;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_3wire_spi(&io_cfg, &io_handle));

    // ---- RGB panel config ----
    esp_lcd_rgb_panel_config_t rgb_cfg = {
        .clk_src = LCD_CLK_SRC_PLL160M,
        .sram_trans_align = 8,
        .psram_trans_align = 64,
        .data_width = 16,
        .bits_per_pixel = 16,
        .num_fbs = 1,
        .bounce_buffer_size_px = LCD_H_RES * 10,
        .de_gpio_num = PIN_DE,
        .pclk_gpio_num = PIN_PCLK,
        .vsync_gpio_num = PIN_VSYNC,
        .hsync_gpio_num = PIN_HSYNC,
        .disp_gpio_num = -1,
        // RGB565 data order: B0..B4, G0..G5, R0..R4  (from board schematic)
        .data_gpio_nums = {
            4, 5, 6, 7, 15,          // B0-B4
            8, 20, 3, 46, 9, 10,     // G0-G5
            11, 12, 13, 14, 0,       // R0-R4
        },
        .timings = {
            .pclk_hz = 11 * 1000 * 1000,
            .h_res = LCD_H_RES,
            .v_res = LCD_V_RES,
            .hsync_back_porch = 50,
            .hsync_front_porch = 10,
            .hsync_pulse_width = 8,
            .vsync_back_porch = 20,
            .vsync_front_porch = 10,
            .vsync_pulse_width = 8,
            .flags.pclk_active_neg = false,
        },
        .flags.fb_in_psram = 1,
    };

    st7701_vendor_config_t vendor_cfg = {
        .rgb_config = &rgb_cfg,
        .init_cmds = vendor_init_cmds,
        .init_cmds_size = sizeof(vendor_init_cmds) / sizeof(vendor_init_cmds[0]),
        .flags = {
            .auto_del_panel_io = 0,
            .mirror_by_cmd = 0,
        },
    };

    // bits_per_pixel=18 makes the component send COLMOD 0x60 (RGB666), matching
    // the vendor type1 sequence. RGB bus stays 16-bit. MADCTL 0x00 matches
    // Arduino rotation=0 with BGR=true.
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = -1,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 18,
        .vendor_config = &vendor_cfg,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7701(io_handle, &panel_cfg, &s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel, true));
    ESP_LOGI(TAG, "ST7701 panel ready %dx%d", LCD_H_RES, LCD_V_RES);

    // ---- LVGL ----
    lv_init();

    size_t buf_px = LCD_H_RES * LVGL_BUF_LINES;
    lv_color_t *buf1 = heap_caps_malloc(buf_px * sizeof(lv_color_t),
                                        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    lv_color_t *buf2 = heap_caps_malloc(buf_px * sizeof(lv_color_t),
                                        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    assert(buf1 && buf2);
    lv_disp_draw_buf_init(&s_draw_buf, buf1, buf2, buf_px);

    lv_disp_drv_init(&s_disp_drv);
    s_disp_drv.hor_res = LCD_H_RES;
    s_disp_drv.ver_res = LCD_V_RES;
    s_disp_drv.flush_cb = lvgl_flush_cb;
    s_disp_drv.draw_buf = &s_draw_buf;
    lv_disp_t *disp = lv_disp_drv_register(&s_disp_drv);

    // LVGL tick via esp_timer (2ms).
    const esp_timer_create_args_t tick_args = {
        .callback = lv_tick_cb, .name = "lv_tick",
    };
    esp_timer_handle_t tick;
    ESP_ERROR_CHECK(esp_timer_create(&tick_args, &tick));
    ESP_ERROR_CHECK(esp_timer_start_periodic(tick, 2 * 1000));

    return disp;
}
