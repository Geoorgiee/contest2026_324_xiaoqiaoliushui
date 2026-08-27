/****************************************************************************
 * vendor/espressif/chips/esp32p4/include/hardware/esp32p4_i2c.h
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

#ifndef __VENDOR_ESPRESSIF_CHIPS_ESP32P4_INCLUDE_HARDWARE_ESP32P4_I2C_H
#define __VENDOR_ESPRESSIF_CHIPS_ESP32P4_INCLUDE_HARDWARE_ESP32P4_I2C_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "esp32p4_soc.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* I2C Peripheral Base Addresses
 *
 * ESP32-P4 has 2 HP I2C controllers (I2C0, I2C1) and 1 LP I2C controller.
 * The HP I2C registers are identical in layout; only the base addresses
 * and interrupt numbers differ.
 */

#define I2C0_BASE                   DR_REG_I2C0_BASE    /* 0x60013000 */
#define I2C1_BASE                   DR_REG_I2C1_BASE    /* 0x60014000 */
#define LP_I2C_BASE                 0x600B2000

/* Number of I2C controllers */

#define I2C_NUM_MAX                 3   /* I2C0, I2C1, LP_I2C */

/* I2C FIFO depth (bytes) */

#define I2C_FIFO_LEN                32

/* Number of command registers */

#define I2C_CMD_REG_NUM             8

/* --------------------------------------------------------------------------
 * I2C Register Offsets
 * -------------------------------------------------------------------------- */

/* SCL low period configuration (9-bit, in i2c_sclk units) */

#define I2C_SCL_LOW_PERIOD_REG      0x0000

/* Control register */

#define I2C_CTR_REG                 0x0004

/* Status register (read-only) */

#define I2C_SR_REG                  0x0008

/* Time-out configuration */

#define I2C_TO_REG                  0x000C

/* Slave address configuration */

#define I2C_SLAVE_ADDR_REG          0x0010

/* FIFO status (read-only) */

#define I2C_FIFO_ST_REG             0x0014

/* FIFO configuration */

#define I2C_FIFO_CONF_REG           0x0018

/* FIFO data (read RXFIFO) */

#define I2C_DATA_REG                0x001C

/* Raw interrupt status */

#define I2C_INT_RAW_REG             0x0020

/* Interrupt clear (write-to-clear) */

#define I2C_INT_CLR_REG             0x0024

/* Interrupt enable */

#define I2C_INT_ENA_REG             0x0028

/* Masked interrupt status (read-only) */

#define I2C_INT_STATUS_REG          0x002C

/* SDA hold time (9-bit, in i2c_sclk units) */

#define I2C_SDA_HOLD_REG            0x0030

/* SDA sample time (9-bit, in i2c_sclk units) */

#define I2C_SDA_SAMPLE_REG          0x0034

/* SCL high period (9-bit + 7-bit wait, in i2c_sclk units) */

#define I2C_SCL_HIGH_PERIOD_REG     0x0038

/* SCL start hold time (9-bit) */

#define I2C_SCL_START_HOLD_REG      0x0040

/* SCL repeated-start setup time (9-bit) */

#define I2C_SCL_RSTART_SETUP_REG    0x0044

/* SCL stop hold time (9-bit) */

#define I2C_SCL_STOP_HOLD_REG       0x0048

/* SCL stop setup time (9-bit) */

#define I2C_SCL_STOP_SETUP_REG      0x004C

/* SCL/SDA filter configuration */

#define I2C_FILTER_CFG_REG          0x0050

/* Command registers 0-7 */

#define I2C_COMD_REG(n)             (0x0058 + ((n) * 4))

/* SCL status timeout */

#define I2C_SCL_ST_TIME_OUT_REG     0x0078

/* SCL main status timeout */

#define I2C_SCL_MAIN_ST_TIME_OUT_REG 0x007C

/* SCL special configuration */

#define I2C_SCL_SP_CONF_REG         0x0080

/* SCL stretch configuration */

#define I2C_SCL_STRETCH_CONF_REG    0x0084

/* Date/version register */

#define I2C_DATE_REG                0x00F8

/* TX FIFO memory (write-only, 32 x 32-bit words) */

#define I2C_TXFIFO_MEM              0x0100

/* RX FIFO memory (read-only, 32 x 32-bit words) */

#define I2C_RXFIFO_MEM              0x0180

/* --------------------------------------------------------------------------
 * I2C_CTR_REG Bits
 * -------------------------------------------------------------------------- */

