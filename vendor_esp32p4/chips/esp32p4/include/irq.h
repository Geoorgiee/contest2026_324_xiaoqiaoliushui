/****************************************************************************
 * vendor/espressif/chip/esp32p4/include/irq.h
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

/* This file should never be included directly but, rather, only indirectly
 * through nuttx/irq.h
 */

#ifndef __VENDOR_ESPRESSIF_CHIP_ESP32P4_INCLUDE_IRQ_H
#define __VENDOR_ESPRESSIF_CHIP_ESP32P4_INCLUDE_IRQ_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <sys/types.h>
#include <arch/chip/irq.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* ESP32-P4 RISC-V PLIC (Platform-Level Interrupt Controller) */

#define ESP32P4_IRQ_FIRST       16  /* Vector number of the first interrupt */

/* IRQ numbers for ESP32-P4 peripherals */

#define ESP32P4_IRQ_UART0       31  /* UART0 interrupt */
#define ESP32P4_IRQ_UART1       32  /* UART1 interrupt */
#define ESP32P4_IRQ_SPI0        33  /* SPI0 interrupt */
#define ESP32P4_IRQ_SPI1        34  /* SPI1 interrupt */
#define ESP32P4_IRQ_I2C0        35  /* I2C0 interrupt */
#define ESP32P4_IRQ_I2C1        36  /* I2C1 interrupt */
#define ESP32P4_IRQ_USB         37  /* USB OTG interrupt */
#define ESP32P4_IRQ_GPIO0       38  /* GPIO interrupt */
#define ESP32P4_IRQ_TIMER0      39  /* Timer0 interrupt */
#define ESP32P4_IRQ_TIMER1      40  /* Timer1 interrupt */

#define NR_IRQS                 64  /* Total number of IRQs */

/****************************************************************************
 * Public Types
 ****************************************************************************/

/****************************************************************************
 * Public Data
 ****************************************************************************/

#undef EXTERN
#if defined(__cplusplus)
#define EXTERN extern "C"
extern "C"
{
#else
#define EXTERN extern
#endif

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#undef EXTERN
#if defined(__cplusplus)
}
#endif

#endif /* __VENDOR_ESPRESSIF_CHIP_ESP32P4_INCLUDE_IRQ_H */
