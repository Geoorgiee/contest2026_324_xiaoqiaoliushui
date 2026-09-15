/*
 * ESP32-P4 MIPI DBI SPI LCD Test
 *
 * Standalone ESP-IDF project to test MIPI DBI SPI display driver
 * on ESP32-P4 with an external SPI LCD panel.
 *
 * Hardware connections (adjust pins for your board):
 *   LCD_SCK  -> GPIO (SPI clock)
 *   LCD_MOSI -> GPIO (SPI data)
 *   LCD_CS   -> GPIO (chip select)
 *   LCD_DC   -> GPIO (data/command)
 *   LCD_RST  -> GPIO (reset)
 *   LCD_BL   -> GPIO (backlight)
 *   VCC      -> 3.3V
 *   GND      -> GND
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"

static const char *TAG = "mipi_dbi_test";

/* ----------------------------------------------------------------
 * Pin configuration - ESP32-P4 EVB default (adjust for your board)
 * ---------------------------------------------------------------- */
#define LCD_HOST           SPI2_HOST

#define PIN_LCD_SCK        10
#define PIN_LCD_MOSI       11
#define PIN_LCD_CS          9
#define PIN_LCD_DC          8
#define PIN_LCD_RST        12
#define PIN_LCD_BL         13

/* Display parameters */
#define LCD_H_RES          320
#define LCD_V_RES          240
#define LCD_PIXEL_CLOCK_HZ (40 * 1000 * 1000)  /* 40 MHz SPI clock */
#define LCD_CMD_BITS       8
#define LCD_PARAM_BITS     8
#define LCD_SPI_QUEUE_LEN  10

/* ----------------------------------------------------------------
 * Color definitions (RGB565)
 * ---------------------------------------------------------------- */
#define COLOR_BLACK        0x0000
#define COLOR_WHITE        0xFFFF
#define COLOR_RED          0xF800
#define COLOR_GREEN        0x07E0
#define COLOR_BLUE         0x001F
#define COLOR_YELLOW       0xFFE0
#define COLOR_CYAN         0x07FF
#define COLOR_MAGENTA      0xF81F

/* Framebuffer */
static uint16_t *s_fb = NULL;

/* ----------------------------------------------------------------
 * Draw solid color fill
 * ---------------------------------------------------------------- */
static void fill_screen(uint16_t color)
{
    for (int i = 0; i < LCD_H_RES * LCD_V_RES; i++) {
        s_fb[i] = color;
    }
}

/* ----------------------------------------------------------------
 * Draw color bars (vertical stripes)
 * ---------------------------------------------------------------- */
static void draw_color_bars(void)
{
    const uint16_t colors[] = {
        COLOR_WHITE, COLOR_YELLOW, COLOR_CYAN, COLOR_GREEN,
        COLOR_MAGENTA, COLOR_RED, COLOR_BLUE, COLOR_BLACK,
    };
    const int num_colors = sizeof(colors) / sizeof(colors[0]);
    int bar_width = LCD_H_RES / num_colors;

    for (int y = 0; y < LCD_V_RES; y++) {
        for (int x = 0; x < LCD_H_RES; x++) {
            int bar = x / bar_width;
            if (bar >= num_colors) bar = num_colors - 1;
            s_fb[y * LCD_H_RES + x] = colors[bar];
        }
    }
}

/* ----------------------------------------------------------------
 * Draw a simple rectangle outline
 * ---------------------------------------------------------------- */
static void draw_rect(int x0, int y0, int x1, int y1, uint16_t color)
{
    for (int x = x0; x <= x1; x++) {
        if (y0 >= 0 && y0 < LCD_V_RES) s_fb[y0 * LCD_H_RES + x] = color;
        if (y1 >= 0 && y1 < LCD_V_RES) s_fb[y1 * LCD_H_RES + x] = color;
    }
    for (int y = y0; y <= y1; y++) {
        if (x0 >= 0 && x0 < LCD_H_RES) s_fb[y * LCD_H_RES + x0] = color;
        if (x1 >= 0 && x1 < LCD_H_RES) s_fb[y * LCD_H_RES + x1] = color;
    }
}

/* ----------------------------------------------------------------
 * Draw a filled rectangle
 * ---------------------------------------------------------------- */
static void fill_rect(int x0, int y0, int x1, int y1, uint16_t color)
{
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            if (x >= 0 && x < LCD_H_RES && y >= 0 && y < LCD_V_RES) {
                s_fb[y * LCD_H_RES + x] = color;
            }
        }
    }
}

