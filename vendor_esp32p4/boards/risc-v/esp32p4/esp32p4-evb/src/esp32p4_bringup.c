/****************************************************************************
 * vendor_esp32p4/boards/risc-v/esp32p4/esp32p4-evb/src/esp32p4_bringup.c
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
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/types.h>
#include <syslog.h>
#include <errno.h>

#include <nuttx/board.h>

#include "esp32p4-evb.h"
#include "esp32p4_gpio.h"

#ifdef CONFIG_ESP32P4_BLE
#  include "esp32p4_ble.h"
#endif

#ifdef CONFIG_ESP32P4_WIFI
#  include "esp32p4_wifi.h"
#endif

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: esp32p4_bringup
 *
 * Description:
 *   Perform architecture-specific initialization.
 *
 *   CONFIG_BOARD_LATE_INITIALIZE=y:
 *     Called from board_late_initialize().
 *
 *   CONFIG_BOARD_LATE_INITIALIZE=n && CONFIG_BOARDCTL=y:
 *     Called from the NSH library via boardctl().
 *
 *   This function is responsible for initializing all board-specific
 *   peripherals and drivers that are not initialized by the chip layer
 *   during up_initialize().
 *
 ****************************************************************************/

int esp32p4_bringup(void)
{
  int ret = OK;

#ifdef CONFIG_ESP32P4_GPIO
  /* Register the GPIO driver.
   *
   * The GPIO driver (esp32p4_gpio.c) provides GPIO pin configuration,
   * read/write operations, and interrupt dispatch for the 54 GPIO pins
   * on the ESP32-P4.
   *
   * esp32p4_gpio_init() attaches the GPIO interrupt handler to the PLIC
   * and enables the GPIO interrupt.
   */

  ret = esp32p4_gpio_init();
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: Failed to initialize GPIO driver: %d\n", ret);
    }

#ifdef CONFIG_DEV_GPIO
  /* Register selected GPIO pins as /dev/gpioN character devices.
   *
   * The LED GPIO (26) is registered as output, and the BOOT button
   * GPIO (21) is registered as input with pull-up.  Additional pins
   * can be registered here as needed by the application.
   */

  /* GPIO 26: Board LED (output) */

  ret = esp32p4_gpio_register(26, GPIO_OUTPUT_PIN, 0);
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: Failed to register GPIO 26: %d\n", ret);
    }

  /* GPIO 21: BOOT button (input with pull-up, active low) */

  ret = esp32p4_gpio_register(21, GPIO_INPUT_PIN_PULLUP, 1);
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: Failed to register GPIO 21: %d\n", ret);
    }
#endif /* CONFIG_DEV_GPIO */
#endif /* CONFIG_ESP32P4_GPIO */

#ifdef CONFIG_ESP32P4_SPI2
  /* Register the SPI2 bus driver.
   *
   * SPI2 is a general-purpose SPI controller on the ESP32-P4
   * that can be used to connect external SPI devices (sensors,
   * displays, EEPROMs, etc.).
   */

  ret = esp32p4_spidev_register(2);
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: Failed to register SPI2: %d\n", ret);
    }
#endif

#ifdef CONFIG_ESP32P4_SPI3
  /* Register the SPI3 bus driver. */

  ret = esp32p4_spidev_register(3);
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: Failed to register SPI3: %d\n", ret);
    }
#endif

#ifdef CONFIG_ESP32P4_I2C0
  /* Register the I2C0 bus driver.
   *
   * I2C0 is a high-performance I2C controller on the ESP32-P4
   * that can be used to connect external I2C devices (sensors,
   * EEPROMs, etc.).
   */

  ret = esp32p4_i2cbus_register(0);
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: Failed to register I2C0: %d\n", ret);
    }
#endif

#ifdef CONFIG_ESP32P4_I2C1
  /* Register the I2C1 bus driver. */

  ret = esp32p4_i2cbus_register(1);
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: Failed to register I2C1: %d\n", ret);
    }
#endif

#ifdef CONFIG_ESP32P4_EVB_SDCARD
  /* Initialize the SD card interface.
   *
   * This initializes the SDMMC controller, detects the SD card,
   * registers the MMC/SD block driver, and mounts the FAT filesystem
   * if auto-mount is enabled.
   */

  ret = esp32p4_sdcard_initialize();
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: Failed to initialize SD card: %d\n", ret);
    }
#endif

#ifdef CONFIG_ESP32P4_LCD_PANEL
  /* Initialize the MIPI-DSI LCD display.
   *
   * This initializes the MIPI DSI bus, PHY, and host controller,
   * configures the LCD panel, and registers the framebuffer device
   * at /dev/fb0.
   */

  ret = esp32p4_lcd_initialize();
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: Failed to initialize LCD: %d\n", ret);
    }
#endif

