/****************************************************************************
 * vendor/espressif/chips/esp32p4/include/esp32p4_gpio.h
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

#ifndef __VENDOR_ESPRESSIF_CHIPS_ESP32P4_INCLUDE_ESP32P4_GPIO_H
#define __VENDOR_ESPRESSIF_CHIPS_ESP32P4_INCLUDE_ESP32P4_GPIO_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdbool.h>

#ifdef CONFIG_DEV_GPIO
#  include <nuttx/ioexpander/gpio.h>
#endif

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Re-export GPIO mode constants from the hardware header for convenience */

#define GPIO_OUTPUT           1
#define GPIO_INPUT            0
#define GPIO_INPUT_PULLUP     2
#define GPIO_INPUT_PULLDOWN   3

/* Re-export interrupt trigger type constants */

#define GPIO_INTR_DISABLE     0
#define GPIO_INTR_POSEDGE     1
#define GPIO_INTR_NEGEDGE     2
#define GPIO_INTR_ANYEDGE     3
#define GPIO_INTR_LOW_LEVEL   4
#define GPIO_INTR_HIGH_LEVEL  5

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifdef __cplusplus
#define EXTERN extern "C"
extern "C"
{
#else
#define EXTERN extern
#endif

/****************************************************************************
 * Name: esp32p4_gpio_init
 *
 * Description:
 *   Initialize the GPIO driver.  This attaches the GPIO interrupt
 *   handler to the PLIC and should be called during board bring-up.
 *
 * Returned Value:
 *   OK on success; A negated errno value on failure.
 *
 ****************************************************************************/

int esp32p4_gpio_init(void);

/****************************************************************************
 * Name: esp32p4_config_gpio
 *
 * Description:
 *   Configure a GPIO pin for input or output mode, with optional
 *   pull-up/pull-down resistors and interrupt trigger type.
 *
 * Input Parameters:
 *   gpio - GPIO number (0 to GPIO_NUM_MAX-1)
 *   mode - GPIO_OUTPUT, GPIO_INPUT, GPIO_INPUT_PULLUP,
 *          GPIO_INPUT_PULLDOWN, or one of the GPIO_INTR_* values.
 *
 * Returned Value:
 *   OK on success; A negated errno value on failure.
 *
 ****************************************************************************/

int esp32p4_config_gpio(int gpio, int mode);

/****************************************************************************
 * Name: esp32p4_gpio_write
 *
 * Description:
 *   Write a value to a GPIO pin.
 *
 * Input Parameters:
 *   gpio  - GPIO number (0 to GPIO_NUM_MAX-1)
 *   value - true for high, false for low
 *
 ****************************************************************************/

void esp32p4_gpio_write(int gpio, bool value);

/****************************************************************************
 * Name: esp32p4_gpio_read
 *
 * Description:
 *   Read the current value of a GPIO pin.
 *
 * Input Parameters:
 *   gpio - GPIO number (0 to GPIO_NUM_MAX-1)
 *
 * Returned Value:
 *   true if the pin is high, false if low.
 *
 ****************************************************************************/

bool esp32p4_gpio_read(int gpio);

/****************************************************************************
 * Name: esp32p4_gpio_set_irq
 *
 * Description:
 *   Set up an interrupt handler for a GPIO pin.
 *
 * Input Parameters:
 *   gpio    - GPIO number
 *   handler - Interrupt handler (xcpt_t signature)
 *   arg     - Handler argument
 *
 * Returned Value:
 *   OK on success; A negated errno value on failure.
 *
 ****************************************************************************/

int esp32p4_gpio_set_irq(int gpio, xcpt_t handler, void *arg);

/****************************************************************************
 * Name: esp32p4_gpio_register
 *
 * Description:
 *   Register a GPIO pin as a NuttX GPIO character device at
 *   /dev/gpioN.  This allows user-space access via the standard
 *   GPIO ioctl interface.
 *
 *   Only available when CONFIG_DEV_GPIO is enabled.
 *
 * Input Parameters:
 *   gpio    - GPIO number (0 to GPIO_NUM_MAX-1)
 *   pintype - Initial pin type (see enum gpio_pintype_e)
 *   minor   - Device minor number (e.g. 0 -> /dev/gpio0)
 *
 * Returned Value:
 *   OK on success; A negated errno value on failure.
 *
 ****************************************************************************/

#ifdef CONFIG_DEV_GPIO
int esp32p4_gpio_register(int gpio, enum gpio_pintype_e pintype,
                           int minor);
#endif

#undef EXTERN
#ifdef __cplusplus
}
#endif

#endif /* __VENDOR_ESPRESSIF_CHIPS_ESP32P4_INCLUDE_ESP32P4_GPIO_H */
