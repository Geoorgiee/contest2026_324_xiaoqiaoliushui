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

#include <debug.h>
#include <fcntl.h>
#include <syslog.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <nuttx/fs/fs.h>

#include <arch/board/board.h>

#include "espressif/esp_start.h"

#ifdef CONFIG_WATCHDOG
#  include "espressif/esp_wdt.h"
#endif

#ifdef CONFIG_TIMER
#  include "espressif/esp_gptimer.h"
#endif

#ifdef CONFIG_ONESHOT
#  include "espressif/esp_oneshot.h"
#endif

#ifdef CONFIG_RTC_DRIVER
#  include "espressif/esp_rtc.h"
#endif

#ifdef CONFIG_DEV_GPIO
#  include "espressif/esp_gpio.h"
#endif

#ifdef CONFIG_INPUT_BUTTONS
#  include <nuttx/input/buttons.h>
#endif

#ifdef CONFIG_ESPRESSIF_EFUSE
#  include "espressif/esp_efuse.h"
#endif

#ifdef CONFIG_ESP_RMT
#  include "esp_board_rmt.h"
#endif

#ifdef CONFIG_ESPRESSIF_I2S
#  include "esp_board_i2s.h"
#endif

#ifdef CONFIG_ESPRESSIF_SPI
#  include "espressif/esp_spi.h"
#  include "esp_board_spidev.h"
#  ifdef CONFIG_ESPRESSIF_SPI_BITBANG
#    include "espressif/esp_spi_bitbang.h"
#  endif
#endif

#ifdef CONFIG_SPI_SLAVE_DRIVER
#  include "espressif/esp_spi.h"
#  include "esp_board_spislavedev.h"
#endif

#ifdef CONFIG_ESPRESSIF_TEMP
#  include "espressif/esp_temperature_sensor.h"
#endif

#ifdef CONFIG_ESP_MCPWM
#  include "esp_board_mcpwm.h"
#endif

#ifdef CONFIG_ESP_PCNT
#  include "espressif/esp_pcnt.h"
#  include "esp_board_pcnt.h"
#endif

#ifdef CONFIG_ESPRESSIF_ADC
#  include "esp_board_adc.h"
#endif

#ifdef CONFIG_PM
#  include "espressif/esp_pm.h"
#endif

#ifdef CONFIG_SYSTEM_NXDIAG_ESPRESSIF_CHIP_WO_TOOL
#  include "espressif/esp_nxdiag.h"
#endif

#ifdef CONFIG_ESP_SDM
#  include "espressif/esp_sdm.h"
#endif

#ifdef CONFIG_COMP
#  include "espressif/esp_ana_cmpr.h"
#endif

#ifdef CONFIG_ESPRESSIF_USE_LP_CORE
#  include "espressif/esp_ulp.h"
#  ifdef CONFIG_ESPRESSIF_ULP_USE_TEST_BIN
#    include "ulp/ulp_code.h"
#  endif
#  ifdef CONFIG_ESPRESSIF_LP_MAILBOX
#    include "espressif/esp_lp_mailbox.h"
#  endif
#endif

#include "esp32p4-evb.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: esp_bringup
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

int esp_bringup(void)
{
  int ret = OK;

#ifdef CONFIG_FS_PROCFS
  /* Mount the procfs file system */

  ret = nx_mount(NULL, "/proc", "procfs", 0, NULL);
  if (ret < 0)
    {
      _err("Failed to mount procfs at /proc: %d\n", ret);
    }
#endif

#ifdef CONFIG_FS_TMPFS
  /* Mount the tmpfs file system */

  ret = nx_mount(NULL, CONFIG_LIBC_TMPDIR, "tmpfs", 0, NULL);
  if (ret < 0)
    {
      _err("Failed to mount tmpfs at %s: %d\n", CONFIG_LIBC_TMPDIR, ret);
    }
#endif

#if defined(CONFIG_ESPRESSIF_EFUSE)
  ret = esp_efuse_initialize("/dev/efuse");
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: Failed to init EFUSE: %d\n", ret);
    }
#endif

#ifdef CONFIG_ESPRESSIF_MWDT0
  ret = esp_wdt_initialize("/dev/watchdog0", ESP_WDT_MWDT0);
  if (ret < 0)
    {
      _err("Failed to initialize WDT: %d\n", ret);
    }
#endif

#ifdef CONFIG_ESPRESSIF_MWDT1
  ret = esp_wdt_initialize("/dev/watchdog1", ESP_WDT_MWDT1);
  if (ret < 0)
    {
      _err("Failed to initialize WDT: %d\n", ret);
    }
#endif

#ifdef CONFIG_ESPRESSIF_RWDT
  ret = esp_wdt_initialize("/dev/watchdog2", ESP_WDT_RWDT);
  if (ret < 0)
    {
      _err("Failed to initialize WDT: %d\n", ret);
    }
#endif

#ifdef CONFIG_TIMER
  ret = esp_timer_initialize(0);
  if (ret < 0)
    {
      _err("Failed to initialize Timer 0: %d\n", ret);
    }

#ifndef CONFIG_ONESHOT
  ret = esp_timer_initialize(1);
  if (ret < 0)
    {
      _err("Failed to initialize Timer 1: %d\n", ret);
    }
