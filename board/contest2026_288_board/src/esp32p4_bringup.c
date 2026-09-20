/****************************************************************************
 * board/contest_board/src/esp32p4_bringup.c
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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <debug.h>
#include <sys/mount.h>

#include <nuttx/fs/fs.h>

#include "esp32p4-function-ev-board.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: esp_bringup
 *
 * Description:
 *   Perform architecture-specific initialization for the L0 (minimal NSH)
 *   baseline.  Peripheral bring-up (I2C/SPI/LEDC/RMT/...) is intentionally
 *   omitted here and added incrementally once the console is up.
 *
 * Returned Value:
 *   Zero (OK) is returned on success; a negated errno value on failure.
 *
 ****************************************************************************/

int esp_bringup(void)
{
#if defined(CONFIG_I2C_DRIVER) && \
    defined(CONFIG_ESPRESSIF_I2C1_MASTER_MODE)
  int i2c_ret;
#endif
#ifdef CONFIG_ESPRESSIF_MIPI_DSI
  int lcd_ret;
#endif
#ifdef CONFIG_ESPRESSIF_BOARD_GT911
  int tch_ret;
#endif
#if defined(CONFIG_AUDIO) && defined(CONFIG_AUDIO_ES8311) && \
    defined(CONFIG_ESPRESSIF_I2S)
  int aud_ret;
#endif
  int ret = OK;

#ifdef CONFIG_FS_PROCFS
  /* Mount the procfs file system */

  ret = nx_mount(NULL, "/proc", "procfs", 0, NULL);
  if (ret < 0)
    {
      _err("Failed to mount procfs at /proc: %d\n", ret);
    }
#endif

#ifdef CONFIG_FS_TMPFS
  /* Mount the tmpfs file system */

  ret = nx_mount(NULL, CONFIG_LIBC_TMPDIR, "tmpfs", 0, NULL);
  if (ret < 0)
    {
      _err("Failed to mount tmpfs at %s: %d\n", CONFIG_LIBC_TMPDIR, ret);
    }
#endif

#ifdef CONFIG_DEV_GPIO
  /* 初始化 GPIO 输出设备，注册 /dev/gpio0
   * 只有 defconfig 里 CONFIG_DEV_GPIO=y 时才会编译这段 */

  ret = esp_gpio_init();
  if (ret < 0)
    {
      _err("Failed to initialize GPIO Driver: %d\n", ret);
    }
#endif

#if defined(CONFIG_I2C_DRIVER) && \
    defined(CONFIG_ESPRESSIF_I2C1_MASTER_MODE)
  i2c_ret = board_i2c_init();
  if (i2c_ret < 0)
    {
      _err("Failed to initialize I2C Driver: %d\n", i2c_ret);
      if (ret >= 0)
        {
          ret = i2c_ret;
        }
    }
#endif

#ifdef CONFIG_ESPRESSIF_MIPI_DSI
  /* MIPI-DSI LCD (ILI9881C 800x1280): bring up the DSI bus/PHY and
   * register /dev/fb0.  Failures only log; NSH stays up regardless.
   */

  lcd_ret = esp32p4_lcd_initialize();
  if (lcd_ret < 0)
    {
      _err("Failed to initialize LCD: %d\n", lcd_ret);
    }
#endif

#ifdef CONFIG_ESPRESSIF_BOARD_GT911
  /* GT911 touchscreen on I2C1 (polled lower half -> /dev/input0).
   * TODO(真机): 首次 bring-up 先用 i2ctool 扫描确认地址 (0x5d/0x14).
   */

  tch_ret = esp32p4_gt911_initialize();
  if (tch_ret < 0)
    {
      _err("Failed to initialize GT911 touchscreen: %d\n", tch_ret);
    }
#endif

#if defined(CONFIG_AUDIO) && defined(CONFIG_AUDIO_ES8311) && \
    defined(CONFIG_ESPRESSIF_I2S)
  /* Audio: I2S0 + ES8311 codec (I2C1 control @ 0x18).
   * - /dev/audio/pcm0   playback
   * - /dev/audio/pcm_in0 record
   * Failures only log; NSH stays up regardless.
   * TODO(真机校准): see esp32p4_audio.c and board.h (PA GPIO53 etc).
   */

  aud_ret = esp32p4_audio_initialize();
  if (aud_ret < 0)
    {
      _err("Failed to initialize audio: %d\n", aud_ret);
    }
#endif

  return ret;
}
