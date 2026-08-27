/****************************************************************************
 * vendor_esp32p4/boards/risc-v/esp32p4/esp32p4-evb/src/esp32p4_sdcard.c
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
 * This file implements the board-level SD card driver for the ESP32-P4 EVB.
 *
 * It supports two interface modes:
 *
 *   1. SDMMC interface (CONFIG_ESP32P4_EVB_SDCARD_SDMMC):
 *      Uses the dedicated SDMMC peripheral for high-speed SD card access.
 *      Supports 1-bit and 4-bit bus widths. This is the recommended
 *      interface for the ESP32-P4 EVB.
 *
 *   2. SPI interface (CONFIG_ESP32P4_EVB_SDCARD_SPI):
 *      Uses the SPI peripheral as an alternative SD card interface.
 *      Supports 1-bit mode only. Useful when SDMMC pins are not
 *      available or when connecting an external SPI SD card adapter.
 *
 * The driver initializes the hardware, registers the MMC/SD block driver,
 * and optionally mounts a FAT filesystem at boot.
 *
 * Reference: ESP-IDF examples/storage/sd_card/sdmmc and
 *            examples/storage/sd_card/sdspi
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <syslog.h>
#include <errno.h>
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include <nuttx/board.h>
#include <nuttx/fs/fs.h>
#include <nuttx/mmcsd.h>

#ifdef CONFIG_ESP32P4_EVB_SDCARD_SDMMC
#  include <nuttx/sdio.h>
#endif

#ifdef CONFIG_ESP32P4_EVB_SDCARD_SPI
#  include <nuttx/spi/spi.h>
#endif

#include "esp32p4-evb.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* SD card mount point */

#ifndef CONFIG_ESP32P4_EVB_SDCARD_MOUNTPOINT
#  define CONFIG_ESP32P4_EVB_SDCARD_MOUNTPOINT "/mnt/sdcard"
#endif

/* SD card slot number for block driver registration */

#ifndef CONFIG_MMCSD_NSLOTS
#  define CONFIG_MMCSD_NSLOTS 1
#endif

/* SDMMC bus width */

#ifdef CONFIG_ESP32P4_EVB_SDCARD_SDMMC
#  ifndef CONFIG_ESP32P4_EVB_SDCARD_SDMMC_WIDTH
#    define CONFIG_ESP32P4_EVB_SDCARD_SDMMC_WIDTH 4
#  endif
#endif

/* Default SDMMC GPIO pin assignments for ESP32-P4 EVB
 * These match the dedicated SDMMC Slot 0 IOMUX pins on ESP32-P4.
 * CLK=GPIO43, CMD=GPIO44, D0=GPIO39, D1=GPIO40, D2=GPIO41, D3=GPIO42
 */

#ifdef CONFIG_ESP32P4_EVB_SDCARD_SDMMC
#  ifndef CONFIG_ESP32P4_EVB_SDCARD_SDMMC_CLK_GPIO
#    define CONFIG_ESP32P4_EVB_SDCARD_SDMMC_CLK_GPIO  43
#  endif
#  ifndef CONFIG_ESP32P4_EVB_SDCARD_SDMMC_CMD_GPIO
#    define CONFIG_ESP32P4_EVB_SDCARD_SDMMC_CMD_GPIO  44
#  endif
#  ifndef CONFIG_ESP32P4_EVB_SDCARD_SDMMC_D0_GPIO
#    define CONFIG_ESP32P4_EVB_SDCARD_SDMMC_D0_GPIO   39
#  endif
#  ifndef CONFIG_ESP32P4_EVB_SDCARD_SDMMC_D1_GPIO
#    define CONFIG_ESP32P4_EVB_SDCARD_SDMMC_D1_GPIO   40
#  endif
#  ifndef CONFIG_ESP32P4_EVB_SDCARD_SDMMC_D2_GPIO
#    define CONFIG_ESP32P4_EVB_SDCARD_SDMMC_D2_GPIO   41
#  endif
#  ifndef CONFIG_ESP32P4_EVB_SDCARD_SDMMC_D3_GPIO
#    define CONFIG_ESP32P4_EVB_SDCARD_SDMMC_D3_GPIO   42
#  endif
#endif

