/****************************************************************************
 * board/contest2026_288_board/src/esp32p4_lcd.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Board glue for the ESP32-P4 MIPI-DSI display (ILI9881C 800x1280).
 *
 * The heavy lifting lives in chip/espressif/esp32p4_mipi_dsi.c (ported from
 * vendor_esp32p4/chips/esp32p4/esp32p4_mipi_dsi.c).  That upper half
 * registers /dev/fb0 internally; this file only supplies the board-side
 * panel timing choice, the LCD reset GPIO and the backlight GPIO.
 *
 ****************************************************************************/

#include <nuttx/config.h>

#ifdef CONFIG_ESPRESSIF_MIPI_DSI

#include <errno.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <syslog.h>

#include <nuttx/signal.h>

#include "espressif/esp_gpio.h"

/* Chip-level driver header (chip/espressif is on the include path via the
 * nuttx/arch/risc-v/src/chip -> contest2026_288_board/chip symlink).
 */

#include "hardware/esp32p4_mipi_dsi.h"

#include "esp32p4-function-ev-board.h"

#ifdef CONFIG_ESPRESSIF_MIPI_DSI_PANEL_ILI9881C

/* ILI9881C 800x1280 panel timing (matches the vendor_esp32p4
 * configs/display defconfig and the ESP-IDF ILI9881C init sequence).
 */

#define BOARD_LCD_HRES        800
#define BOARD_LCD_VRES        1280
#define BOARD_LCD_BPP         16
#define BOARD_LCD_DPI_CLK_MHZ 80
#define BOARD_LCD_HSYNC       40
#define BOARD_LCD_HBP         140
#define BOARD_LCD_HFP         40
#define BOARD_LCD_VSYNC       4
#define BOARD_LCD_VBP         16
#define BOARD_LCD_VFP         16

#else
#  error "Only the ILI9881C panel is wired for the 288 board"
#endif

/* DSI bus: two data lanes, 1000 Mbps/lane (vendor display defconfig). */

#define BOARD_DSI_LANES   2
#define BOARD_DSI_RATE_MBPS 1000

/* Reset pulse mirrors the ESP-IDF ILI9881C reset guidance. */

#define BOARD_LCD_RESET_ASSERT_MS   10
#define BOARD_LCD_RESET_RELEASE_MS 120

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: esp32p4_lcd_reset
 *
 * Description:
 *   Hardware-reset the LCD panel.  Assert for 10 ms, release for 120 ms.
 *   Skipped when no reset GPIO is configured (-1).
 *
 ****************************************************************************/

int esp32p4_lcd_reset(void)
{
#ifdef CONFIG_ESPRESSIF_MIPI_DSI_LCD_RESET_GPIO
  int gpio = CONFIG_ESPRESSIF_MIPI_DSI_LCD_RESET_GPIO;
  int ret;

  if (gpio < 0)
    {
      syslog(LOG_INFO, "LCD: no panel reset GPIO configured\n");
      return OK;
    }

  ret = esp_configgpio(gpio, OUTPUT);
  if (ret < 0)
    {
      syslog(LOG_ERR, "LCD: reset GPIO%d config failed: %d\n", gpio, ret);
      return -EIO;
    }

  /* RST is active low on the ILI9881C modules. */

  esp_gpiowrite(gpio, false);
  nxsig_usleep(BOARD_LCD_RESET_ASSERT_MS * 1000);
  esp_gpiowrite(gpio, true);
  nxsig_usleep(BOARD_LCD_RESET_RELEASE_MS * 1000);
  syslog(LOG_INFO, "LCD: panel reset done on GPIO%d\n", gpio);
#else
  syslog(LOG_INFO, "LCD: panel reset GPIO not configured, skip\n");
#endif

  return OK;
}

/****************************************************************************
 * Name: esp32p4_lcd_backlight_set
 *
 * Description:
 *   Enable or disable the LCD backlight (static GPIO on/off; PWM dimming
 *   is a later board integration step).
 *
 ****************************************************************************/

