/****************************************************************************
 * vendor/espressif/chips/esp32p4/include/hardware/esp32p4_timer.h
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

#ifndef __VENDOR_ESPRESSIF_CHIPS_ESP32P4_INCLUDE_HARDWARE_ESP32P4_TIMER_H
#define __VENDOR_ESPRESSIF_CHIPS_ESP32P4_INCLUDE_HARDWARE_ESP32P4_TIMER_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "esp32p4_soc.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* HP Timer Group 0 Registers */

#define TIMG0_BASE                  DR_REG_TIMG0_BASE

#define TIMG_T0CONFIG_REG(n)       (DR_REG_TIMG0_BASE + (n) * 0x1000 + 0x0000)
#define TIMG_T0LO_REG(n)           (DR_REG_TIMG0_BASE + (n) * 0x1000 + 0x0004)
#define TIMG_T0HI_REG(n)           (DR_REG_TIMG0_BASE + (n) * 0x1000 + 0x0008)
#define TIMG_T0UPDATE_REG(n)       (DR_REG_TIMG0_BASE + (n) * 0x1000 + 0x000C)
#define TIMG_T0LOAD_LO_REG(n)     (DR_REG_TIMG0_BASE + (n) * 0x1000 + 0x0018)
#define TIMG_T0LOAD_HI_REG(n)     (DR_REG_TIMG0_BASE + (n) * 0x1000 + 0x001C)
#define TIMG_T0LOAD_REG(n)         (DR_REG_TIMG0_BASE + (n) * 0x1000 + 0x0020)

/* Timer Group Interrupt Registers */

#define TIMG_INT_ST_REG(n)         (DR_REG_TIMG0_BASE + (n) * 0x1000 + 0x0060)
#define TIMG_INT_CLR_TIMERS_REG(n) (DR_REG_TIMG0_BASE + (n) * 0x1000 + 0x0064)
#define TIMG_INT_ENA_TIMERS_REG(n) (DR_REG_TIMG0_BASE + (n) * 0x1000 + 0x0068)

/* Watchdog Timer Registers */

#define TIMG_WDTCONFIG0_REG(n)    (DR_REG_TIMG0_BASE + (n) * 0x1000 + 0x0048)
#define TIMG_WDTCONFIG1_REG(n)    (DR_REG_TIMG0_BASE + (n) * 0x1000 + 0x004C)
#define TIMG_WDTCONFIG2_REG(n)    (DR_REG_TIMG0_BASE + (n) * 0x1000 + 0x0050)
#define TIMG_WDTCONFIG3_REG(n)    (DR_REG_TIMG0_BASE + (n) * 0x1000 + 0x0054)
#define TIMG_WDTCONFIG4_REG(n)    (DR_REG_TIMG0_BASE + (n) * 0x1000 + 0x0058)
#define TIMG_WDTCONFIG5_REG(n)    (DR_REG_TIMG0_BASE + (n) * 0x1000 + 0x005C)
#define TIMG_WDTFEED_REG(n)       (DR_REG_TIMG0_BASE + (n) * 0x1000 + 0x0060)

/* Timer Configuration Bits */

#define TIMG_T0_EN                 (1 << 31)   /* Timer enable */
#define TIMG_T0_INCREASE           (1 << 30)   /* 1 = count up, 0 = count down */
#define TIMG_T0_AUTORELOAD         (1 << 29)   /* Auto-reload on alarm */
#define TIMG_T0_DIVIDER_S          13
#define TIMG_T0_DIVIDER_M          (0xffff << TIMG_T0_DIVIDER_S)
#define TIMG_T0_EDGE_INT_EN        (1 << 12)   /* Edge-triggered interrupt */
#define TIMG_T0_LEVEL_INT_EN       (1 << 11)   /* Level-triggered interrupt */
#define TIMG_T0_ALARM_EN           (1 << 10)   /* Alarm enable */

/* Timer Interrupt Bits */

#define TIMG_T0_INT_RAW            (1 << 0)    /* Timer 0 interrupt raw */
#define TIMG_T1_INT_RAW            (1 << 1)    /* Timer 1 interrupt raw */
#define TIMG_WDT_INT_RAW           (1 << 2)    /* Watchdog interrupt raw */

/* Timer clock source: APB clock (80 MHz typically) */

#define TIMG_BASE_CLK_HZ           80000000

#endif /* __VENDOR_ESPRESSIF_CHIPS_ESP32P4_INCLUDE_HARDWARE_ESP32P4_TIMER_H */
