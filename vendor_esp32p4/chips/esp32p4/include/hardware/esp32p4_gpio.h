/****************************************************************************
 * vendor/espressif/chips/esp32p4/include/hardware/esp32p4_gpio.h
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

#ifndef __VENDOR_ESPRESSIF_CHIPS_ESP32P4_INCLUDE_HARDWARE_ESP32P4_GPIO_H
#define __VENDOR_ESPRESSIF_CHIPS_ESP32P4_INCLUDE_HARDWARE_ESP32P4_GPIO_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "esp32p4_soc.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* GPIO Base Address */

#define GPIO_BASE                   DR_REG_GPIO_BASE

/* GPIO Output Registers (32 GPIOs per register, 2 registers for 54 GPIOs) */

#define GPIO_OUT_REG                (GPIO_BASE + 0x0004)
#define GPIO_OUT_W1TS_REG           (GPIO_BASE + 0x0008)
#define GPIO_OUT_W1TC_REG           (GPIO_BASE + 0x000C)
#define GPIO_OUT1_REG               (GPIO_BASE + 0x0010)
#define GPIO_OUT1_W1TS_REG          (GPIO_BASE + 0x0014)
#define GPIO_OUT1_W1TC_REG          (GPIO_BASE + 0x0018)

/* GPIO Output Enable Registers (32 GPIOs per register, 2 registers for 54 GPIOs) */

#define GPIO_ENABLE_REG             (GPIO_BASE + 0x0020)
#define GPIO_ENABLE_W1TS_REG        (GPIO_BASE + 0x0024)
#define GPIO_ENABLE_W1TC_REG        (GPIO_BASE + 0x0028)
#define GPIO_ENABLE1_REG            (GPIO_BASE + 0x002C)
#define GPIO_ENABLE1_W1TS_REG       (GPIO_BASE + 0x0030)
#define GPIO_ENABLE1_W1TC_REG       (GPIO_BASE + 0x0034)

/* GPIO Input Registers */

#define GPIO_IN_REG                 (GPIO_BASE + 0x003C)
#define GPIO_IN1_REG                (GPIO_BASE + 0x0040)

/* GPIO Interrupt Registers */

#define GPIO_STATUS_REG             (GPIO_BASE + 0x0044)
#define GPIO_STATUS_W1TS_REG        (GPIO_BASE + 0x0048)
#define GPIO_STATUS_W1TC_REG        (GPIO_BASE + 0x004C)
#define GPIO_STATUS1_REG            (GPIO_BASE + 0x0050)
#define GPIO_STATUS1_W1TS_REG       (GPIO_BASE + 0x0054)
#define GPIO_STATUS1_W1TC_REG       (GPIO_BASE + 0x0058)

/* GPIO Interrupt Source Registers */

#define GPIO_INTR_0_REG             (GPIO_BASE + 0x005C)
#define GPIO_INTR1_0_REG            (GPIO_BASE + 0x0060)

/* GPIO Pin Configuration Registers (4 bytes per pin, 57 pins total) */

#define GPIO_PIN_REG(n)             (GPIO_BASE + 0x0074 + ((n) * 4))

/* GPIO Pin Register Bits */

#define GPIO_PIN_PAD_DRIVER_BIT     (1 << 2)   /* Open-drain mode */
#define GPIO_PIN_INT_TYPE_S         7
#define GPIO_PIN_INT_TYPE_M         (0x7 << GPIO_PIN_INT_TYPE_S)
#define GPIO_PIN_INT_TYPE_DIS       (0 << GPIO_PIN_INT_TYPE_S)
#define GPIO_PIN_INT_TYPE_POSEDGE   (1 << GPIO_PIN_INT_TYPE_S)
#define GPIO_PIN_INT_TYPE_NEGEDGE   (2 << GPIO_PIN_INT_TYPE_S)
#define GPIO_PIN_INT_TYPE_ANYEDGE   (3 << GPIO_PIN_INT_TYPE_S)
#define GPIO_PIN_INT_TYPE_LOLEVEL   (4 << GPIO_PIN_INT_TYPE_S)
#define GPIO_PIN_INT_TYPE_HILEVEL   (5 << GPIO_PIN_INT_TYPE_S)