#ifdef CONFIG_ESP32P4_WIFI
  /* Initialize the WiFi driver.
   *
   * This initializes the ESP-Hosted WiFi interface via SDIO to
   * the ESP32-C6 co-processor and registers the network device.
   *
   * NOTE: ESP32-P4 has no built-in WiFi.  An external WiFi
   * co-processor (e.g., ESP32-C6) connected via SDIO is required.
   */

  ret = esp_wifi_initialize();
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: Failed to initialize WiFi: %d\n", ret);
    }
#endif

#ifdef CONFIG_ESP32P4_BLE
  /* Initialize the BLE driver.
   *
   * This initializes the NimBLE (or Bluedroid) Bluetooth stack,
   * configures GAP and GATT services, and starts the BLE host
   * task.  The driver will begin advertising automatically once
   * the stack is synced with the controller.
   *
   * NOTE: ESP32-P4 has no built-in Bluetooth radio.  An external
   * BLE co-processor (e.g., ESP32-C6/H2) connected via HCI UART
   * is required for BLE functionality.
   */

  ret = esp32p4_ble_init();
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: Failed to initialize BLE: %d\n", ret);
    }
#endif

  /* Initialize board-specific peripherals.
   *
   * This includes any additional board-level drivers such as:
   *   - External sensors
   *   - Display controllers
   *   - Network interfaces
   *   - USB devices
   *
   * These are board-specific and depend on the EVB hardware
   * configuration.
   */

  return ret;
}

/****************************************************************************
 * Name: board_early_initialize
 *
 * Description:
 *   If CONFIG_BOARD_EARLY_INITIALIZE is selected, then an additional
 *   initialization call will be performed in the boot-up sequence to a
 *   function called board_early_initialize().  board_early_initialize()
 *   will be called immediately after up_initialize() and well before
 *   board_late_initialize() is called and the initial application is
 *   started.
 *
 *   This is the earliest point at which board-specific initialization
 *   can be performed.  At this point, the OS data structures have been
 *   initialized, the heap is available, and the interrupt system is
 *   configured, but the scheduler has not been started yet.
 *
 ****************************************************************************/

#ifdef CONFIG_BOARD_EARLY_INITIALIZE
void board_early_initialize(void)
{
  /* Perform early board-specific hardware initialization.
   *
   * This includes:
   *   - Board GPIO configuration (LEDs, buttons)
   *   - Console UART verification
   *   - Peripheral power domain enable
   *   - IO MUX pin configuration
   */

  esp32p4_board_initialize();
}
#endif

/****************************************************************************
 * Name: board_late_initialize
 *
 * Description:
 *   If CONFIG_BOARD_LATE_INITIALIZE is selected, then an additional
 *   initialization call will be performed in the boot-up sequence to a
 *   function called board_late_initialize().  board_late_initialize()
 *   will be called after up_initialize() and board_early_initialize()
 *   and just before the initial application is started.
 *
 *   This is the point at which drivers for additional board-specific
 *   peripherals should be registered.  The scheduler is running at
 *   this point, so blocking operations can be performed.
 *
 ****************************************************************************/

#ifdef CONFIG_BOARD_LATE_INITIALIZE
void board_late_initialize(void)
{
  /* Perform board-specific initialization.
   *
   * This registers drivers for board-level peripherals such as:
   *   - GPIO device driver
   *   - SPI bus driver
   *   - I2C bus driver
   *   - Other board-specific drivers
   */

  esp32p4_bringup();
}
#endif

/****************************************************************************
 * Name: board_app_initialize
 *
 * Description:
 *   Perform application specific initialization.  This function is never
 *   called directly from application code, but only indirectly via the
 *   (non-standard) boardctl() interface using the command BOARDIOC_INIT.
 *
 *   If CONFIG_BOARD_LATE_INITIALIZE is defined, then this function is
 *   called from board_late_initialize() and the board-specific
 *   initialization has already been performed.
 *
 ****************************************************************************/

int board_app_initialize(uintptr_t arg)
{
#ifdef CONFIG_BOARD_LATE_INITIALIZE
  /* Board initialization already performed by board_late_initialize() */

  return OK;
#else
  /* Perform board-specific initialization */

  return esp32p4_bringup();
#endif
}

/****************************************************************************
 * Name: board_app_finalinitialize
 *
 * Description:
 *   Perform application specific initialization.  This function is never
 *   called directly from application code, but only indirectly via the
 *   (non-standard) boardctl() interface using the command
 *   BOARDIOC_FINALINIT.
 *
 *   This is called after all board_app_initialize() calls have completed.
 *   It provides a final opportunity for board-specific initialization
 *   after all application modules have been initialized.
 *
 ****************************************************************************/

#ifdef CONFIG_BOARDCTL_FINALINIT
int board_app_finalinitialize(uintptr_t arg)
{
  /* No final initialization needed for the ESP32-P4 EVB */

  return OK;
}
#endif