/* Default SPI GPIO pin assignments */

#ifdef CONFIG_ESP32P4_EVB_SDCARD_SPI
#  ifndef CONFIG_ESP32P4_EVB_SDCARD_SPI_BUS
#    define CONFIG_ESP32P4_EVB_SDCARD_SPI_BUS  1
#  endif
#  ifndef CONFIG_ESP32P4_EVB_SDCARD_SPI_CS_GPIO
#    define CONFIG_ESP32P4_EVB_SDCARD_SPI_CS_GPIO  14
#  endif
#  ifndef CONFIG_ESP32P4_EVB_SDCARD_SPI_MOSI_GPIO
#    define CONFIG_ESP32P4_EVB_SDCARD_SPI_MOSI_GPIO  12
#  endif
#  ifndef CONFIG_ESP32P4_EVB_SDCARD_SPI_MISO_GPIO
#    define CONFIG_ESP32P4_EVB_SDCARD_SPI_MISO_GPIO  13
#  endif
#  ifndef CONFIG_ESP32P4_EVB_SDCARD_SPI_CLK_GPIO
#    define CONFIG_ESP32P4_EVB_SDCARD_SPI_CLK_GPIO  11
#  endif
#endif

/* Test file path for SD card verification */

#define SDCARD_TEST_FILE  CONFIG_ESP32P4_EVB_SDCARD_MOUNTPOINT "/test.txt"
#define SDCARD_TEST_DATA  "ESP32-P4 SD card test OK\n"
#define SDCARD_READ_BUFSZ 64

/****************************************************************************
 * Private Types
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

#ifdef CONFIG_ESP32P4_EVB_SDCARD_SPI
static FAR struct spi_dev_s *g_spi_dev = NULL;
#endif

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: esp32p4_sdcard_mount
 *
 * Description:
 *   Mount the SD card filesystem.
 *
 *   This function creates the mount point directory if it doesn't exist,
 *   then mounts the SD card block device as a FAT filesystem. This
 *   mirrors the ESP-IDF esp_vfs_fat_sdmmc_mount() pattern but uses
 *   the NuttX VFS mount() system call.
 *
 * Parameters:
 *   mountpoint - The mount point path
 *
 * Return:
 *   Zero (OK) on success; a negated errno on failure.
 *
 ****************************************************************************/

static int esp32p4_sdcard_mount(const char *mountpoint)
{
  int ret;

  /* Create the mount point directory if it doesn't exist */

  ret = mkdir(mountpoint, 0755);
  if (ret < 0 && errno != EEXIST)
    {
      syslog(LOG_ERR, "ERROR: Failed to create mount point %s: %d\n",
             mountpoint, errno);
      return -errno;
    }

  /* Mount the SD card as a FAT filesystem.
   *
   * The block device /dev/mmcsd0 is registered by mmcsd_slotinitialize()
   * or mmcsd_spislotinitialize() during initialization.
   *
   * Try vfat first (FAT32/FAT16), then fall back to fat (FAT12).
   */

  ret = mount("/dev/mmcsd0", mountpoint, "vfat", 0, NULL);
  if (ret < 0)
    {
      syslog(LOG_INFO,
             "INFO: vfat mount failed (%d), trying fat filesystem\n",
             errno);
      ret = mount("/dev/mmcsd0", mountpoint, "fat", 0, NULL);
      if (ret < 0)
        {
          syslog(LOG_ERR,
                 "ERROR: Failed to mount SD card at %s: %d\n",
                 mountpoint, errno);
          return -errno;
        }
    }

  syslog(LOG_INFO, "SD card mounted at %s\n", mountpoint);
  return OK;
}

/****************************************************************************
 * Name: esp32p4_sdcard_test
 *
 * Description:
 *   Perform a basic read/write test on the mounted SD card filesystem.
 *
 *   This function writes a test file to the SD card and reads it back
 *   to verify the filesystem is working correctly. This is similar to
 *   the ESP-IDF example's s_example_write_file/s_example_read_file
 *   pattern.
 *
 * Parameters:
 *   mountpoint - The mount point path
 *
 * Return:
 *   Zero (OK) on success; a negated errno on failure.
 *
 ****************************************************************************/

