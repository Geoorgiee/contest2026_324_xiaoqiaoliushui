############################################################################
# board/contest_board/src/Makefile
#
# SPDX-License-Identifier: Apache-2.0
#
# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.  The
# ASF licenses this file to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance with the
# License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
# License for the specific language governing permissions and limitations
# under the License.
#
#############################################################################

include $(TOPDIR)/Make.defs

CSRCS = esp32p4_boot.c esp32p4_bringup.c

ifeq ($(CONFIG_DEV_GPIO),y)
  CSRCS += esp32p4_gpio.c
endif

ifeq ($(CONFIG_I2C_DRIVER),y)
  ifeq ($(CONFIG_ESPRESSIF_I2C1_MASTER_MODE),y)
    CSRCS += esp32p4_board_i2c.c
  endif
endif

ifeq ($(CONFIG_BOARDCTL),y)
  CSRCS += esp32p4_appinit.c
  ifeq ($(CONFIG_BOARDCTL_RESET),y)
    CSRCS += esp32p4_reset.c
  endif
endif

# MIPI-DSI LCD (ILI9881C 800x1280): /dev/fb0 via chip-level esp32p4_mipi_dsi.c

ifeq ($(CONFIG_ESPRESSIF_MIPI_DSI),y)
  CSRCS += esp32p4_lcd.c
  # board include dir is resolved from dependency-generation cwd too, so the
  # chip espressif include path must be absolute (TOPDIR = nuttx).
  CFLAGS += ${INCDIR_PREFIX}$(TOPDIR)$(DELIM)arch$(DELIM)risc-v$(DELIM)src$(DELIM)chip$(DELIM)espressif
endif

# GT911 touchscreen (polled lower half on I2C1)

ifeq ($(CONFIG_ESPRESSIF_BOARD_GT911),y)
  CSRCS += esp32p4_touch.c
endif

# Audio (I2S0 + ES8311 codec on I2C1): /dev/audio/pcm0, /dev/audio/pcm_in0
# NOTE: no extra -I needed: "espressif/esp_i2{s,c}.h" resolves through the
# global -I $(ARCH_SRCDIR)/chip (arch/risc-v/src/chip -> board chip dir).

ifeq ($(CONFIG_AUDIO_ES8311),y)
  ifeq ($(CONFIG_ESPRESSIF_I2S),y)
    CSRCS += esp32p4_audio.c
  endif
endif

include $(TOPDIR)/boards/Board.mk
