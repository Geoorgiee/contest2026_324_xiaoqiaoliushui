/****************************************************************************
 * board/contest_board/src/esp32p4_audio.c
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
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#if defined(CONFIG_AUDIO) && defined(CONFIG_ESPRESSIF_I2S) && \
    defined(CONFIG_AUDIO_ES8311)

#include <debug.h>
#include <errno.h>

#include <nuttx/audio/audio.h>
#include <nuttx/audio/i2s.h>
#include <nuttx/audio/pcm.h>
#include <nuttx/audio/es8311.h>
#include <nuttx/i2c/i2c_master.h>

#include "espressif/esp_i2c.h"
#include "espressif/esp_i2s.h"
#include <arch/board/board.h>

#include "esp32p4-function-ev-board.h"

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* One lower-half instance per ES8311 direction (playback / record) */

static struct es8311_lower_s g_es8311_lower[2];

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: esp32p4_audio_initialize
 *
 * Description:
 *   Bring up the audio path of the ESP32-P4-Function-EV-Board V1.6:
 *
 *     I2S0 (master, digital pins per CONFIG_ESPRESSIF_I2S0_*PIN)
 *      -> ES8311 mono codec (I2C1 control @ 0x18, SDA=GPIO7, SCL=GPIO8)
 *      -> NS4150B class-D PA -> speaker
 *
 *   Registers:
 *     /dev/audio/pcm0     playback  (pcm decode front-end + es8311)
 *     /dev/audio/pcm_in0  record    (es8311)
 *     /dev/i2schar0       raw I2S character device (test only, optional)
 *
 * Returned Value:
 *   Zero (OK) on success; a negated errno value on failure.
 *
 ****************************************************************************/

int esp32p4_audio_initialize(void)
{
  FAR struct es8311_lower_s *lower;
  FAR struct i2s_dev_s *i2s;
  FAR struct i2c_master_s *i2c;
  FAR struct audio_lowerhalf_s *es8311;
  int ret;

  /* Get an instance of the I2S interface (digital audio to ES8311) */

  i2s = esp_i2sbus_initialize(ESPRESSIF_I2S0);
  if (i2s == NULL)
    {
      auderr("ERROR: Failed to initialize I2S\n");
      return -ENODEV;
    }

  /* Get an instance of the I2C interface (codec control path) */

  i2c = esp_i2cbus_initialize(ESPRESSIF_I2C1);
  if (i2c == NULL)
    {
      auderr("ERROR: Failed to initialize I2C1\n");
      return -ENODEV;
    }

#ifdef CONFIG_AUDIO_I2SCHAR
  /* Optionally expose a raw I2S character device at /dev/i2schar0.
   * Intended only for I2S loopback/signaling probing; not needed for
   * normal audio playback.
   */

  ret = i2schar_register(i2s, 0);
  if (ret < 0)
    {
      auderr("ERROR: i2schar_register failed: %d\n", ret);
      return ret;
    }
#endif

  /* Playback: es8311 -> (optional pcm decode front end) -> /dev/audio/pcm0 */

  g_es8311_lower[0].address = CONFIG_ESPRESSIF_ES8311_I2C_ADDRESS;
  g_es8311_lower[0].frequency = CONFIG_ESPRESSIF_ES8311_I2C_FREQUENCY;

  es8311 = es8311_initialize(i2c, i2s, &g_es8311_lower[0]);
  if (es8311 == NULL)
    {
      auderr("ERROR: Failed to initialize the ES8311 (playback)\n");
      return -ENODEV;
    }

#ifdef CONFIG_AUDIO_FORMAT_PCM
  /* Embed the ES8311/I2S into a PCM decoder so we have a PCM front end */

  es8311 = pcm_decode_initialize(es8311);
  if (es8311 == NULL)
    {
      auderr("ERROR: Failed to create the PCM decoder\n");
      return -ENODEV;
    }
#endif

  ret = audio_register("pcm0", es8311);
  if (ret < 0)
    {
      auderr("ERROR: Failed to register /dev/audio/pcm0: %d\n", ret);
      return ret;
    }

  /* Record: es8311 -> /dev/audio/pcm_in0
   * The second instance reuses the same I2S/I2C hardware; the es8311
   * driver programs the ADC path on the record session start.
   */

  lower = &g_es8311_lower[1];
  lower->address = CONFIG_ESPRESSIF_ES8311_I2C_ADDRESS;
  lower->frequency = CONFIG_ESPRESSIF_ES8311_I2C_FREQUENCY;

  es8311 = es8311_initialize(i2c, i2s, lower);
  if (es8311 == NULL)
    {
      auderr("ERROR: Failed to initialize the ES8311 (record)\n");
      return -ENODEV;
    }

  ret = audio_register("pcm_in0", es8311);
  if (ret < 0)
    {
      auderr("ERROR: Failed to register /dev/audio/pcm_in0: %d\n", ret);
      return ret;
    }

  /* TODO(真机校准):
   * 1. 导通 NS4150B 功放使能脚 BSP_POWER_AMP_IO = GPIO53 (esp-bsp
   *    esp32_p4_function_ev_board.h), 若无内置功放使能逻辑则需在
   *    播放前拉高该 GPIO (可通过 /dev/gpio 或在此处直接配置).
   * 2. 校准 I2S pin map: 以下为 esp-bsp 官方 pin 定义 (do the
   *    CONFIG_ESPRESSIF_I2S0_*PIN entries in the defconfig match?).
   * 3. 若 I2S RX (mic path) 与 TX 复用同一 I2S0 时噪声明显, 考虑
   *    ES8311_SRC_MCLK / BCLK 选择.
   */

  return OK;
}

#endif /* CONFIG_AUDIO && CONFIG_ESPRESSIF_I2S && CONFIG_AUDIO_ES8311 */
