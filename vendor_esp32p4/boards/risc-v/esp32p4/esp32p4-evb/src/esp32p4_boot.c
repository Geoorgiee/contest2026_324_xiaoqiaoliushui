/****************************************************************************
 * vendor_esp32p4/boards/risc-v/esp32p4/esp32p4-evb/src/esp32p4_boot.c
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

#include <stdint.h>
#include <debug.h>

#include <nuttx/board.h>
#include <arch/board/board.h>

#include "riscv_internal.h"
#include "esp32p4-evb.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* ESP32-P4 EVB Boot Configuration
 *
 * The ESP32-P4 HP Core boots from the boot ROM, which:
 *   1. Loads the flash bootloader
 *   2. Configures basic clocks (40 MHz XTAL)
 *   3. Sets up the cache subsystem
 *   4. Jumps to the application entry point
 *
 * The application entry point (__start in esp_head.S) then:
 *   1. Sets up the initial stack
 *   2. Clears BSS
 *   3. Copies data from flash to SRAM
 *   4. Calls __espressif_start() (in esp32p4_start.c)
 *
 * This file provides board-level boot helper functions that are
 * called during the early boot sequence, before the full OS is
 * initialized.
 */

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: esp32p4_board_gpio_init
 *
 * Description:
 *   Initialize board-specific GPIO configuration.
 *   This includes setting up the LED GPIO as output and configuring
 *   the boot button GPIO as input.
 *
 ****************************************************************************/

static void esp32p4_board_gpio_init(void)
{
#ifdef CONFIG_ESP32P4_GPIO
  /* Configure the on-board LED GPIO as output.
   * The LED is active-high on the ESP32-P4 EVB.
   * GPIO 0 is used as the LED pin (adjust for actual board).
   */

  /* esp32p4_config_gpio(BOARD_LED1_GPIO, GPIO_OUTPUT);
   * esp32p4_gpio_write(BOARD_LED1_GPIO, false);
   */

  /* Configure the boot button GPIO as input.
   * The boot button is active-low (pressed = low).
   * GPIO 0 is used as the button pin (adjust for actual board).
   */

  /* esp32p4_config_gpio(BOARD_BUTTON1_GPIO, GPIO_INPUT_PULLUP); */
#endif
}

/****************************************************************************
 * Name: esp32p4_board_console_init
 *
 * Description:
 *   Initialize the board console.  This ensures that the UART used for
 *   the serial console is properly configured early in the boot sequence
 *   so that debug output is available as soon as possible.
 *
 ****************************************************************************/

static void esp32p4_board_console_init(void)
{
  /* The UART0 is configured as the serial console by default.
   * The chip-layer UART driver (esp32p4_serial.c) handles the full
   * UART initialization during riscv_earlyserialinit() and
   * riscv_serialinit().
   *
   * For early debug output, the boot ROM has already configured
   * UART0 at 115200 baud, so up_lowputc() works immediately.
   *
   * No additional board-level console initialization is needed here.
   */
}

/****************************************************************************
 * Name: esp32p4_board_periph_init
 *
 * Description:
 *   Initialize board-specific peripheral configuration.
 *   This includes enabling power domains, configuring IO MUX, and
 *   any other board-level peripheral setup needed before the chip
 *   drivers can be used.
 *
 ****************************************************************************/

static void esp32p4_board_periph_init(void)
{
  /* Enable power domains for peripherals used on this board.
   *
   * ESP32-P4 has multiple power domains:
   *   - HP (High Performance) domain: CPU, HP SRAM, peripherals
   *   - LP (Low Power) domain: LP core, LP SRAM, RTC peripherals
   *
   * The HP domain is already powered on by the boot ROM.
   * Additional power domain configuration may be needed for
   * specific peripherals (USB, MIPI, etc.).
   */

  /* Configure IO MUX for UART0 pins.
   *
   * ESP32-P4 UART0 default pins:
   *   TX: GPIO 11 (IO MUX function 0)
   *   RX: GPIO 12 (IO MUX function 0)
   *
   * These are configured by the boot ROM, but we may need to
   * reconfigure if the board uses different pins.
   */

  /* Configure IO MUX for other peripheral pins as needed.
   * SPI, I2C, GPIO pin configurations are board-specific and
   * should be configured here.
   */
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: esp32p4_board_initialize
 *
 * Description:
 *   Board-level initialization function called early in the boot sequence,
 *   before board_late_initialize().  This is the board-level equivalent of
 *   the chip-level esp32p4_init_peripherals() function.
 *
 *   This function is responsible for:
 *   - Board-specific GPIO configuration (LEDs, buttons)
 *   - Board-specific peripheral pin configuration (IO MUX)
 *   - Board-specific power domain configuration
 *   - Console initialization
 *
 *   It is called from board_early_initialize() when
 *   CONFIG_BOARD_EARLY_INITIALIZE is enabled.
 *
 ****************************************************************************/

void esp32p4_board_initialize(void)
{
  /* Initialize board-specific peripheral pin configuration */

  esp32p4_board_periph_init();

  /* Initialize the console early for debug output */

  esp32p4_board_console_init();

  /* Initialize board-specific GPIO (LEDs, buttons) */

  esp32p4_board_gpio_init();
}
