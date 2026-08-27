/****************************************************************************
 * vendor/espressif/chips/esp32p4/include/hardware/esp32p4_clock.h
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

#ifndef __VENDOR_ESPRESSIF_CHIPS_ESP32P4_INCLUDE_HARDWARE_ESP32P4_CLOCK_H
#define __VENDOR_ESPRESSIF_CHIPS_ESP32P4_INCLUDE_HARDWARE_ESP32P4_CLOCK_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "esp32p4_soc.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Clock and Reset Controller Base */

#define CLKRST_BASE                 DR_REG_CLKRST_BASE

/* System Clock Configuration Registers */

#define CLKRST_SOC_CLK_CTRL0_REG   (CLKRST_BASE + 0x0000)
#define CLKRST_SOC_CLK_CTRL1_REG   (CLKRST_BASE + 0x0004)
#define CLKRST_SOC_CLK_CTRL2_REG   (CLKRST_BASE + 0x0008)
#define CLKRST_CPU_PERIP_CLK_EN0_REG (CLKRST_BASE + 0x0010)
#define CLKRST_CPU_PERIP_CLK_EN1_REG (CLKRST_BASE + 0x0014)
#define CLKRST_HP_PERIP_CLK_EN_REG  (CLKRST_BASE + 0x001C)

/* PLL Configuration Registers */

#define CLKRST_PLL_CTRL0_REG       (CLKRST_BASE + 0x0020)
#define CLKRST_PLL_CTRL1_REG       (CLKRST_BASE + 0x0024)
#define CLKRST_PLL_CTRL2_REG       (CLKRST_BASE + 0x0028)
#define CLKRST_PLL_CTRL3_REG       (CLKRST_BASE + 0x002C)
#define CLKRST_PLL_CONFIG_REG      (CLKRST_BASE + 0x0030)

/* PLL Configuration Bits
 *
 * PLL frequency = XTAL_FREQ * (SDM_DENOM + SDM_DUMMY) / SDM_DENOM
 *
 * For 480 MHz from 40 MHz XTAL:
 *   Feedback multiplier = 12
 *   SDM_DUMMY = 0, SDM_DENOM = 1 (integer mode)
 */

#define CLKRST_PLL_EN              (1 << 0)
#define CLKRST_PLL_LOCK_DETECT_EN  (1 << 1)
#define CLKRST_PLL_LOCK_DETECT_RST (1 << 2)
#define CLKRST_PLL_DIV_REF_S      4
#define CLKRST_PLL_DIV_REF_M      (0x3f << CLKRST_PLL_DIV_REF_S)
#define CLKRST_PLL_DIV_FB_S       10
#define CLKRST_PLL_DIV_FB_M       (0xff << CLKRST_PLL_DIV_FB_S)
#define CLKRST_PLL_OUTPUT_DIV_S   18
#define CLKRST_PLL_OUTPUT_DIV_M   (0x3 << CLKRST_PLL_OUTPUT_DIV_S)

/* PLL Status Register */

#define CLKRST_PLL_STATUS_REG      (CLKRST_BASE + 0x0034)
#define CLKRST_PLL_LOCKED          (1 << 0)

/* CPU Clock Source Selection */

#define CLKRST_CPU_CLK_SRC_S       0
#define CLKRST_CPU_CLK_SRC_M       (0x7 << CLKRST_CPU_CLK_SRC_S)
#define CLKRST_CPU_CLK_SRC_XTAL    (0 << CLKRST_CPU_CLK_SRC_S)
#define CLKRST_CPU_CLK_SRC_PLL     (1 << CLKRST_CPU_CLK_SRC_S)
#define CLKRST_CPU_CLK_SRC_8M      (2 << CLKRST_CPU_CLK_SRC_S)

/* CPU Frequency Divider
 *
 * CPU_CLK = PLL_FREQ / CPU_CLK_DIV
 * For 400 MHz: 480 MHz / 1.2 (use integer divider 1 or fractional mode)
 * APB_CLK = PLL_FREQ / APB_CLK_DIV
 * For 80 MHz: 480 MHz / 6
 */

#define CLKRST_CPU_CLK_DIV_S       4
#define CLKRST_CPU_CLK_DIV_M       (0xff << CLKRST_CPU_CLK_DIV_S)


/* APB Clock Divider */

#define CLKRST_APB_CLK_DIV_S       12
#define CLKRST_APB_CLK_DIV_M       (0xff << CLKRST_APB_CLK_DIV_S)

/* Peripheral Clock Enable Bits (UART) */

#define CLKRST_UART0_CLK_EN        (1 << 0)
#define CLKRST_UART1_CLK_EN        (1 << 1)

/* Peripheral Clock Enable Bits (Timer) */

#define CLKRST_TIMER0_CLK_EN       (1 << 0)
#define CLKRST_TIMER1_CLK_EN       (1 << 1)

/* Peripheral Clock Enable Bits (GPIO) */

#define CLKRST_GPIO_CLK_EN         (1 << 0)

/* Reset Control Registers */

#define CLKRST_HP_RST_EN0_REG      (CLKRST_BASE + 0x0040)
#define CLKRST_HP_RST_EN1_REG      (CLKRST_BASE + 0x0044)

/* Crystal Oscillator Frequency */

#define RTC_XTAL_FREQ_40M          40000000

/* Default APB clock frequency */

#define APB_CLK_FREQ_DEFAULT       80000000   /* 80 MHz */

#endif /* __VENDOR_ESPRESSIF_CHIPS_ESP32P4_INCLUDE_HARDWARE_ESP32P4_CLOCK_H */
