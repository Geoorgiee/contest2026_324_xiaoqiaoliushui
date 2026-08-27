/****************************************************************************
 * vendor/espressif/chips/esp32p4/include/hardware/esp32p4_mspi.h
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

#ifndef __VENDOR_ESPRESSIF_CHIPS_ESP32P4_INCLUDE_HARDWARE_ESP32P4_MSPI_H
#define __VENDOR_ESPRESSIF_CHIPS_ESP32P4_INCLUDE_HARDWARE_ESP32P4_MSPI_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "esp32p4_soc.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* MSPI Controller Base Addresses
 *
 * ESP32-P4 has two MSPI controllers:
 *   SPI0 (0x60003000): Used for PSRAM cache access
 *   SPI1 (0x60004000): Used for flash cache access
 *
 * PSRAM is accessed through SPI0, which is connected to the cache
 * controller for memory-mapped access.
 */

#define MSPI_SPI0_BASE              DR_REG_SPI0_BASE
#define MSPI_SPI1_BASE              DR_REG_SPI1_BASE

/****************************************************************************
 * SPI_MEM Register Offsets (from SPI0/SPI1 base)
 *
 * These registers control the MSPI controller's memory-mapped access
 * mode, which is used for PSRAM and flash access through the cache.
 ****************************************************************************/

#define SPI_MEM_CMD_REG             0x0000  /* Command register */
#define SPI_MEM_ADDR_REG            0x0004  /* Address register */
#define SPI_MEM_CTRL_REG            0x0008  /* Control register */
#define SPI_MEM_CLOCK_REG           0x000C  /* Clock configuration */
#define SPI_MEM_USER_REG            0x0010  /* User-defined command config */
#define SPI_MEM_USER1_REG           0x0014  /* User-defined command config 1 */
#define SPI_MEM_USER2_REG           0x0018  /* User-defined command config 2 */
#define SPI_MEM_MS_DLEN_REG         0x001C  /* Data bit length */
#define SPI_MEM_MISC_REG            0x0020  /* Miscellaneous configuration */
#define SPI_MEM_TX_CRC_REG          0x0024  /* TX CRC register */
#define SPI_MEM_DATE_REG            0x0028  /* Date register */

/* Write data buffer (W0-W15) */

#define SPI_MEM_W_BASE              0x0080
#define SPI_MEM_W(n)                (SPI_MEM_W_BASE + ((n) * 4))

/* Read data buffer (R0-R15) */

#define SPI_MEM_R_BASE              0x00C0
#define SPI_MEM_R(n)                (SPI_MEM_R_BASE + ((n) * 4))

/* FSM and status registers */

#define SPI_MEM_FSM_REG             0x0100  /* FSM status register */

/* Cache control registers */

#define SPI_MEM_CACHE_FCTRL_REG     0x0200  /* Flash cache control */
#define SPI_MEM_CACHE_SCTRL_REG     0x0204  /* SRAM cache control */

/* SRAM (PSRAM) specific registers */

#define SPI_MEM_SRAM_CMD_REG        0x0300  /* SRAM command configuration */
#define SPI_MEM_SRAM_DWR_CMD_REG    0x0304  /* SRAM dummy/write command */
#define SPI_MEM_SRAM_DRD_CMD_REG    0x0308  /* SRAM dummy/read command */
#define SPI_MEM_SRAM_CLK_REG        0x0380  /* SRAM clock configuration */

/****************************************************************************
 * SPI_MEM_CMD_REG Bits
 ****************************************************************************/

#define SPI_MEM_USR                 (1 << 18)   /* User-defined command */
#define SPI_MEM_FLASH_PE            (1 << 16)   /* Flash program/erase */
#define SPI_MEM_CS_DIS              (1 << 9)    /* CS disable during idle */

/****************************************************************************
 * SPI_MEM_CTRL_REG Bits
 ****************************************************************************/

#define SPI_MEM_FREAD_DUAL          (1 << 14)   /* Dual read mode */
#define SPI_MEM_FREAD_QUAD          (1 << 13)   /* Quad read mode */
#define SPI_MEM_FREAD_OCT           (1 << 12)   /* Octal read mode */
#define SPI_MEM_FCMD_DUAL           (1 << 11)   /* Dual command mode */
#define SPI_MEM_FCMD_QUAD           (1 << 10)   /* Quad command mode */
#define SPI_MEM_FCMD_OCT            (1 << 9)    /* Octal command mode */

