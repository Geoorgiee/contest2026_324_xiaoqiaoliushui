/****************************************************************************
 * vendor/espressif/chips/esp32p4/include/hardware/esp32p4_uart.h
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

#ifndef __VENDOR_ESPRESSIF_CHIPS_ESP32P4_INCLUDE_HARDWARE_ESP32P4_UART_H
#define __VENDOR_ESPRESSIF_CHIPS_ESP32P4_INCLUDE_HARDWARE_ESP32P4_UART_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "esp32p4_soc.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* UART Register Offsets */

#define UART_FIFO_REG               0x0000  /* FIFO data register */
#define UART_INT_RAW_REG            0x0004  /* Raw interrupt status */
#define UART_INT_ST_REG             0x0008  /* Masked interrupt status */
#define UART_INT_ENA_REG            0x000C  /* Interrupt enable */
#define UART_INT_CLR_REG            0x0010  /* Interrupt clear */
#define UART_CLKDIV_REG             0x0014  /* Clock divider */
#define UART_CLKDIV_FRAG_REG       0x0018  /* Clock divider fraction */
#define UART_CONF0_REG             0x0020  /* Configuration 0 */
#define UART_CONF1_REG             0x0024  /* Configuration 1 */
#define UART_FLOW_CONF_REG         0x0028  /* Flow control config */
#define UART_SLEEP_CONF_REG        0x002C  /* Sleep configuration */
#define UART_SWFC_CONF_REG         0x0030  /* Software flow control */
#define UART_IDLE_CONF_REG         0x0034  /* Idle character config */
#define UART_RS485_CONF_REG        0x0038  /* RS485 configuration */
#define UART_AT_CMD_PRECNT_REG     0x003C  /* AT command pre-count */
#define UART_AT_CMD_POSTCNT_REG    0x0040  /* AT command post-count */
#define UART_AT_CMD_GAPTOUT_REG    0x0044  /* AT command gap timeout */
#define UART_AT_CMD_CHAR_REG       0x0048  /* AT command character */
#define UART_MEM_CONF_REG          0x004C  /* Memory configuration */
#define UART_MEM_TX_STATUS_REG     0x0060  /* TX memory status */
#define UART_MEM_RX_STATUS_REG     0x0064  /* RX memory status */
#define UART_STATUS_REG            0x0068  /* UART status */
#define UART_DATE_REG              0x00F0  /* UART version */

/* UART Interrupt Bits */

#define UART_RXFIFO_FULL_INT_RAW   (1 << 0)   /* RX FIFO full */
#define UART_TXFIFO_EMPTY_INT_RAW  (1 << 1)   /* TX FIFO empty */
#define UART_PARITY_ERR_INT_RAW    (1 << 2)   /* Parity error */
#define UART_FRM_ERR_INT_RAW       (1 << 3)   /* Frame error */
#define UART_RXFIFO_OVF_INT_RAW    (1 << 4)   /* RX FIFO overflow */
#define UART_DSR_CHG_INT_RAW       (1 << 5)   /* DSR change */
#define UART_CTS_CHG_INT_RAW       (1 << 6)   /* CTS change */
#define UART_BRK_DET_INT_RAW       (1 << 7)   /* Break detect */
#define UART_RXFIFO_TOUT_INT_RAW   (1 << 8)   /* RX FIFO timeout */

/* UART Status Bits */

#define UART_ST_UTX_OUT             (0xf << 0)  /* TX state machine */
#define UART_ST_URX_OUT             (0xf << 4)  /* RX state machine */

/* UART CONF0 Bits
 *
 * Data format fields (bits 0-5) per ESP32-P4 HP UART register layout:
 *   Bit 0     : PARITY      - Parity type (0=even, 1=odd)
 *   Bit 1     : PARITY_EN   - Parity check enable
 *   Bits [3:2]: BIT_NUM     - Data bits (00=5, 01=6, 10=7, 11=8)
 *   Bits [5:4]: STOP_BIT_NUM - Stop bits (01=1, 10=1.5, 11=2)
 */

#define UART_PARITY                (1 << 0)   /* 0=even parity, 1=odd parity */
#define UART_PARITY_EN             (1 << 1)   /* Enable parity check */
#define UART_BIT_NUM_S             2
#define UART_BIT_NUM_M             (0x3 << UART_BIT_NUM_S)
#define UART_BIT_NUM_5             (0x0 << UART_BIT_NUM_S)  /* 5 data bits */
#define UART_BIT_NUM_6             (0x1 << UART_BIT_NUM_S)  /* 6 data bits */
#define UART_BIT_NUM_7             (0x2 << UART_BIT_NUM_S)  /* 7 data bits */
#define UART_BIT_NUM_8             (0x3 << UART_BIT_NUM_S)  /* 8 data bits */
#define UART_STOP_BIT_NUM_S        4
#define UART_STOP_BIT_NUM_M        (0x3 << UART_STOP_BIT_NUM_S)
#define UART_STOP_BIT_NUM_1        (0x1 << UART_STOP_BIT_NUM_S)  /* 1 stop bit */
#define UART_STOP_BIT_NUM_1P5      (0x2 << UART_STOP_BIT_NUM_S)  /* 1.5 stop bits */
#define UART_STOP_BIT_NUM_2        (0x3 << UART_STOP_BIT_NUM_S)  /* 2 stop bits */

/* FIFO control (bits 6-7) */

#define UART_TXFIFO_RST            (1 << 6)
#define UART_RXFIFO_RST            (1 << 7)

/* IRDA and misc (bits 8-15) */

#define UART_IRDA_EN               (1 << 8)
#define UART_IRDA_TX_EN            (1 << 9)
#define UART_IRDA_WCTL             (1 << 10)
#define UART_IRDA_TX_INV           (1 << 11)
#define UART_LOOPBACK              (1 << 12)
#define UART_TX_FLOW_EN            (1 << 13)
#define UART_IRDA_DPLX             (1 << 14)
#define UART_IRDA_RX_FILTER        (1 << 15)

/* Thresholds (bits 16-29 of CONF0) */

#define UART_RXFIFO_FULL_THRHD_S   16
#define UART_RXFIFO_FULL_THRHD_M   (0x7f << UART_RXFIFO_FULL_THRHD_S)
#define UART_TXFIFO_EMPTY_THRHD_S  23
#define UART_TXFIFO_EMPTY_THRHD_M  (0x7f << UART_TXFIFO_EMPTY_THRHD_S)

/* Control bits (bits 30-31 of CONF0) */

#define UART_DIS_RX_DAT_OVF        (1 << 30)
#define UART_ERR_WR_MASK           (1 << 31)

/* UART CONF1 Bits */

#define UART_RX_TOUT_EN            (1 << 6)
#define UART_RX_TOUT_THRHD_S       7
#define UART_RX_TOUT_THRHD_M       (0x7ff << UART_RX_TOUT_THRHD_S)
#define UART_RX_FLOW_EN            (1 << 18)

/* Default Baud Rate */

#define UART_CLK_FREQ              80000000  /* 80 MHz APB clock */
#define UART_DEFAULT_BAUD          115200

#endif /* __VENDOR_ESPRESSIF_CHIPS_ESP32P4_INCLUDE_HARDWARE_ESP32P4_UART_H */