#ifdef CONFIG_ESP32P4_EVB_SDCARD_DEVOPT
static int esp32p4_sdcard_test(const char *mountpoint)
{
  char readbuf[SDCARD_READ_BUFSZ];
  int fd;
  int ret;
  ssize_t nwritten;
  ssize_t nread;

  /* Write test file */

  syslog(LOG_INFO, "SD card: Writing test file %s\n", SDCARD_TEST_FILE);

  fd = open(SDCARD_TEST_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0666);
  if (fd < 0)
    {
      syslog(LOG_ERR, "ERROR: Failed to open %s for writing: %d\n",
             SDCARD_TEST_FILE, errno);
      return -errno;
    }

  nwritten = write(fd, SDCARD_TEST_DATA, strlen(SDCARD_TEST_DATA));
  close(fd);

  if (nwritten < 0)
    {
      syslog(LOG_ERR, "ERROR: Failed to write test file: %d\n", errno);
      return -errno;
    }

  syslog(LOG_INFO, "SD card: Wrote %zd bytes\n", nwritten);

  /* Read test file back */

  syslog(LOG_INFO, "SD card: Reading test file %s\n", SDCARD_TEST_FILE);

  fd = open(SDCARD_TEST_FILE, O_RDONLY);
  if (fd < 0)
    {
      syslog(LOG_ERR, "ERROR: Failed to open %s for reading: %d\n",
             SDCARD_TEST_FILE, errno);
      return -errno;
    }

  memset(readbuf, 0, sizeof(readbuf));
  nread = read(fd, readbuf, sizeof(readbuf) - 1);
  close(fd);

  if (nread < 0)
    {
      syslog(LOG_ERR, "ERROR: Failed to read test file: %d\n", errno);
      return -errno;
    }

  readbuf[nread] = '\0';
  syslog(LOG_INFO, "SD card: Read %zd bytes: '%s'\n", nread, readbuf);

  /* Verify data integrity */

  if (strcmp(readbuf, SDCARD_TEST_DATA) != 0)
    {
      syslog(LOG_ERR, "ERROR: Data mismatch!\n");
      return -EIO;
    }

  syslog(LOG_INFO, "SD card: Read/write test PASSED\n");
  return OK;
}
#endif /* CONFIG_ESP32P4_EVB_SDCARD_DEVOPT */

/****************************************************************************
 * Name: esp32p4_sdcard_unmount
 *
 * Description:
 *   Unmount the SD card filesystem.
 *
 * Parameters:
 *   mountpoint - The mount point path
 *
 * Return:
 *   Zero (OK) on success; a negated errno on failure.
 *
 ****************************************************************************/

static int esp32p4_sdcard_unmount(const char *mountpoint)
{
  int ret;

  ret = umount(mountpoint);
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: Failed to unmount %s: %d\n",
             mountpoint, errno);
      return -errno;
    }

  syslog(LOG_INFO, "SD card unmounted from %s\n", mountpoint);
  return OK;
}

/****************************************************************************
 * Name: esp32p4_sdcard_sdmmc_init
 *
 * Description:
 *   Initialize the SD card using the SDMMC interface.
 *
 *   This function:
 *   1. Initializes the SDMMC hardware controller via sdio_initialize()
 *   2. Registers the MMC/SD block driver via mmcsd_slotinitialize()
 *
 *   The sdio_initialize() function (implemented in the chip layer)
 *   configures the SDMMC peripheral, sets up GPIO pins for the SD bus
 *   signals (CLK, CMD, D0-D3), enables the SDMMC clock, and performs
 *   a hardware reset. This is analogous to ESP-IDF's sdmmc_host_init()
 *   followed by sdmmc_host_init_slot().
 *
 *   The mmcsd_slotinitialize() function registers the block driver at
 *   /dev/mmcsd0 and starts the card detection and initialization
 *   thread. This is analogous to ESP-IDF's esp_vfs_fat_sdmmc_mount()
 *   which internally calls sdmmc_card_init() and registers the FAT
 *   filesystem.
 *
 * Return:
 *   Zero (OK) on success; a negated errno on failure.
 *
 ****************************************************************************/