/* ----------------------------------------------------------------
 * Draw a gradient pattern
 * ---------------------------------------------------------------- */
static void draw_gradient(void)
{
    for (int y = 0; y < LCD_V_RES; y++) {
        for (int x = 0; x < LCD_H_RES; x++) {
            uint8_t r = (x * 31) / LCD_H_RES;    /* 5 bits red */
            uint8_t g = (y * 63) / LCD_V_RES;    /* 6 bits green */
            uint8_t b = ((x + y) * 31) / (LCD_H_RES + LCD_V_RES); /* 5 bits blue */
            s_fb[y * LCD_H_RES + x] = (r << 11) | (g << 5) | b;
        }
    }
}

/* ----------------------------------------------------------------
 * Draw diagonal lines
 * ---------------------------------------------------------------- */
static void draw_diagonal_lines(uint16_t color)
{
    for (int i = 0; i < LCD_H_RES && i < LCD_V_RES; i++) {
        s_fb[i * LCD_H_RES + i] = color;  /* top-left to bottom-right */
        s_fb[i * LCD_H_RES + (LCD_H_RES - 1 - i)] = color;  /* top-right to bottom-left */
    }
}

/* ----------------------------------------------------------------
 * Backlight control
 * ---------------------------------------------------------------- */
static esp_err_t init_backlight(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_LCD_BL),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Backlight GPIO config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Turn on backlight */
    ret = gpio_set_level(PIN_LCD_BL, 1);
    ESP_LOGI(TAG, "Backlight ON");
    return ret;
}

/* ----------------------------------------------------------------
 * Application entry point
 * ---------------------------------------------------------------- */