/* GPIO Pin Interrupt Enable Bits */

#define GPIO_PIN_INT_ENA_S         10
#define GPIO_PIN_INT_ENA_M         (0x1f << GPIO_PIN_INT_ENA_S)
#define GPIO_PIN_INT_ENA_NONE      (0 << GPIO_PIN_INT_ENA_S)
#define GPIO_PIN_INT_ENA_INTR0     (1 << GPIO_PIN_INT_ENA_S)  /* Enable for GPIO_INTR0 */

/* GPIO Function Selection Register (1 register per pin, 4 bytes each) */

#define GPIO_FUNC_IN_SEL_CFG_REG(n)  (GPIO_BASE + 0x0154 + ((n) * 4))
#define GPIO_FUNC_OUT_SEL_CFG_REG(n) (GPIO_BASE + 0x0554 + ((n) * 4))

/* GPIO Output Function Selection Bits */

#define GPIO_FUNC_OUT_SEL_S         0
#define GPIO_FUNC_OUT_SEL_M         (0x1ff << GPIO_FUNC_OUT_SEL_S)
#define GPIO_FUNC_OEN_SEL           (1 << 11)   /* Use output enable signal */
#define GPIO_FUNC_OEN_INV_SEL       (1 << 12)   /* Invert OE signal */

/* IO Mux Base Address (ESP32-P4) */

#define DR_REG_HPPERIPH1_BASE       0x600c0000
#define IO_MUX_BASE                 (DR_REG_HPPERIPH1_BASE + 0x21000)

/* IO Mux Per-Pin Configuration Register
 * Each GPIO has a 4-byte IO_MUX register starting at IO_MUX_BASE + 0x04.
 * IO_MUX_GPIO_REG(n) = IO_MUX_BASE + 0x04 + (n) * 4
 */

#define IO_MUX_GPIO_REG(n)          (IO_MUX_BASE + 0x04 + ((n) * 4))

/* IO Mux Register Bit Fields */

#define IO_MUX_MCU_SEL_S            12
#define IO_MUX_MCU_SEL_M            (0x7 << IO_MUX_MCU_SEL_S)
#define IO_MUX_FUN_DRV_S            10
#define IO_MUX_FUN_DRV_M            (0x3 << IO_MUX_FUN_DRV_S)
#define IO_MUX_FUN_IE               (1 << 9)    /* Input enable */
#define IO_MUX_FUN_WPU              (1 << 8)    /* Pull-up enable */
#define IO_MUX_FUN_WPD              (1 << 7)    /* Pull-down enable */
#define IO_MUX_FILTER_EN            (1 << 15)   /* Pin filter enable */

/* GPIO function select value (MCU_SEL = 1 for GPIO function) */

#define IO_MUX_GPIO_FUNC            1

/* GPIO Number Definitions */

#define GPIO_NUM_0                  0
#define GPIO_NUM_1                  1
#define GPIO_NUM_2                  2
#define GPIO_NUM_3                  3
#define GPIO_NUM_4                  4
#define GPIO_NUM_5                  5
#define GPIO_NUM_MAX                54

/* GPIO Direction */

#define GPIO_OUTPUT                 1
#define GPIO_INPUT                  0
#define GPIO_INPUT_PULLUP           2
#define GPIO_INPUT_PULLDOWN         3

/* GPIO Interrupt Trigger Types (matches ESP-IDF gpio_int_type_t) */

#define GPIO_INTR_DISABLE           0   /* Disable GPIO interrupt */
#define GPIO_INTR_POSEDGE           1   /* Rising edge trigger */
#define GPIO_INTR_NEGEDGE           2   /* Falling edge trigger */
#define GPIO_INTR_ANYEDGE           3   /* Both rising and falling edge */
#define GPIO_INTR_LOW_LEVEL         4   /* Low level trigger */
#define GPIO_INTR_HIGH_LEVEL        5   /* High level trigger */

/* GPIO Level */

#define GPIO_HIGH                   1
#define GPIO_LOW                    0

#endif /* __VENDOR_ESPRESSIF_CHIPS_ESP32P4_INCLUDE_HARDWARE_ESP32P4_GPIO_H */