/****************************************************************************
 * SPI_MEM_CLOCK_REG Bits
 *
 * Clock configuration for the SPI peripheral.
 * SPI_CLK = CLK_SRC / (CLKDIV_PRE + 1) / (CLKCNT_N + 1)
 *
 * For OPI PSRAM at 80 MHz (from 160 MHz source):
 *   CLKDIV_PRE = 0, CLKCNT_N = 1 => 160 / 2 = 80 MHz
 ****************************************************************************/

#define SPI_MEM_CLK_EQU_SYSCLK      (1 << 31)   /* Clock = system clock */
#define SPI_MEM_CLKDIV_PRE_S        12
#define SPI_MEM_CLKDIV_PRE_M        (0x3f << SPI_MEM_CLKDIV_PRE_S)
#define SPI_MEM_CLKCNT_N_S          0
#define SPI_MEM_CLKCNT_N_M          (0x3f << SPI_MEM_CLKCNT_N_S)
#define SPI_MEM_CLKCNT_H_S          6
#define SPI_MEM_CLKCNT_H_M          (0x3f << SPI_MEM_CLKCNT_H_S)
#define SPI_MEM_CLKCNT_L_S          12
#define SPI_MEM_CLKCNT_L_M          (0x3f << SPI_MEM_CLKCNT_L_S)

/****************************************************************************
 * SPI_MEM_USER_REG Bits
 *
 * Controls which phases are active in a user-defined SPI transaction.
 ****************************************************************************/

#define SPI_MEM_USR_COMMAND          (1 << 31)   /* Enable command phase */
#define SPI_MEM_USR_ADDR             (1 << 30)   /* Enable address phase */
#define SPI_MEM_USR_DUMMY            (1 << 29)   /* Enable dummy phase */
#define SPI_MEM_USR_MISO             (1 << 28)   /* Enable read phase */
#define SPI_MEM_USR_MOSI             (1 << 27)   /* Enable write phase */
#define SPI_MEM_USR_DUMMY_IDLE       (1 << 26)   /* Dummy phase idle */
#define SPI_MEM_USR_MOSI_HIGHPART    (1 << 25)   /* Write to high buffer */
#define SPI_MEM_USR_MISO_HIGHPART    (1 << 24)   /* Read from high buffer */
#define SPI_MEM_SIO                  (1 << 17)   /* Single I/O mode */

/****************************************************************************
 * SPI_MEM_USER1_REG Bits
 ****************************************************************************/

#define SPI_MEM_USR_ADDR_BITLEN_S   26
#define SPI_MEM_USR_ADDR_BITLEN_M   (0x3f << SPI_MEM_USR_ADDR_BITLEN_S)
#define SPI_MEM_USR_DUMMY_CYCLELEN_S 0
#define SPI_MEM_USR_DUMMY_CYCLELEN_M (0xff << SPI_MEM_USR_DUMMY_CYCLELEN_S)

/****************************************************************************
 * SPI_MEM_USER2_REG Bits
 ****************************************************************************/

#define SPI_MEM_USR_COMMAND_BITLEN_S 28
#define SPI_MEM_USR_COMMAND_BITLEN_M (0xf << SPI_MEM_USR_COMMAND_BITLEN_S)
#define SPI_MEM_USR_COMMAND_VALUE_S  0
#define SPI_MEM_USR_COMMAND_VALUE_M  (0xffff << SPI_MEM_USR_COMMAND_VALUE_S)

/****************************************************************************
 * SPI_MEM_MS_DLEN_REG Bits
 ****************************************************************************/

#define SPI_MEM_MS_DATA_BITLEN_S    0
#define SPI_MEM_MS_DATA_BITLEN_M    (0x3fffff << SPI_MEM_MS_DATA_BITLEN_S)

/****************************************************************************
 * SPI_MEM_MISC_REG Bits
 ****************************************************************************/

#define SPI_MEM_CS1_DIS             (1 << 1)    /* CS1 disable */
#define SPI_MEM_CS0_DIS             (1 << 0)    /* CS0 disable */