void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "ESP32-P4 MIPI DBI SPI LCD Test");
    ESP_LOGI(TAG, "Resolution: %dx%d", LCD_H_RES, LCD_V_RES);
    ESP_LOGI(TAG, "SPI Clock: %d MHz", LCD_PIXEL_CLOCK_HZ / 1000000);
    ESP_LOGI(TAG, "========================================");

    /* --------------------------------------------------------
     * Step 1: Allocate framebuffer in PSRAM
     * -------------------------------------------------------- */
    ESP_LOGI(TAG, "Step 1: Allocating framebuffer...");
    s_fb = heap_caps_malloc(LCD_H_RES * LCD_V_RES * sizeof(uint16_t),
                            MALLOC_CAP_SPIRAM);
    if (s_fb == NULL) {
        ESP_LOGE(TAG, "Failed to allocate framebuffer in PSRAM, trying internal...");
        s_fb = heap_caps_malloc(LCD_H_RES * LCD_V_RES * sizeof(uint16_t),
                                MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    }
    if (s_fb == NULL) {
        ESP_LOGE(TAG, "Failed to allocate framebuffer!");
        return;
    }
    ESP_LOGI(TAG, "Framebuffer: %p (%d bytes)", s_fb,
             LCD_H_RES * LCD_V_RES * sizeof(uint16_t));

    /* --------------------------------------------------------
     * Step 2: Initialize backlight
     * -------------------------------------------------------- */
    ESP_LOGI(TAG, "Step 2: Initializing backlight...");
    ESP_ERROR_CHECK(init_backlight());

    /* --------------------------------------------------------
     * Step 3: Create SPI bus
     * -------------------------------------------------------- */
    ESP_LOGI(TAG, "Step 3: Creating SPI bus...");
    spi_bus_config_t bus_cfg = {
        .sclk_io_num = PIN_LCD_SCK,
        .mosi_io_num = PIN_LCD_MOSI,
        .miso_io_num = -1,  /* No MISO for display */
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * LCD_V_RES * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &bus_cfg, SPI_DMA_CH_AUTO));
    ESP_LOGI(TAG, "SPI bus initialized");

    /* --------------------------------------------------------
     * Step 4: Create panel IO (MIPI DBI SPI interface)
     * -------------------------------------------------------- */
    ESP_LOGI(TAG, "Step 4: Creating MIPI DBI SPI panel IO...");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = PIN_LCD_DC,
        .cs_gpio_num = PIN_LCD_CS,
        .pclk_hz = LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = LCD_CMD_BITS,
        .lcd_param_bits = LCD_PARAM_BITS,
        .spi_mode = 0,
        .trans_queue_depth = LCD_SPI_QUEUE_LEN,
    };
    /* Attach LCD to SPI bus */
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST,
                                              &io_config, &io_handle));
    ESP_LOGI(TAG, "Panel IO created");

    /* --------------------------------------------------------
     * Step 5: Create LCD panel driver (ST7789)
     * -------------------------------------------------------- */
    ESP_LOGI(TAG, "Step 5: Creating LCD panel driver...");
    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));
    ESP_LOGI(TAG, "ST7789 panel driver created");

    /* --------------------------------------------------------
     * Step 6: Initialize and configure the display
     * -------------------------------------------------------- */
    ESP_LOGI(TAG, "Step 6: Initializing display...");
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

    /* Turn on display (for ST7789) */
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
    ESP_LOGI(TAG, "Display ON");

    /* Invert colors if needed (ST7789 often needs this) */
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));
    ESP_LOGI(TAG, "Color inversion enabled");

    /* Set orientation (landscape) */
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, false, false));
    ESP_LOGI(TAG, "Orientation: landscape");

    /* --------------------------------------------------------
     * Step 7: Display test patterns
     * -------------------------------------------------------- */
    ESP_LOGI(TAG, "Step 7: Displaying test patterns...");

    /* Test 1: Solid red */
    ESP_LOGI(TAG, "  Test 1: Solid RED");
    fill_screen(COLOR_RED);
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, 0, 0,
                                               LCD_H_RES, LCD_V_RES, s_fb));
    vTaskDelay(pdMS_TO_TICKS(1000));

    /* Test 2: Solid green */
    ESP_LOGI(TAG, "  Test 2: Solid GREEN");
    fill_screen(COLOR_GREEN);
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, 0, 0,
                                               LCD_H_RES, LCD_V_RES, s_fb));
    vTaskDelay(pdMS_TO_TICKS(1000));

    /* Test 3: Solid blue */
    ESP_LOGI(TAG, "  Test 3: Solid BLUE");
    fill_screen(COLOR_BLUE);
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, 0, 0,
                                               LCD_H_RES, LCD_V_RES, s_fb));
    vTaskDelay(pdMS_TO_TICKS(1000));

    /* Test 4: Color bars */
    ESP_LOGI(TAG, "  Test 4: Color bars");
    draw_color_bars();
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, 0, 0,
                                               LCD_H_RES, LCD_V_RES, s_fb));
    vTaskDelay(pdMS_TO_TICKS(2000));

    /* Test 5: Gradient */
    ESP_LOGI(TAG, "  Test 5: Gradient");
    draw_gradient();
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, 0, 0,
                                               LCD_H_RES, LCD_V_RES, s_fb));
    vTaskDelay(pdMS_TO_TICKS(2000));

    /* Test 6: Rectangles */
    ESP_LOGI(TAG, "  Test 6: Rectangles");
    fill_screen(COLOR_BLACK);
    draw_rect(10, 10, 100, 80, COLOR_WHITE);
    fill_rect(120, 10, 210, 80, COLOR_RED);
    draw_rect(10, 100, 100, 170, COLOR_GREEN);
    fill_rect(120, 100, 210, 170, COLOR_BLUE);
    draw_diagonal_lines(COLOR_YELLOW);
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, 0, 0,
                                               LCD_H_RES, LCD_V_RES, s_fb));
    vTaskDelay(pdMS_TO_TICKS(2000));

    /* Test 7: Checkerboard */
    ESP_LOGI(TAG, "  Test 7: Checkerboard");
    for (int y = 0; y < LCD_V_RES; y++) {
        for (int x = 0; x < LCD_H_RES; x++) {
            int block = (x / 20) + (y / 20);
            s_fb[y * LCD_H_RES + x] = (block & 1) ? COLOR_WHITE : COLOR_BLACK;
        }
    }
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, 0, 0,
                                               LCD_H_RES, LCD_V_RES, s_fb));
    vTaskDelay(pdMS_TO_TICKS(2000));

    /* Test 8: Final - white screen */
    ESP_LOGI(TAG, "  Test 8: Solid WHITE");
    fill_screen(COLOR_WHITE);
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, 0, 0,
                                               LCD_H_RES, LCD_V_RES, s_fb));
    vTaskDelay(pdMS_TO_TICKS(1000));

    /* --------------------------------------------------------
     * Done
     * -------------------------------------------------------- */
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "All tests completed successfully!");
    ESP_LOGI(TAG, "========================================");

    /* Keep displaying last pattern */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
