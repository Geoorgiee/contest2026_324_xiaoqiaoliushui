/****************************************************************************
 * vendor/espressif/chips/esp32p4/include/hardware/esp32p4_spi.h
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

#ifndef __VENDOR_ESPRESSIF_CHIPS_ESP32P4_INCLUDE_HARDWARE_ESP32P4_SPI_H
#define __VENDOR_ESPRESSIF_CHIPS_ESP32P4_INCLUDE_HARDWARE_ESP32P4_SPI_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "esp32p4_soc.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* GP-SPI Peripheral Base Addresses
 *
 * ESP32-P4 has 2 general-purpose SPI controllers (SPI2, SPI3) that are
 * distinct from the MSPI (SPI0/SPI1) controllers used for flash/PSRAM.
 * The GP-SPI controllers are used for external SPI devices (sensors,
 * displays, EEPROMs, etc.).
 *
 * Reference: ESP32-P4 Technical Reference Manual, Chapter 24 (GP-SPI)
 */

#define SPI2_BASE                   0x60015000
#define SPI3_BASE                   0x60016000

/* Number of GP-SPI controllers */

#define SPI_NUM_MAX                 2   /* SPI2, SPI3 */

/* SPI FIFO depth (words) */

#define SPI_FIFO_LEN                16  /* 16 x 32-bit words = 64 bytes */

/* --------------------------------------------------------------------------
 * GP-SPI Register Offsets
 * -------------------------------------------------------------------------- */

/* Command register */

#define SPI_CMD_REG                 0x0000

/* Address register */

#define SPI_ADDR_REG                0x0004

/* Control register */

#define SPI_CTRL_REG                0x0008

/* Clock configuration */

#define SPI_CLOCK_REG               0x000C

/* User-defined command configuration */

#define SPI_USER_REG                0x0010

/* User command configuration 1 */

#define SPI_USER1_REG               0x0014

/* User command configuration 2 */

#define SPI_USER2_REG               0x0018

/* Data bit length for current command */

#define SPI_MS_DLEN_REG             0x001C

/* Miscellaneous configuration */

#define SPI_MISC_REG                0x0020

/* Interrupt raw status */

#define SPI_INT_RAW_REG             0x0024

/* Interrupt clear (write-to-clear) */

#define SPI_INT_CLR_REG             0x0028

/* Interrupt enable */

#define SPI_INT_ENA_REG             0x002C

/* Interrupt masked status (read-only) */

#define SPI_INT_ST_REG              0x0030

/* DMA configuration */

#define SPI_DMA_CONF_REG            0x0034

/* DMA TX link configuration */

#define SPI_DMA_OUT_LINK_REG        0x0038

/* DMA RX link configuration */

#define SPI_DMA_IN_LINK_REG         0x003C

/* DMA TX status */

#define SPI_DMA_OUT_STATUS_REG      0x0040

/* DMA RX status */

#define SPI_DMA_IN_STATUS_REG       0x0044

/* DMA TX data configuration */

#define SPI_DMA_OUT_DATA_REG        0x0048

/* DMA RX data configuration */

#define SPI_DMA_IN_DATA_REG         0x004C

/* Write data buffer W0-W15 (16 x 32-bit words) */

#define SPI_W(n)                    (0x0080 + ((n) * 4))

/* Read data buffer R0-R15 (16 x 32-bit words) */

#define SPI_R(n)                    (0x00C0 + ((n) * 4))

/* Slave mode configuration */

#define SPI_SLAVE_REG               0x00E0

/* Slave control register */

#define SPI_SLAVE1_REG              0x00E4

/* Clock gate register */

#define SPI_CLK_GATE_REG            0x00E8

/* Date/version register */

#define SPI_DATE_REG                0x00FC

/* --------------------------------------------------------------------------
 * SPI_CMD_REG Bits
 * -------------------------------------------------------------------------- */

#define SPI_USR                     (1 << 18)  /* Start user-defined command */

/* --------------------------------------------------------------------------
 * SPI_CTRL_REG Bits
 * -------------------------------------------------------------------------- */

#define SPI_WR_BIT_ORDER            (1 << 25)  /* 1=LSB first for TX */
#define SPI_RD_BIT_ORDER            (1 << 26)  /* 1=LSB first for RX */
#define SPI_FREAD_DUAL              (1 << 14)  /* Fast read dual I/O */
#define SPI_FREAD_QUAD              (1 << 13)  /* Fast read quad I/O */
#define SPI_FREAD_OCT               (1 << 12)  /* Fast read octal I/O */
#define SPI_FCMD_DUAL               (1 << 11)  /* Fast command dual */
#define SPI_FCMD_QUAD               (1 << 10)  /* Fast command quad */
#define SPI_FCMD_OCT                (1 << 9)   /* Fast command octal */