int esp32p4_lcd_backlight_set(bool enable)
{
#ifdef CONFIG_ESPRESSIF_MIPI_DSI_LCD_BACKLIGHT_GPIO
  int gpio = CONFIG_ESPRESSIF_MIPI_DSI_LCD_BACKLIGHT_GPIO;
  int ret;

  if (gpio < 0)
    {
      return OK;
    }

  ret = esp_configgpio(gpio, OUTPUT);
  if (ret < 0)
    {
      syslog(LOG_ERR, "LCD: backlight GPIO%d config failed: %d\n", gpio, ret);
      return -EIO;
    }

  esp_gpiowrite(gpio, enable);
  syslog(LOG_INFO, "LCD: backlight=%d gpio=%d\n", enable, gpio);
#else
  syslog(LOG_INFO, "LCD: backlight GPIO not configured, skip (enable=%d)\n",
         enable);
#endif

  return OK;
}

/****************************************************************************
 * Name: esp32p4_lcd_initialize
 *
 * Description:
 *   Bring up the MIPI-DSI bus and register /dev/fb0 (800x1280 RGB565).
 *   Called from esp32p4_bringup.c.  Failures are logged but do not abort
 *   the rest of NSH bring-up.
 *
 ****************************************************************************/

int esp32p4_lcd_initialize(void)
{
  struct esp32p4_dsi_bus_config_s bus_config;
  struct esp32p4_lcd_panel_config_s panel_config;
  int ret;

  esp32p4_lcd_backlight_set(false);

  memset(&bus_config, 0, sizeof(bus_config));
  bus_config.num_data_lanes = BOARD_DSI_LANES;
  bus_config.lane_bit_rate_mbps = BOARD_DSI_RATE_MBPS;
  bus_config.phy_clk_freq_hz = 20 * 1000 * 1000;

  syslog(LOG_INFO, "LCD: DSI bus init lanes=%d rate=%lu Mbps\n",
         bus_config.num_data_lanes,
         (unsigned long)bus_config.lane_bit_rate_mbps);

  ret = esp32p4_mipi_dsi_initialize(&bus_config);
  if (ret < 0)
    {
      syslog(LOG_ERR, "LCD: DSI bus init failed: %d\n", ret);
      return ret;
    }

  memset(&panel_config, 0, sizeof(panel_config));
  panel_config.panel_type = ESP32P4_LCD_PANEL_ILI9881C;
  panel_config.reset_gpio_num = -1;  /* esp32p4_lcd_reset() drives GPIO */
  panel_config.bpp = BOARD_LCD_BPP;
  panel_config.timing.h_size = BOARD_LCD_HRES;
  panel_config.timing.v_size = BOARD_LCD_VRES;
  panel_config.timing.hsync_pulse_width = BOARD_LCD_HSYNC;
  panel_config.timing.hsync_back_porch = BOARD_LCD_HBP;
  panel_config.timing.hsync_front_porch = BOARD_LCD_HFP;
  panel_config.timing.vsync_pulse_width = BOARD_LCD_VSYNC;
  panel_config.timing.vsync_back_porch = BOARD_LCD_VBP;
  panel_config.timing.vsync_front_porch = BOARD_LCD_VFP;
  panel_config.timing.dpi_clock_mhz = BOARD_LCD_DPI_CLK_MHZ;

  ret = esp32p4_lcd_reset();
  if (ret < 0)
    {
      syslog(LOG_ERR, "LCD: panel reset failed: %d\n", ret);
      return ret;
    }

  ret = esp32p4_mipi_dsi_panel_init(&panel_config);
  if (ret < 0)
    {
      syslog(LOG_ERR, "LCD: panel init failed: %d\n", ret);
      return ret;
    }

  esp32p4_lcd_backlight_set(true);

  syslog(LOG_INFO,
         "LCD: /dev/fb0 registered (ILI9881C %dx%d RGB565, "
         "DPI clock=%lu MHz)\n",
         BOARD_LCD_HRES, BOARD_LCD_VRES,
         (unsigned long)BOARD_LCD_DPI_CLK_MHZ);
  return OK;
}

#endif /* CONFIG_ESPRESSIF_MIPI_DSI */
