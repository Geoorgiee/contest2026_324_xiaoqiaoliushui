/****************************************************************************
 * boards/risc-v/esp32p4/esp32p4-function-ev-board/src/esp32p4-function-ev-board.h
 *
 * SPDX-License-Identifier: Apache-2.0
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

#ifndef __BOARDS_RISCV_ESP32P4_ESP32P4_FUNCTION_EV_BOARD_SRC_ESP32P4_FUNCTION_EV_BOARD_H
#define __BOARDS_RISCV_ESP32P4_ESP32P4_FUNCTION_EV_BOARD_SRC_ESP32P4_FUNCTION_EV_BOARD_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Audio pin map.
 *
 * Values confirmed from the Espressif official BSP for this board
 * (esp-bsp bsp/esp32_p4_function_ev_board/include/bsp/esp32_p4_function_ev_board.h):
 *   BSP_I2S_SCLK = GPIO12, BSP_I2S_MCLK = GPIO13,
 *   BSP_I2S_LCLK = GPIO10, BSP_I2S_DOUT = GPIO9,
 *   BSP_I2S_DSIN = GPIO11, I2C SCL = GPIO8, I2C SDA = GPIO7,
 *   BSP_POWER_AMP_IO (NS4150B PA enable) = GPIO53.
 *
 * The actual pin routing is configured through the chip-level options
 * CONFIG_ESPRESSIF_I2S0_BCLKPIN / WSPIN / DINPIN / DOUTPIN / MCLKPIN in the
 * defconfig; these macros document the map and are used by board code.
 */

#define ESP32P4_AUDIO_I2S_BCLK_PIN  12
#define ESP32P4_AUDIO_I2S_MCLK_PIN  13
#define ESP32P4_AUDIO_I2S_WS_PIN    10
#define ESP32P4_AUDIO_I2S_DOUT_PIN  9
#define ESP32P4_AUDIO_I2S_DIN_PIN   11
#define ESP32P4_AUDIO_PA_AMP_GPIO   53 /* TODO(真机): NS4150B PA enable */

/* RMT gpio */

#define RMT_RXCHANNEL       4
#define RMT_TXCHANNEL       0

#ifdef CONFIG_RMT_LOOP_TEST_MODE
#  define RMT_INPUT_PIN     0
#  define RMT_OUTPUT_PIN    0
#else
#  define RMT_INPUT_PIN     2
#  define RMT_OUTPUT_PIN    8
#endif

/****************************************************************************
 * Public Types
 ****************************************************************************/

/****************************************************************************
 * Public Data
 ****************************************************************************/

#ifndef __ASSEMBLY__

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Name: esp_bringup
 *
 * Description:
 *   Perform architecture-specific initialization.
 *
 * Input Parameters:
 *   None.
 *
 * Returned Value:
 *   Zero (OK) is returned on success; A negated errno value is returned on
 *   any failure.
 *
 ****************************************************************************/

int esp_bringup(void);

/****************************************************************************
 * Name: board_i2c_init
 *
 * Description:
 *   Initialize I2C1 and register /dev/i2c1.
 *
 * Returned Value:
 *   Zero (OK) is returned on success; a negated errno value on failure.
 *
 ****************************************************************************/

#if defined(CONFIG_I2C_DRIVER) && \
    defined(CONFIG_ESPRESSIF_I2C1_MASTER_MODE)
int board_i2c_init(void);
#endif

/****************************************************************************
 * Name: board_twai_setup
 *
 * Description:
 *  Initialize TWAI and register the TWAI device
 *
 * Input Parameters:
 *   port - Port number (for hardware that has multiple TWAI interfaces)
 *
 * Returned Value:
 *   Zero (OK) is returned on success; A negated errno value is returned on
 *   any failure.
 *
 ****************************************************************************/

#ifdef CONFIG_ESPRESSIF_TWAI
int board_twai_setup(int port);
#endif

/****************************************************************************
 * Name: esp_gpio_init
 *
 * Description:
 *   Configure the GPIO driver.
 *
 * Input Parameters:
 *   None.
 *
 * Returned Value:
 *   Zero (OK).
 *
 ****************************************************************************/

#ifdef CONFIG_DEV_GPIO
int esp_gpio_init(void);
#endif

/****************************************************************************
 * Name: board_emac_init
 *
 * Description:
 *   Bring up the ESP32-P4 Ethernet MAC driver (esp_eth backed).
 *
 * Input Parameters:
 *   None.
 *
 * Returned Value:
 *   Zero (OK) on success; a negated errno value on failure.
 *
 ****************************************************************************/

#ifdef CONFIG_ESPRESSIF_EMAC
int board_emac_init(void);
#endif

/****************************************************************************
 * Name: esp32p4_lcd_initialize
 *
 * Description:
 *   Bring up the MIPI-DSI bus and register /dev/fb0
 *   (ILI9881C 800x1280 RGB565).  Implemented in esp32p4_lcd.c.
 *
 ****************************************************************************/

#ifdef CONFIG_ESPRESSIF_MIPI_DSI
int esp32p4_lcd_initialize(void);
#endif

/****************************************************************************
 * Name: esp32p4_gt911_initialize
 *
 * Description:
 *   Register the GT911 touchscreen (polling lower half) at /dev/input0 on
 *   I2C1.  Implemented in esp32p4_touch.c.
 *
 *   NOTE(真机): GT911 地址常见 0x5d/0x14, 需先 i2ctool 扫描确认.
 *
 ****************************************************************************/

#ifdef CONFIG_ESPRESSIF_BOARD_GT911
int esp32p4_gt911_initialize(void);
#endif

/****************************************************************************
 * Name: esp32p4_audio_initialize
 *
 * Description:
 *   Bring up the audio path: I2S0 + ES8311 codec (I2C1 control) and register
 *   /dev/audio/pcm0 (playback via PCM decode) and /dev/audio/pcm_in0
 *   (record). Implemented in esp32p4_audio.c.
 *
 * Returned Value:
 *   Zero (OK) is returned on success; a negated errno value is returned on
 *   failure.
 *
 ****************************************************************************/

#if defined(CONFIG_AUDIO) && defined(CONFIG_AUDIO_ES8311) && \
    defined(CONFIG_ESPRESSIF_I2S)
int esp32p4_audio_initialize(void);
#endif

#endif /* __ASSEMBLY__ */
#endif /* __BOARDS_RISCV_ESP32P4_ESP32P4_FUNCTION_EV_BOARD_SRC_ESP32P4_FUNCTION_EV_BOARD_H */