#define I2C_SDA_FORCE_OUT           (1 << 0)   /* 1=direct, 0=open-drain */
#define I2C_SCL_FORCE_OUT           (1 << 1)   /* 1=direct, 0=open-drain */
#define I2C_SAMPLE_SCL_LEVEL        (1 << 2)   /* 1=sample on SCL low */
#define I2C_RX_FULL_ACK_LEVEL       (1 << 3)   /* ACK value when RX full */
#define I2C_MS_MODE                 (1 << 4)   /* 1=master, 0=slave */
#define I2C_TRANS_START             (1 << 5)   /* Write 1 to start TX */
#define I2C_TX_LSB_FIRST            (1 << 6)
#define I2C_RX_LSB_FIRST            (1 << 7)
#define I2C_CLK_EN                  (1 << 8)   /* 0=force on, 1=gated */
#define I2C_ARBITRATION_EN          (1 << 9)
#define I2C_FSM_RST                 (1 << 10)  /* Write 1 to reset FSM */
#define I2C_CONF_UPGATE             (1 << 11)  /* Write 1 to sync config */
#define I2C_SLV_TX_AUTO_START_EN    (1 << 12)

/* --------------------------------------------------------------------------
 * I2C_SR_REG Bits (Read-Only)
 * -------------------------------------------------------------------------- */

#define I2C_RESP_REC                (1 << 0)   /* 0=ACK, 1=NACK */
#define I2C_SLAVE_RW                (1 << 1)   /* 1=master reads */
#define I2C_ARB_LOST                (1 << 3)
#define I2C_BUS_BUSY                (1 << 4)
#define I2C_SLAVE_ADDRESSED         (1 << 5)
#define I2C_RXFIFO_CNT_S            8
#define I2C_RXFIFO_CNT_M            (0x3f << I2C_RXFIFO_CNT_S)
#define I2C_STRETCH_CAUSE_S         14
#define I2C_STRETCH_CAUSE_M         (0x3 << I2C_STRETCH_CAUSE_S)
#define I2C_TXFIFO_CNT_S            18
#define I2C_TXFIFO_CNT_M            (0x3f << I2C_TXFIFO_CNT_S)
#define I2C_SCL_MAIN_STATE_S        24
#define I2C_SCL_MAIN_STATE_M        (0x7 << I2C_SCL_MAIN_STATE_S)
#define I2C_SCL_STATE_S             28
#define I2C_SCL_STATE_M             (0x7 << I2C_SCL_STATE_S)

/* --------------------------------------------------------------------------
 * I2C_TO_REG Bits
 * -------------------------------------------------------------------------- */

#define I2C_TIME_OUT_VALUE_S        0
#define I2C_TIME_OUT_VALUE_M        (0x1f << I2C_TIME_OUT_VALUE_S)
#define I2C_TIME_OUT_EN             (1 << 5)

/* --------------------------------------------------------------------------
 * I2C_FIFO_CONF_REG Bits
 * -------------------------------------------------------------------------- */

#define I2C_RXFIFO_WM_THRHD_S      0
#define I2C_RXFIFO_WM_THRHD_M      (0x1f << I2C_RXFIFO_WM_THRHD_S)
#define I2C_TXFIFO_WM_THRHD_S      5
#define I2C_TXFIFO_WM_THRHD_M      (0x1f << I2C_TXFIFO_WM_THRHD_S)
#define I2C_NONFIFO_EN              (1 << 10)
#define I2C_FIFO_ADDR_CFG_EN        (1 << 11)
#define I2C_RX_FIFO_RST             (1 << 12)
#define I2C_TX_FIFO_RST             (1 << 13)
#define I2C_FIFO_PRT_EN             (1 << 14)

/* --------------------------------------------------------------------------
 * I2C_INT_RAW_REG / I2C_INT_CLR_REG / I2C_INT_ENA_REG Bits
 * -------------------------------------------------------------------------- */

#define I2C_RXFIFO_WM_INT           (1 << 0)
#define I2C_TXFIFO_WM_INT           (1 << 1)
#define I2C_RXFIFO_OVF_INT          (1 << 2)
#define I2C_END_DETECT_INT          (1 << 3)
#define I2C_BYTE_TRANS_DONE_INT     (1 << 4)
#define I2C_ARBITRATION_LOST_INT    (1 << 5)
#define I2C_MST_TXFIFO_UDF_INT      (1 << 6)
#define I2C_TRANS_COMPLETE_INT      (1 << 7)
#define I2C_TIME_OUT_INT            (1 << 8)
#define I2C_TRANS_START_INT         (1 << 9)
#define I2C_NACK_INT                (1 << 10)
#define I2C_TXFIFO_OVF_INT          (1 << 11)
#define I2C_RXFIFO_UDF_INT          (1 << 12)
#define I2C_SCL_ST_TO_INT           (1 << 13)
#define I2C_SCL_MAIN_ST_TO_INT      (1 << 14)
#define I2C_DET_START_INT           (1 << 15)
#define I2C_SLAVE_STRETCH_INT       (1 << 16)

/* All master-mode interrupts */

#define I2C_MASTER_INT_MASK         (I2C_RXFIFO_WM_INT | \
                                     I2C_TXFIFO_WM_INT | \
                                     I2C_END_DETECT_INT | \
                                     I2C_BYTE_TRANS_DONE_INT | \
                                     I2C_ARBITRATION_LOST_INT | \
                                     I2C_TRANS_COMPLETE_INT | \
                                     I2C_TIME_OUT_INT | \
                                     I2C_NACK_INT)

