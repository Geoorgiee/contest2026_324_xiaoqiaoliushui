/****************************************************************************
 * vendor/espressif/chips/esp32p4/include/hardware/esp32p4_soc.h
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

#ifndef __VENDOR_ESPRESSIF_CHIPS_ESP32P4_INCLUDE_HARDWARE_ESP32P4_SOC_H
#define __VENDOR_ESPRESSIF_CHIPS_ESP32P4_INCLUDE_HARDWARE_ESP32P4_SOC_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/irq.h>
#include <nuttx/spinlock.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* ESP32-P4 Peripheral Base Addresses */

#define DR_REG_HP_PERI_BASE         0x60000000
#define DR_REG_LP_PERI_BASE         0x600B0000

/* UART Peripheral Base Addresses */

#define DR_REG_UART0_BASE           0x60000000
#define DR_REG_UART1_BASE           0x60001000

/* GPIO Peripheral Base Addresses */

#define DR_REG_GPIO_BASE            0x60091000
#define DR_REG_GPIO_SD_BASE         0x60091f00

/* PLIC / Interrupt Matrix Base Address */

#define DR_REG_PLIC_BASE            0x60009000
#define DR_REG_PLIC_MX_BASE         0x60009000

/* Timer Peripheral Base Addresses */

#define DR_REG_TIMG0_BASE           0x6001A000
#define DR_REG_TIMG1_BASE           0x6001B000

/* System Timer Base Address */

#define DR_REG_SYSTIMER_BASE        0x60017000

/* Clock and Reset Controller */

#define DR_REG_CLKRST_BASE          0x60008000

/* SPI Peripheral Base Addresses
 *
 * SPI0/SPI1 are MSPI (Memory SPI) controllers used for flash/PSRAM.
 * SPI2/SPI3 are GP-SPI (General Purpose SPI) controllers for external
 * SPI devices.
 */

#define DR_REG_SPI0_BASE            0x60003000
#define DR_REG_SPI1_BASE            0x60004000
#define DR_REG_MSPI_BASE            0x60003000
#define DR_REG_SPI2_BASE            0x60015000
#define DR_REG_SPI3_BASE            0x60016000

/* I2C Peripheral Base Addresses */

#define DR_REG_I2C0_BASE            0x60013000
#define DR_REG_I2C1_BASE            0x60014000

/* USB Peripheral Base Address */

#define DR_REG_USB_BASE             0x60080000

/* DMA Peripheral Base Address */

#define DR_REG_GDMA_BASE            0x60084000

/* MIPI Peripheral Base Addresses */

#define DR_REG_MIPI_DSI_BASE        0x60086000
#define DR_REG_MIPI_CSI_BASE        0x60088000

/* Memory Map */

#define SOC_IRAM_LOW                0x4ff00000
#define SOC_IRAM_HIGH               0x4ffbffff
#define SOC_IROM_LOW                0x42000000
#define SOC_IROM_HIGH               0x45ffffff
#define SOC_DRAM_LOW                0x4ff00000
#define SOC_DRAM_HIGH               0x4ffbffff

/* Cache Line Size */

#define CACHE_LINE_SIZE             64

/****************************************************************************
 * Register Access Macros
 ****************************************************************************/

#define REG_READ(addr)              (*(volatile uint32_t *)(addr))
#define REG_WRITE(addr, val)        (*(volatile uint32_t *)(addr) = (val))

/* REG_SET_BIT and REG_CLR_BIT perform non-atomic read-modify-write
 * operations.  They must be wrapped with critical sections to prevent
 * race conditions when called from multiple threads or interrupt context.
 *
 * NOTE: For GPIO registers, prefer using the W1TS/W1TC registers
 * (e.g. GPIO_OUT_W1TS_REG / GPIO_OUT_W1TC_REG) which are hardware-
 * level atomic and do not need interrupt protection.
 */

#define REG_SET_BIT(addr, bit) \
  do { \
    irqstate_t __flags = enter_critical_section(); \
    REG_WRITE((addr), REG_READ(addr) | (bit)); \
    leave_critical_section(__flags); \
  } while (0)

#define REG_CLR_BIT(addr, bit) \
  do { \
    irqstate_t __flags = enter_critical_section(); \
    REG_WRITE((addr), REG_READ(addr) & ~(bit)); \
    leave_critical_section(__flags); \
  } while (0)

/****************************************************************************
 * Inline Helper Functions
 ****************************************************************************/

/* modifyreg32 is provided by NuttX common code (riscv_internal.h).
 * The vendor header previously defined a static inline version with
 * critical-section wrapping, but this conflicts with the extern
 * declaration in riscv_internal.h.  Use the NuttX-provided version.
 */

#endif /* __VENDOR_ESPRESSIF_CHIPS_ESP32P4_INCLUDE_HARDWARE_ESP32P4_SOC_H */