#endif
#endif

#ifdef CONFIG_ONESHOT
  ret = esp_oneshot_initialize();
  if (ret < 0)
    {
      _err("Failed to initialize Oneshot Timer: %d\n", ret);
    }
#endif

#ifdef CONFIG_ESP_RMT
  ret = board_rmt_txinitialize(RMT_OUTPUT_PIN);
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: board_rmt_txinitialize() failed: %d\n", ret);
    }

  ret = board_rmt_rxinitialize(RMT_INPUT_PIN);
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: board_rmt_txinitialize() failed: %d\n", ret);
    }
#endif

#ifdef CONFIG_RTC_DRIVER
  /* Initialize the RTC driver */

  ret = esp_rtc_driverinit();
  if (ret < 0)
    {
      _err("Failed to initialize the RTC driver: %d\n", ret);
    }
#endif

#ifdef CONFIG_ESPRESSIF_SPI
#  if defined(CONFIG_ESPRESSIF_SPI2_SLAVE) && defined(CONFIG_ESPRESSIF_SPI2)
  ret = board_spislavedev_initialize(ESPRESSIF_SPI2);
  if (ret < 0)
    {
      syslog(LOG_ERR, "Failed to initialize SPI%d Slave driver: %d\n",
             ESPRESSIF_SPI2, ret);
    }
#  elif defined(CONFIG_ESPRESSIF_SPI2)
  ret = board_spidev_initialize(ESPRESSIF_SPI2);
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: Failed to init spidev 2: %d\n", ret);
    }
#  endif

#  if defined(CONFIG_ESPRESSIF_SPI3_SLAVE) && defined(CONFIG_ESPRESSIF_SPI3)
  ret = board_spislavedev_initialize(ESPRESSIF_SPI3);
  if (ret < 0)
    {
      syslog(LOG_ERR, "Failed to initialize SPI%d Slave driver: %d\n",
             ESPRESSIF_SPI3, ret);
    }
#  elif defined(CONFIG_ESPRESSIF_SPI3)
  ret = board_spidev_initialize(ESPRESSIF_SPI3);
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: Failed to init spidev 3: %d\n", ret);
    }
#  endif
#endif

#ifdef CONFIG_ESPRESSIF_TEMP
  ret = esp_temperature_sensor_initialize();
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: Failed to init temperature sensor: %d\n", ret);
    }
#endif

#ifdef CONFIG_ESPRESSIF_I2S
  ret = board_i2s_initialize(0);
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: Failed to init I2S: %d\n", ret);
    }
#endif

#ifdef CONFIG_ESPRESSIF_LEDC
  ret = board_ledc_initialize();
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: Failed to init LEDC: %d\n", ret);
    }
#endif

#ifdef CONFIG_ESP_MCPWM
  ret = board_mcpwm_initialize();
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: Failed to init MCPWM: %d\n", ret);
    }
#endif

#ifdef CONFIG_ESP_PCNT
  ret = esp_pcnt_initialize();
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: Failed to init PCNT: %d\n", ret);
    }
#endif

#ifdef CONFIG_ESPRESSIF_ADC
  ret = board_adc_initialize();
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: Failed to init ADC: %d\n", ret);
    }
#endif

#ifdef CONFIG_ESP_SDM
  ret = esp_sdm_initialize();
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: Failed to init SDM: %d\n", ret);
    }
#endif

#ifdef CONFIG_COMP
  ret = esp_ana_cmpr_initialize();
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: Failed to init analog comparator: %d\n", ret);
    }
#endif

#ifdef CONFIG_ESPRESSIF_USE_LP_CORE
  ret = esp_ulp_initialize();
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: Failed to init LP core: %d\n", ret);
    }
#endif

#ifdef CONFIG_ESPRESSIF_SPIFLASH
  ret = esp_spiflash_initialize();
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: Failed to init SPI Flash: %d\n", ret);
    }
#endif

#ifdef CONFIG_ESPRESSIF_EMAC
  ret = board_emac_init();
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: Failed to init EMAC: %d\n", ret);
    }
#endif

#ifdef CONFIG_DEV_GPIO
  ret = esp_gpio_init();
  if (ret < 0)
    {
      ierr("Failed to initialize GPIO Driver: %d\n", ret);
    }
#endif

#ifdef CONFIG_INPUT_BUTTONS
  ret = btn_lower_initialize("/dev/buttons");
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: btn_lower_initialize() failed: %d\n", ret);
    }
#endif

#ifdef CONFIG_ESPRESSIF_WIRELESS
  ret = esp_wireless_initialize();
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: esp_wireless_initialize() failed: %d\n", ret);
    }
#endif

#ifdef CONFIG_ESPRESSIF_BLE
  ret = esp_ble_initialize();
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: esp_ble_initialize() failed: %d\n", ret);
    }
#endif

#ifdef CONFIG_ESPRESSIF_I2C
  ret = board_i2c_initialize();
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: board_i2c_initialize() failed: %d\n", ret);
    }
#endif

#ifdef CONFIG_ESPRESSIF_BMP180
  ret = board_bmp180_initialize(0, 0);
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: board_bmp180_initialize() failed: %d\n", ret);
    }
#endif

  return ret;
}