/* --------------------------------------------------------------------------
 * SPI_CLOCK_REG Bits
 *
 * Clock divider configuration:
 *   SPI_CLK_EQU_SYSCLK = 1: SPI clock = system clock (no division)
 *   SPI_CLKDIV_PRE [17:12]: Pre-divider (1-16, value+1)
 *   SPI_CLKCNT_N [5:0]: N counter (total period = N+1)
 *   SPI_CLKCNT_H [11:6]: H counter (high phase = H+1)
 *   SPI_CLKCNT_L [17:12]: L counter (low phase = L+1, shares bits with PRE)
 *
 * Actual formula depends on the mode:
 *   If CLK_EQU_SYSCLK=1: SPI_CLK = SYS_CLK
 *   Otherwise: SPI_CLK = CLK_PRE / (CLKCNT_N + 1)
 * -------------------------------------------------------------------------- */

#define SPI_CLK_EQU_SYSCLK          (1 << 31)
#define SPI_CLKDIV_PRE_S            18
#define SPI_CLKDIV_PRE_M            (0x3f << SPI_CLKDIV_PRE_S)
#define SPI_CLKCNT_N_S              0
#define SPI_CLKCNT_N_M              (0x3f << SPI_CLKCNT_N_S)
#define SPI_CLKCNT_H_S              6
#define SPI_CLKCNT_H_M              (0x3f << SPI_CLKCNT_H_S)
#define SPI_CLKCNT_L_S              12
#define SPI_CLKCNT_L_M              (0x3f << SPI_CLKCNT_L_S)

/* --------------------------------------------------------------------------
 * SPI_USER_REG Bits
 * -------------------------------------------------------------------------- */

#define SPI_DOUTDIN                 (1 << 0)   /* Full-duplex mode */
#define SPI_CS_HOLD                 (1 << 5)   /* CS hold time */
#define SPI_CS_SETUP                (1 << 6)   /* CS setup time */
#define SPI_CK_I_EDGE               (1 << 7)   /* Input clock edge */
#define SPI_CK_OUT_EDGE             (1 << 8)   /* Output clock edge */
#define SPI_WR_BYTE_ORDER           (1 << 10)  /* Byte order for write */
#define SPI_RD_BYTE_ORDER           (1 << 11)  /* Byte order for read */
#define SPI_FWRITE_DUAL             (1 << 12)  /* Fast write dual */
#define SPI_FWRITE_QUAD             (1 << 13)  /* Fast write quad */
#define SPI_FWRITE_OCT              (1 << 14)  /* Fast write octal */
#define SPI_USR_CONF_NXT            (1 << 26)  /* Use next config */
#define SPI_SIO                     (1 << 27)  /* 3-wire mode (shared MISO/MOSI) */
#define SPI_USR_COMMAND             (1 << 31)  /* Enable command phase */
#define SPI_USR_ADDR                (1 << 30)  /* Enable address phase */
#define SPI_USR_DUMMY               (1 << 29)  /* Enable dummy phase */
#define SPI_USR_MISO                (1 << 28)  /* Enable read data phase */
#define SPI_USR_MOSI                (1 << 27)  /* Enable write data phase */

/* --------------------------------------------------------------------------
 * SPI_USER1_REG Bits
 * -------------------------------------------------------------------------- */

#define SPI_USR_ADDR_BITLEN_S       26
#define SPI_USR_ADDR_BITLEN_M       (0x3f << SPI_USR_ADDR_BITLEN_S)
#define SPI_USR_DUMMY_CYCLELEN_S    0
#define SPI_USR_DUMMY_CYCLELEN_M    (0xff << SPI_USR_DUMMY_CYCLELEN_S)
#define SPI_CS_HOLD_TIME_S          8
#define SPI_CS_HOLD_TIME_M          (0xf << SPI_CS_HOLD_TIME_S)
#define SPI_CS_SETUP_TIME_S         12
#define SPI_CS_SETUP_TIME_M         (0xf << SPI_CS_SETUP_TIME_S)

/* --------------------------------------------------------------------------
 * SPI_USER2_REG Bits
 * -------------------------------------------------------------------------- */

#define SPI_USR_COMMAND_BITLEN_S    28
#define SPI_USR_COMMAND_BITLEN_M    (0xf << SPI_USR_COMMAND_BITLEN_S)
#define SPI_USR_COMMAND_VALUE_S     0
#define SPI_USR_COMMAND_VALUE_M     (0xffff << SPI_USR_COMMAND_VALUE_S)

/* --------------------------------------------------------------------------
 * SPI_MS_DLEN_REG Bits
 *
 * Data bit length = SPI_MS_DATA_BITLEN + 1
 * -------------------------------------------------------------------------- */

