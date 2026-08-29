# ##############################################################################
# vendor/espressif/chips/esp32p4/CMakeLists.txt
#
# Licensed to the Apache Software Foundation (ASF) under one or more contributor
# license agreements.  See the NOTICE file distributed with this work for
# additional information regarding copyright ownership.  The ASF licenses this
# file to you under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License.  You may obtain a copy of
# the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
# License for the specific language governing permissions and limitations under
# the License.
#
# ##############################################################################

set(SRCS esp32p4_start.c esp32p4_irq.c esp32p4_clockconfig.c esp32p4_serial.c
         esp32p4_gpio.c esp32p4_timerisr.c esp32p4_allocateheap.c)

if(CONFIG_ESP32P4_SPI2 OR CONFIG_ESP32P4_SPI3)
  list(APPEND SRCS esp32p4_spi.c)
endif()

if(CONFIG_ESP32P4_I2C0 OR CONFIG_ESP32P4_I2C1)
  list(APPEND SRCS esp32p4_i2c.c)
endif()

if(CONFIG_ESP32P4_MIPI_DSI)
  list(APPEND SRCS esp32p4_mipi_dsi.c)
endif()

if(CONFIG_ESP32P4_BLE)
  list(APPEND SRCS esp32p4_ble.c)
endif()

if(CONFIG_ESP32P4_WIFI)
  list(APPEND SRCS esp32p4_wifi.c)
endif()

if(CONFIG_ESP32P4_SDMMC)
  list(APPEND SRCS esp32p4_sdmmc.c)
endif()

if(CONFIG_ESP32P4_PSRAM)
  list(APPEND SRCS esp32p4_psram.c)
endif()

target_sources(arch PRIVATE ${SRCS})

target_include_directories(arch PRIVATE ${NUTTX_CHIP_ABS_DIR}/include)
target_include_directories(arch PRIVATE ${NUTTX_CHIP_ABS_DIR}/include/hardware)