#ifdef CONFIG_ESP32P4_EVB_SDCARD_SDMMC
static int esp32p4_sdcard_sdmmc_init(void)
{
  struct sdio_dev_s *sdio;
  int ret;

  syslog(LOG_INFO, "SD card: Initializing SDMMC interface\n");
  syslog(LOG_INFO, "SD card: CLK=GPIO%d CMD=GPIO%d D0=GPIO%d\n",
         CONFIG_ESP32P4_EVB_SDCARD_SDMMC_CLK_GPIO,
         CONFIG_ESP32P4_EVB_SDCARD_SDMMC_CMD_GPIO,
         CONFIG_ESP32P4_EVB_SDCARD_SDMMC_D0_GPIO);

#if CONFIG_ESP32P4_EVB_SDCARD_SDMMC_WIDTH >= 4
  syslog(LOG_INFO, "SD card: D1=GPIO%d D2=GPIO%d D3=GPIO%d (4-bit)\n",
         CONFIG_ESP32P4_EVB_SDCARD_SDMMC_D1_GPIO,
         CONFIG_ESP32P4_EVB_SDCARD_SDMMC_D2_GPIO,
         CONFIG_ESP32P4_EVB_SDCARD_SDMMC_D3_GPIO);
#endif

  /* Initialize the SDMMC hardware and get the SDIO device interface.
   *
   * sdio_initialize() performs:
   *   - SDMMC peripheral clock enable
   *   - Hardware reset
   *   - GPIO pin configuration (IOMUX or GPIO Matrix)
   *   - Initial 400kHz clock setup for card probing
   *   - 1-bit bus width for initial communication
   *
   * This corresponds to ESP-IDF's:
   *   sdmmc_host_init() + sdmmc_host_init_slot()
   */

  sdio = sdio_initialize(0);
  if (sdio == NULL)
    {
      syslog(LOG_ERR, "ERROR: Failed to initialize SDMMC controller\n");
      return -ENODEV;
    }

  syslog(LOG_INFO, "SD card: SDMMC controller initialized\n");

  /* Bind the SDIO interface to the MMC/SD driver.
   *
   * mmcsd_slotinitialize() performs:
   *   - Registers /dev/mmcsd0 block device
   *   - Starts card detection thread
   *   - Sends CMD0 (GO_IDLE_STATE)
   *   - Sends CMD8 (SEND_IF_COND) for SDHC detection
   *   - Sends ACMD41 (SD_SEND_OP_COND) for card initialization
   *   - Reads CID and CSD registers
   *   - Sets block length to 512 bytes
   *   - Switches to high-speed mode if supported
   *   - Sets bus width to 4-bit if configured
   *
   * This corresponds to ESP-IDF's:
   *   sdmmc_card_init() + FAT filesystem registration
   */

  ret = mmcsd_slotinitialize(CONFIG_MMCSD_NSLOTS, sdio);
  if (ret < 0)
    {
      syslog(LOG_ERR,
             "ERROR: Failed to initialize MMC/SD slot: %d\n", ret);
      return ret;
    }

  syslog(LOG_INFO, "SD card: Block driver registered at /dev/mmcsd%d\n",
         CONFIG_MMCSD_NSLOTS - 1);

  return OK;
}
#endif /* CONFIG_ESP32P4_EVB_SDCARD_SDMMC */

/****************************************************************************
 * Name: esp32p4_sdcard_spi_init
 *
 * Description:
 *   Initialize the SD card using the SPI interface.
 *
 *   This function:
 *   1. Initializes the SPI bus via spibus_initialize()
 *   2. Registers the MMC/SD block driver via mmcsd_spislotinitialize()
 *
 *   The SPI interface is an alternative to SDMMC when the SD card is
 *   connected via SPI. This is analogous to ESP-IDF's
 *   spi_bus_initialize() + esp_vfs_fat_sdspi_mount() pattern.
 *
 *   SPI mode supports only 1-bit data transfer and is limited to
 *   20 MHz clock frequency. It requires 4 GPIO pins: MOSI, MISO,
 *   CLK, and CS.
 *
 * Return:
 *   Zero (OK) on success; a negated errno on failure.
 *
 ****************************************************************************/

