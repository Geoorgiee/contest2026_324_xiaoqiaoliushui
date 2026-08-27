/****************************************************************************
 * vendor_esp32p4/boards/risc-v/esp32p4/esp32p4-evb/src/esp32p4-evb.h
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

#ifndef __VENDOR_ESP32P4_BOARDS_RISCV_ESP32P4_ESP32P4_EVB_SRC_ESP32P4_EVB_H
#define __VENDOR_ESP32P4_BOARDS_RISCV_ESP32P4_ESP32P4_EVB_SRC_ESP32P4_EVB_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/compiler.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Debug ********************************************************************/

/* LED definitions **********************************************************/

/* The ESP32-P4 EVB has a single LED connected to GPIO 26.
 * GPIO 0 is used by the SPI flash and must not be used for LED.
 * Active high: setting GPIO high turns the LED on.
 */

#define LED_STARTED       0  /* LED is on after OS starts */
#define LED_HEAPALLOCATE  0  /* LED toggles on heap allocation */
#define LED_IRQSENABLED   0  /* LED toggles when IRQs enabled */
#define LED_STACKCREATED  0  /* LED toggles after stack creation */
#define LED_INIRQ         0  /* LED is on while in an interrupt */
#define LED_SIGNAL        0  /* LED toggles on signal */
#define LED_ASSERTION     0  /* LED toggles on assertion */
#define LED_PANIC         0  /* LED is on during panic */

/* Button definitions *******************************************************/

/* The ESP32-P4 EVB has a BOOT button connected to GPIO 21 (active low) */

#define BUTTON_BOOT       21
#define NUM_BUTTONS       1
#define BUTTON_BOOT_BIT   (1 << BUTTON_BOOT)

/****************************************************************************
 * Public Types
 ****************************************************************************/

/****************************************************************************
 * Public Data
 ****************************************************************************/

#ifndef __ASSEMBLY__

/****************************************************************************
 * Public Function Prototypes
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
 ****************************************************************************/

int esp32p4_bringup(void);

/****************************************************************************
 * Name: esp32p4_sdcard_initialize
 *
 * Description:
 *   Initialize the SD card interface and optionally mount the filesystem.
 *
 *   This function initializes the SDMMC controller, detects the SD card,
 *   registers the MMC/SD block driver, and optionally mounts the FAT
 *   filesystem.
 *
 ****************************************************************************/

#ifdef CONFIG_ESP32P4_EVB_SDCARD
int esp32p4_sdcard_initialize(void);
int esp32p4_sdcard_uninitialize(void);
int esp32p4_sdcard_mountfs(void);
#endif

/****************************************************************************
 * Name: esp32p4_lcd_initialize
 *
 * Description:
 *   Initialize the MIPI-DSI LCD display.
 *
 *   This function initializes the MIPI DSI bus, PHY, and host controller,
 *   configures the LCD panel based on Kconfig settings, and registers
 *   the framebuffer device at /dev/fb0.
 *
 ****************************************************************************/

#ifdef CONFIG_ESP32P4_LCD_PANEL
int esp32p4_lcd_initialize(void);
#endif

/****************************************************************************
 * Name: esp32p4_board_initialize
 *
 * Description:
 *   Board-level initialization function called early in the boot sequence.
 *   Configures board-specific GPIO, IO MUX, and peripheral power domains.
 *   Called from board_early_initialize() when CONFIG_BOARD_EARLY_INITIALIZE
 *   is enabled.
 *
 ****************************************************************************/

void esp32p4_board_initialize(void);

#endif /* __ASSEMBLY__ */
#endif /* __VENDOR_ESP32P4_BOARDS_RISCV_ESP32P4_ESP32P4_EVB_SRC_ESP32P4_EVB_H */