/****************************************************************************
 * SPI_MEM_SRAM_CMD_REG Bits
 ****************************************************************************/

#define SPI_MEM_CACHE_SRAM_USR_RCMD (1 << 28)   /* Cache SRAM read cmd */
#define SPI_MEM_CACHE_SRAM_USR_WCMD (1 << 23)   /* Cache SRAM write cmd */

/****************************************************************************
 * SPI_MEM_SRAM_CLK_REG Bits
 ****************************************************************************/

#define SPI_MEM_SCLK_EQU_SYSCLK     (1 << 31)   /* SRAM clock = sys clock */
#define SPI_MEM_SCLKCNT_N_S         0
#define SPI_MEM_SCLKCNT_N_M         (0x3f << SPI_MEM_SCLKCNT_N_S)
#define SPI_MEM_SCLKCNT_H_S         6
#define SPI_MEM_SCLKCNT_H_M         (0x3f << SPI_MEM_SCLKCNT_H_S)
#define SPI_MEM_SCLKCNT_L_S         12
#define SPI_MEM_SCLKCNT_L_M         (0x3f << SPI_MEM_SCLKCNT_L_S)

/****************************************************************************
 * SPI_MEM_CACHE_FCTRL_REG Bits
 ****************************************************************************/

#define SPI_MEM_CACHE_USR_CMD_4BYTE  (1 << 8)   /* 4-byte command */
#define SPI_MEM_CACHE_FLASH_PES_EN   (1 << 7)   /* Flash PES enable */
#define SPI_MEM_CACHE_EMPTY_DISCARD  (1 << 6)   /* Empty discard */
#define SPI_MEM_CACHE_FMEM_WB_EN     (1 << 5)   /* Write-back enable */
#define SPI_MEM_CACHE_FMEM_RD_MODE_S 4
#define SPI_MEM_CACHE_FMEM_RD_MODE_M (0x1 << SPI_MEM_CACHE_FMEM_RD_MODE_S)
#define SPI_MEM_CACHE_FMEM_MBUS_EN   (1 << 3)   /* MBUS enable */
#define SPI_MEM_CACHE_FMEM_SHARE_EN  (1 << 2)   /* Share enable */
#define SPI_MEM_CACHE_FMEM_CACHE_EN  (1 << 1)   /* Cache enable */

/****************************************************************************
 * PSRAM Command Definitions (OPI Mode)
 *
 * These are the OPI-mode commands for ESP-PSRAM65 (or compatible) chips.
 * In OPI mode, all commands are 8 bits (sent on all 8 data lines).
 ****************************************************************************/

/* PSRAM commands in SPI mode (used before entering OPI) */

#define PSRAM_CMD_READ_ID_SPI       0x9F    /* Read ID (SPI mode) */
#define PSRAM_CMD_ENTER_OPI         0xA0    /* Enter OPI mode */

/* PSRAM commands in OPI mode */

#define PSRAM_CMD_READ_ID_OPI       0x60    /* Read ID (OPI mode) */
#define PSRAM_CMD_READ_OPI          0x2000  /* Linear burst read */
#define PSRAM_CMD_WRITE_OPI         0x8000  /* Linear burst write */
#define PSRAM_CMD_RESET_EN_OPI      0x6666  /* Reset enable */
#define PSRAM_CMD_RESET_OPI         0x9999  /* Reset */

/* PSRAM ID values (for verification) */

#define PSRAM_ID_ESPPSRAM65         0x5D0D  /* ESP-PSRAM65 ID */

/****************************************************************************
 * Clock Configuration Constants
 *
 * Default clock configuration for PSRAM initialization.
 * The MSPI clock source is typically 160 MHz or 80 MHz depending on
 * the PLL configuration.
 ****************************************************************************/

#define MSPI_CLK_SRC_FREQ           160000000   /* 160 MHz clock source */
#define MSPI_PSRAM_INIT_CLK_FREQ    80000000    /* 80 MHz for init */
#define MSPI_PSRAM_MAX_CLK_FREQ     200000000   /* 200 MHz max for OPI */

#endif /* __VENDOR_ESPRESSIF_CHIPS_ESP32P4_INCLUDE_HARDWARE_ESP32P4_MSPI_H */