#ifdef CONFIG_ESP32P4_EVB_SDCARD_SPI
static int esp32p4_sdcard_spi_init(void)
{
  int ret;

  syslog(LOG_INFO, "SD card: Initializing SPI interface\n");
  syslog(LOG_INFO, "SD card: SPI%d MOSI=GPIO%d MISO=GPIO%d "
         "CLK=GPIO%d CS=GPIO%d\n",
         CONFIG_ESP32P4_EVB_SDCARD_SPI_BUS,
         CONFIG_ESP32P4_EVB_SDCARD_SPI_MOSI_GPIO,
         CONFIG_ESP32P4_EVB_SDCARD_SPI_MISO_GPIO,
         CONFIG_ESP32P4_EVB_SDCARD_SPI_CLK_GPIO,
         CONFIG_ESP32P4_EVB_SDCARD_SPI_CS_GPIO);

  /* Initialize the SPI bus.
   *
   * spibus_initialize() performs:
   *   - SPI peripheral clock enable
   *   - GPIO pin configuration for MOSI, MISO, CLK
   *   - SPI controller reset
   *   - Default SPI mode configuration
   *
   * This corresponds to ESP-IDF's spi_bus_initialize().
   */

  g_spi_dev = spibus_initialize(CONFIG_ESP32P4_EVB_SDCARD_SPI_BUS);
  if (g_spi_dev == NULL)
    {
      syslog(LOG_ERR, "ERROR: Failed to initialize SPI bus %d\n",
             CONFIG_ESP32P4_EVB_SDCARD_SPI_BUS);
      return -ENODEV;
    }

  syslog(LOG_INFO, "SD card: SPI bus %d initialized\n",
         CONFIG_ESP32P4_EVB_SDCARD_SPI_BUS);

  /* Bind the SPI interface to the MMC/SD driver.
   *
   * mmcsd_spislotinitialize() performs:
   *   - Registers /dev/mmcsd0 block device
   *   - Configures SPI mode 0 (CPOL=0, CPHA=0) for SD card
   *   - Sets SPI frequency to 400kHz for card probing
   *   - Sends CMD0 (GO_IDLE_STATE) with CS asserted
   *   - Sends CMD8 (SEND_IF_COND) for SDHC detection
   *   - Sends ACMD41 (SD_SEND_OP_COND) for card initialization
   *   - Reads CID and CSD registers
   *   - Sets block length to 512 bytes
   *   - Increases SPI frequency for data transfer
   *
   * This corresponds to ESP-IDF's:
   *   SDSPI_HOST_DEFAULT() + esp_vfs_fat_sdspi_mount()
   */

  ret = mmcsd_spislotinitialize(CONFIG_MMCSD_NSLOTS, 0, g_spi_dev);
  if (ret < 0)
    {
      syslog(LOG_ERR,
             "ERROR: Failed to initialize MMC/SD SPI slot: %d\n", ret);
      return ret;
    }

  syslog(LOG_INFO, "SD card: SPI block driver registered at "
         "/dev/mmcsd%d\n", CONFIG_MMCSD_NSLOTS - 1);

  return OK;
}
#endif /* CONFIG_ESP32P4_EVB_SDCARD_SPI */

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: esp32p4_sdcard_initialize
 *
 * Description:
 *   Initialize the SD card interface and optionally mount the filesystem.
 *
 *   This is the main entry point for SD card initialization. It:
 *   1. Initializes the SDMMC or SPI controller (based on Kconfig)
 *   2. Registers the MMC/SD block driver at /dev/mmcsd0
 *   3. Optionally mounts the FAT filesystem (if AUTOMOUNT is enabled)
 *   4. Optionally runs a read/write test (if DEVOPT is enabled)
 *
 *   The initialization flow mirrors the ESP-IDF pattern:
 *   - SDMMC: sdmmc_host_init() -> sdmmc_host_init_slot() ->
 *            esp_vfs_fat_sdmmc_mount()
 *   - SPI:   spi_bus_initialize() -> esp_vfs_fat_sdspi_mount()
 *
 *   In NuttX terms:
 *   - SDMMC: sdio_initialize() -> mmcsd_slotinitialize() -> mount()
 *   - SPI:   spibus_initialize() -> mmcsd_spislotinitialize() -> mount()
 *
 * Return:
 *   Zero (OK) on success; a negated errno on failure.
 *
 ****************************************************************************/

