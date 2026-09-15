# ESP32-P4 MIPI DBI SPI LCD Test

Standalone ESP-IDF project to test MIPI DBI SPI display on ESP32-P4.

## Hardware

Connect an SPI LCD (e.g., ST7789 320x240) to ESP32-P4:

| LCD Pin | ESP32-P4 GPIO | Description |
|---------|---------------|-------------|
| SCK     | GPIO 10       | SPI Clock   |
| MOSI    | GPIO 11       | SPI Data    |
| CS      | GPIO 9        | Chip Select |
| DC      | GPIO 8        | Data/Cmd    |
| RST     | GPIO 12       | Reset       |
| BL      | GPIO 13       | Backlight   |
| VCC     | 3.3V          | Power       |
| GND     | GND           | Ground      |

> **Note**: Adjust pin numbers in `main/test_mipi_dbi.c` for your board.

## Build & Flash

```bash
# Set target to ESP32-P4
idf.py set-target esp32p4

# Build
idf.py build

# Flash (connect board via USB)
idf.py -p /dev/ttyUSB0 flash

# Monitor serial output
idf.py -p /dev/ttyUSB0 monitor
```

## Test Patterns

The program displays these test patterns sequentially:
1. Solid RED
2. Solid GREEN
3. Solid BLUE
4. Color bars (8 vertical stripes)
5. Gradient (RGB gradient)
6. Rectangles and diagonal lines
7. Checkerboard pattern
8. Solid WHITE (final)

## Component Manager

The project uses ESP-IDF Component Manager to fetch:
- `espressif/esp_lcd_panel_io_spi` - SPI panel IO driver
- `espressif/esp_lcd_panel` - LCD panel driver (includes ST7789)

Components are auto-downloaded on first build.

## Customization

Edit `main/test_mipi_dbi.c` to:
- Change pin assignments (`PIN_LCD_*` defines)
- Change display resolution (`LCD_H_RES`, `LCD_V_RES`)
- Change SPI clock speed (`LCD_PIXEL_CLOCK_HZ`)
- Add your own test patterns