#define SPI_MS_DATA_BITLEN_S        0
#define SPI_MS_DATA_BITLEN_M        (0x3f << SPI_MS_DATA_BITLEN_S)

/* --------------------------------------------------------------------------
 * SPI_MISC_REG Bits
 * -------------------------------------------------------------------------- */

#define SPI_CS0_DIS                 (1 << 0)   /* Disable CS0 */
#define SPI_CS1_DIS                 (1 << 1)   /* Disable CS1 */
#define SPI_CS2_DIS                 (1 << 2)   /* Disable CS2 */
#define SPI_CS_IDLE_VAL             (1 << 7)   /* CS idle level */
#define SPI_CK_IDLE_EDGE            (1 << 8)   /* Clock idle edge (CPOL) */
#define SPI_CS_KEEP_ACTIVE          (1 << 9)   /* Keep CS active after xfer */

/* --------------------------------------------------------------------------
 * SPI_INT_RAW_REG / SPI_INT_CLR_REG / SPI_INT_ENA_REG / SPI_INT_ST_REG
 * -------------------------------------------------------------------------- */

#define SPI_TRANS_DONE_INT          (1 << 9)   /* Transfer complete */
#define SPI_INT_WR_DMA_DONE         (1 << 10)  /* DMA write done */
#define SPI_INT_RD_DMA_DONE         (1 << 11)  /* DMA read done */
#define SPI_INT_TRANS_DONE_EN       (1 << 9)

/* --------------------------------------------------------------------------
 * SPI_GPIO Matrix Signal Indices
 *
 * On ESP32-P4, peripheral signals are routed through the GPIO matrix.
 * Each GP-SPI controller has CLK, MISO, MOSI, CS0-CS5 signals.
 *
 * Reference: ESP32-P4 Technical Reference Manual, GPIO Matrix chapter.
 * -------------------------------------------------------------------------- */

#define SPI2_CLK_OUT_SIG            0
#define SPI2_CLK_IN_SIG             0
#define SPI2_MISO_OUT_SIG           1
#define SPI2_MISO_IN_SIG            1
#define SPI2_MOSI_OUT_SIG           2
#define SPI2_MOSI_IN_SIG            2
#define SPI2_CS0_OUT_SIG            3
#define SPI2_CS0_IN_SIG             3
#define SPI2_CS1_OUT_SIG            4
#define SPI2_CS1_IN_SIG             4
#define SPI2_CS2_OUT_SIG            5
#define SPI2_CS2_IN_SIG             5

#define SPI3_CLK_OUT_SIG            6
#define SPI3_CLK_IN_SIG             6
#define SPI3_MISO_OUT_SIG           7
#define SPI3_MISO_IN_SIG            7
#define SPI3_MOSI_OUT_SIG           8
#define SPI3_MOSI_IN_SIG            8
#define SPI3_CS0_OUT_SIG            9
#define SPI3_CS0_IN_SIG             9
#define SPI3_CS1_OUT_SIG            10
#define SPI3_CS1_IN_SIG             10
#define SPI3_CS2_OUT_SIG            11
#define SPI3_CS2_IN_SIG             11

/* --------------------------------------------------------------------------
 * SPI Clock Configuration
 *
 * The GP-SPI peripheral uses the APB clock as its source.
 * APB_CLK = 80 MHz (default on ESP32-P4).
 *
 * SPI_CLK = APB_CLK / (pre_divider * (N + 1))
 *
 * For 10 MHz: pre=1, N=7 => 80 MHz / (1 * 8) = 10 MHz
 * For 20 MHz: pre=1, N=3 => 80 MHz / (1 * 4) = 20 MHz
 * For 40 MHz: pre=1, N=1 => 80 MHz / (1 * 2) = 40 MHz
 * For 80 MHz: use CLK_EQU_SYSCLK
 * -------------------------------------------------------------------------- */

#define SPI_APB_CLK_FREQ            80000000   /* 80 MHz */

/* Maximum SPI clock frequency (APB clock) */

#define SPI_MAX_CLK_FREQ            SPI_APB_CLK_FREQ

/* Minimum SPI clock frequency (APB / 64 / 64) */

#define SPI_MIN_CLK_FREQ            (SPI_APB_CLK_FREQ / (64 * 64))

/* --------------------------------------------------------------------------
 * Register Access Macros
 * -------------------------------------------------------------------------- */

#define SPI_REG(base, offset) \
  (*(volatile uint32_t *)((base) + (offset)))

#endif /* __VENDOR_ESPRESSIF_CHIPS_ESP32P4_INCLUDE_HARDWARE_ESP32P4_SPI_H */
