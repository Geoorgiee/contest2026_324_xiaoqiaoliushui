/****************************************************************************
 * vendor_esp32p4/boards/risc-v/esp32p4/esp32p4-evb/src/esp32p4_lcd.c
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * This file implements the board-level LCD initialization for the
 * ESP32-P4 EVB.  It configures the MIPI-DSI bus and LCD panel
 * using the chip-level esp32p4_mipi_dsi driver.
 *
 * The initialization sequence follows the ESP-IDF camera_dsi example:
 *   1. (Optional) Enable MIPI PHY power via LDO
 *   2. Configure GPIO for panel reset
 *   3. Initialize MIPI DSI bus (PHY, clocks, host controller)
 *   4. Configure LCD panel with video timing from Kconfig
 *   5. Initialize LCD panel and register framebuffer at /dev/fb0
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <syslog.h>
#include <errno.h>

#include "hardware/esp32p4_mipi_dsi.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Derive panel type from Kconfig */

#ifdef CONFIG_ESP32P4_LCD_PANEL_ILI9881C
#  define ESP32P4_EVB_LCD_PANEL_TYPE  ESP32P4_LCD_PANEL_ILI9881C
#elif defined(CONFIG_ESP32P4_LCD_PANEL_EK79007)
#  define ESP32P4_EVB_LCD_PANEL_TYPE  ESP32P4_LCD_PANEL_EK79007
#else
#  define ESP32P4_EVB_LCD_PANEL_TYPE  ESP32P4_LCD_PANEL_CUSTOM
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: esp32p4_lcd_initialize
 *
 * Description:
 *   Initialize the MIPI-DSI LCD display on the ESP32-P4 EVB.
 *
 *   This function:
 *     1. Configures the MIPI DSI bus (PHY, clocks, host controller)
 *     2. Initializes the LCD panel with the correct video timing
 *     3. Registers the framebuffer device at /dev/fb0
 *
 *   The DSI bus configuration uses:
 *     - Number of data lanes: CONFIG_ESP32P4_MIPI_DSI_LANES (1 or 2)
 *     - Lane bit rate: CONFIG_ESP32P4_MIPI_DSI_LANE_RATE_MBPS (80-1500)
 *
 *   The panel configuration uses:
 *     - Resolution: CONFIG_ESP32P4_LCD_HRES x CONFIG_ESP32P4_LCD_VRES
 *     - BPP: CONFIG_ESP32P4_LCD_BPP (16 for RGB565, 24 for RGB888)
 *     - DPI clock: CONFIG_ESP32P4_LCD_DPI_CLK_MHZ
 *     - HSYNC/HBP/HFP/VSYNC/VBP/VFP from Kconfig
 *
 ****************************************************************************/

#ifdef CONFIG_ESP32P4_LCD_PANEL
int esp32p4_lcd_initialize(void)
{
  struct esp32p4_dsi_bus_config_s bus_config;
  struct esp32p4_lcd_panel_config_s panel_config;
  int ret;

  syslog(LOG_INFO, "ESP32P4-EVB: Initializing MIPI-DSI LCD display\n");

  /* Configure the DSI bus */

  bus_config.num_data_lanes = CONFIG_ESP32P4_MIPI_DSI_LANES;
  bus_config.lane_bit_rate_mbps = CONFIG_ESP32P4_MIPI_DSI_LANE_RATE_MBPS;
  bus_config.phy_clk_freq_hz = 0;  /* Use default (20 MHz PLL_F20M) */

  syslog(LOG_INFO, "ESP32P4-EVB: DSI bus: %d lanes, %lu Mbps\n",
         bus_config.num_data_lanes, bus_config.lane_bit_rate_mbps);

  /* Initialize the MIPI DSI bus, PHY, and host controller */

  ret = esp32p4_mipi_dsi_initialize(&bus_config);
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: MIPI DSI bus init failed: %d\n", ret);
      return ret;
    }

  /* Configure the LCD panel */

  panel_config.panel_type = ESP32P4_EVB_LCD_PANEL_TYPE;
  panel_config.reset_gpio_num = CONFIG_ESP32P4_LCD_RESET_GPIO;
  panel_config.bpp = CONFIG_ESP32P4_LCD_BPP;

  /* Video timing from Kconfig */

  panel_config.timing.h_size = CONFIG_ESP32P4_LCD_HRES;
  panel_config.timing.v_size = CONFIG_ESP32P4_LCD_VRES;
  panel_config.timing.dpi_clock_mhz = CONFIG_ESP32P4_LCD_DPI_CLK_MHZ;
  panel_config.timing.hsync_pulse_width = CONFIG_ESP32P4_LCD_HSYNC;
  panel_config.timing.hsync_back_porch = CONFIG_ESP32P4_LCD_HBP;
  panel_config.timing.hsync_front_porch = CONFIG_ESP32P4_LCD_HFP;
  panel_config.timing.vsync_pulse_width = CONFIG_ESP32P4_LCD_VSYNC;
  panel_config.timing.vsync_back_porch = CONFIG_ESP32P4_LCD_VBP;
  panel_config.timing.vsync_front_porch = CONFIG_ESP32P4_LCD_VFP;

  syslog(LOG_INFO, "ESP32P4-EVB: Panel: %dx%d @ %d bpp, DPI clock=%d MHz\n",
         CONFIG_ESP32P4_LCD_HRES, CONFIG_ESP32P4_LCD_VRES,
         CONFIG_ESP32P4_LCD_BPP, CONFIG_ESP32P4_LCD_DPI_CLK_MHZ);

  syslog(LOG_INFO, "ESP32P4-EVB: Timing: HSYNC=%d HBP=%d HFP=%d "
         "VSYNC=%d VBP=%d VFP=%d\n",
         CONFIG_ESP32P4_LCD_HSYNC, CONFIG_ESP32P4_LCD_HBP,
         CONFIG_ESP32P4_LCD_HFP, CONFIG_ESP32P4_LCD_VSYNC,
         CONFIG_ESP32P4_LCD_VBP, CONFIG_ESP32P4_LCD_VFP);

  /* Initialize the LCD panel and register framebuffer */

  ret = esp32p4_mipi_dsi_panel_init(&panel_config);
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: LCD panel init failed: %d\n", ret);
      return ret;
    }

  syslog(LOG_INFO, "ESP32P4-EVB: LCD initialized: %dx%d @ %d bpp, "
         "framebuffer at /dev/fb0\n",
         CONFIG_ESP32P4_LCD_HRES, CONFIG_ESP32P4_LCD_VRES,
         CONFIG_ESP32P4_LCD_BPP);

  return OK;
}
#endif /* CONFIG_ESP32P4_LCD_PANEL */