int esp32p4_sdcard_initialize(void)
{
  int ret;

  syslog(LOG_INFO, "SD card: Initializing SD card interface\n");

#ifdef CONFIG_ESP32P4_EVB_SDCARD_SDMMC
  /* Initialize using SDMMC interface */

  ret = esp32p4_sdcard_sdmmc_init();
#elif defined(CONFIG_ESP32P4_EVB_SDCARD_SPI)
  /* Initialize using SPI interface */

  ret = esp32p4_sdcard_spi_init();
#else
  syslog(LOG_ERR, "ERROR: No SD card interface selected\n");
  return -ENODEV;
#endif

  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: SD card interface init failed: %d\n", ret);
      return ret;
    }

#ifdef CONFIG_ESP32P4_EVB_SDCARD_AUTOMOUNT
  /* Auto-mount the SD card filesystem.
   *
   * The mount may fail if no card is inserted. This is not treated
   * as a fatal error since the card may be inserted later.
   * This mirrors ESP-IDF's behavior where mount failure is handled
   * gracefully with an error message.
   */

  ret = esp32p4_sdcard_mount(CONFIG_ESP32P4_EVB_SDCARD_MOUNTPOINT);
  if (ret < 0)
    {
      syslog(LOG_WARNING,
             "WARNING: SD card mount failed (card may not be present): "
             "%d\n", ret);
      /* Don't return error - SD card may not be inserted */
    }
#  ifdef CONFIG_ESP32P4_EVB_SDCARD_DEVOPT
  else
    {
      /* Run a basic read/write test to verify the filesystem */

      ret = esp32p4_sdcard_test(
              CONFIG_ESP32P4_EVB_SDCARD_MOUNTPOINT);
      if (ret < 0)
        {
          syslog(LOG_WARNING,
                 "WARNING: SD card test failed: %d\n", ret);
          /* Don't return error - test failure is not fatal */
        }
    }
#  endif
#endif

  return OK;
}

/****************************************************************************
 * Name: esp32p4_sdcard_uninitialize
 *
 * Description:
 *   Unmount the SD card filesystem and release resources.
 *
 *   This function:
 *   1. Unmounts the FAT filesystem
 *   2. Releases the SPI bus (if SPI interface is used)
 *
 *   This is the counterpart to esp32p4_sdcard_initialize() and should
 *   be called when the SD card is no longer needed (e.g., before
 *   entering low-power mode).
 *
 * Return:
 *   Zero (OK) on success; a negated errno on failure.
 *
 ****************************************************************************/

int esp32p4_sdcard_uninitialize(void)
{
  int ret;

  syslog(LOG_INFO, "SD card: Uninitializing SD card\n");

  /* Unmount the filesystem first */

  ret = esp32p4_sdcard_unmount(
          CONFIG_ESP32P4_EVB_SDCARD_MOUNTPOINT);
  if (ret < 0)
    {
      syslog(LOG_WARNING,
             "WARNING: SD card unmount failed: %d\n", ret);
    }

#ifdef CONFIG_ESP32P4_EVB_SDCARD_SPI
  /* Release the SPI bus.
   *
   * This corresponds to ESP-IDF's spi_bus_free().
   * The SPI bus should only be freed after all devices on it
   * have been removed.
   */

  if (g_spi_dev != NULL)
    {
      SPIBUS_FREE(g_spi_dev);
      g_spi_dev = NULL;
      syslog(LOG_INFO, "SD card: SPI bus released\n");
    }
#endif

  return OK;
}

/****************************************************************************
 * Name: esp32p4_sdcard_mountfs
 *
 * Description:
 *   Mount the SD card filesystem. This can be called after the SD card
 *   has been initialized without auto-mount, or after a previous
 *   unmount.
 *
 * Return:
 *   Zero (OK) on success; a negated errno on failure.
 *
 ****************************************************************************/

int esp32p4_sdcard_mountfs(void)
{
  return esp32p4_sdcard_mount(
           CONFIG_ESP32P4_EVB_SDCARD_MOUNTPOINT);
}