/* --------------------------------------------------------------------------
 * I2C_FILTER_CFG_REG Bits
 * -------------------------------------------------------------------------- */

#define I2C_SCL_FILTER_THRES_S      0
#define I2C_SCL_FILTER_THRES_M      (0xf << I2C_SCL_FILTER_THRES_S)
#define I2C_SDA_FILTER_THRES_S      4
#define I2C_SDA_FILTER_THRES_M      (0xf << I2C_SDA_FILTER_THRES_S)
#define I2C_SCL_FILTER_EN           (1 << 8)
#define I2C_SDA_FILTER_EN           (1 << 9)

/* --------------------------------------------------------------------------
 * I2C Command Register (COMD) Encoding
 *
 * Each command register is 14 bits wide (bits [13:0]) plus a
 * command_done flag at bit [31].
 *
 *   [2:0]   op_code
 *   [10:3]  byte_num  (number of bytes for READ/WRITE)
 *   [11]    ack_check_en (WRITE only)
 *   [12]    ack_exp      (WRITE only: expected ACK level)
 *   [13]    ack_val      (READ only: ACK/NACK to send)
 *
 * Op codes:
 *   0 = RSTART (repeated start)
 *   1 = WRITE
 *   2 = READ
 *   3 = STOP
 *   4 = END (marks end of command sequence)
 * -------------------------------------------------------------------------- */

#define I2C_CMD_OP_CODE_S           0
#define I2C_CMD_OP_CODE_M           (0x7 << I2C_CMD_OP_CODE_S)
#define I2C_CMD_BYTE_NUM_S          3
#define I2C_CMD_BYTE_NUM_M          (0xff << I2C_CMD_BYTE_NUM_S)
#define I2C_CMD_ACK_CHECK_EN        (1 << 11)
#define I2C_CMD_ACK_EXP             (1 << 12)
#define I2C_CMD_ACK_VAL             (1 << 13)
#define I2C_CMD_DONE                (1 << 31)

/* Op code values */

#define I2C_CMD_OP_RSTART           0
#define I2C_CMD_OP_WRITE            1
#define I2C_CMD_OP_READ             2
#define I2C_CMD_OP_STOP             3
#define I2C_CMD_OP_END              4

/* --------------------------------------------------------------------------
 * I2C GPIO Matrix Signal Indices
 *
 * On ESP32-P4, peripheral signals are routed through the GPIO matrix.
 * Each I2C controller has SCL_OUT, SCL_IN, SDA_OUT, SDA_IN signals.
 * -------------------------------------------------------------------------- */

#define I2C0_SCL_OUT_SIG            68
#define I2C0_SCL_IN_SIG             68
#define I2C0_SDA_OUT_SIG            69
#define I2C0_SDA_IN_SIG             69
#define I2C1_SCL_OUT_SIG            70
#define I2C1_SCL_IN_SIG             70
#define I2C1_SDA_OUT_SIG            71
#define I2C1_SDA_IN_SIG             71

/* --------------------------------------------------------------------------
 * I2C Clock Configuration
 *
 * The I2C peripheral uses the APB clock as its source.
 * APB_CLK = 80 MHz (default on ESP32-P4).
 *
 * SCL frequency = APB_CLK / (scl_high_period + scl_low_period)
 *
 * For 100 kHz (standard mode):
 *   total_period = 80 MHz / 100 kHz = 800
 *   high = 400, low = 400
 *
 * For 400 kHz (fast mode):
 *   total_period = 80 MHz / 400 kHz = 200
 *   high = 100, low = 100
 * -------------------------------------------------------------------------- */

#define I2C_APB_CLK_FREQ            80000000   /* 80 MHz */

/* Default timing values (in APB clock cycles) */

#define I2C_SCL_HIGH_PERIOD_100K    400
#define I2C_SCL_LOW_PERIOD_100K     400
#define I2C_SCL_HIGH_PERIOD_400K    100
#define I2C_SCL_LOW_PERIOD_400K     100

/* SDA timing (in i2c_sclk cycles) */

#define I2C_SDA_HOLD_TIME           10
#define I2C_SDA_SAMPLE_TIME         10

/* Start/Stop timing (in i2c_sclk cycles) */

#define I2C_SCL_START_HOLD_TIME     8
#define I2C_SCL_RSTART_SETUP_TIME   8
#define I2C_SCL_STOP_HOLD_TIME      8
#define I2C_SCL_STOP_SETUP_TIME     8

/* Filter threshold (APB clock cycles) */

#define I2C_FILTER_CYCLES           7

/* Timeout value (2^n i2c_sclk cycles, n=13 gives ~8ms at 100kHz) */

#define I2C_TIMEOUT_VALUE           13

/* --------------------------------------------------------------------------
 * Register Access Macros
 * -------------------------------------------------------------------------- */

#define I2C_REG(base, offset) \
  (*(volatile uint32_t *)((base) + (offset)))

#endif /* __VENDOR_ESPRESSIF_CHIPS_ESP32P4_INCLUDE_HARDWARE_ESP32P4_I2C_H */
