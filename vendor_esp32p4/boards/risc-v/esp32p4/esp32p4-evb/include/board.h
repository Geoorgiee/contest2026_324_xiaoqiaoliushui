/****************************************************************************
 * vendor_esp32p4/boards/risc-v/esp32p4/esp32p4-evb/include/board.h
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

#ifndef __VENDOR_ESP32P4_BOARDS_RISCV_ESP32P4_ESP32P4_EVB_INCLUDE_BOARD_H
#define __VENDOR_ESP32P4_BOARDS_RISCV_ESP32P4_ESP32P4_EVB_INCLUDE_BOARD_H

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* ESP32-P4 EVB Board LED GPIO
 *
 * The ESP32-P4 EVB has an on-board LED connected to GPIO 26.
 * GPIO 0 is used by the SPI flash and must not be used for LED.
 * GPIO 26 is a general-purpose pin available on the EVB header.
 */

#define BOARD_LED1_GPIO  26

/* ESP32-P4 EVB Board Button GPIO
 *
 * The ESP32-P4 EVB has a BOOT button connected to GPIO 21 (active low).
 * GPIO 21 is the strapping pin used for boot mode selection.
 */

#define BOARD_BUTTON1_GPIO  21

/* ESP32-P4 EVB Board Clock Configuration
 *
 * The board uses a 40 MHz crystal oscillator as the reference clock.
 * The PLL generates 480 MHz, and the CPU runs at 400 MHz.
 * The APB bus clock runs at 80 MHz for peripheral access.
 */

#define BOARD_XTAL_FREQ     40000000
#define BOARD_CPU_FREQ      240000000  /* PLL(480MHz) / 2 = 240MHz actual */
#define BOARD_APB_FREQ      80000000

/* ESP32-P4 EVB Board UART Configuration
 *
 * UART0 is the default serial console, connected to the USB-UART bridge
 * on the EVB board. Default baud rate is 115200.
 */

#define BOARD_UART0_BAUD    115200
#define BOARD_UART0_BITS    8
#define BOARD_UART0_PARITY  0
#define BOARD_UART0_2STOP   0

#endif /* __VENDOR_ESP32P4_BOARDS_RISCV_ESP32P4_ESP32P4_EVB_INCLUDE_BOARD_H */
