# ##############################################################################
# vendor_esp32p4/boards/risc-v/esp32p4/esp32p4-evb/src/CMakeLists.txt
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

set(SRCS esp32p4_bringup.c esp32p4_boot.c)

if(CONFIG_ESP32P4_EVB_SDCARD)
  list(APPEND SRCS esp32p4_sdcard.c)
endif()

if(CONFIG_ESP32P4_TINYML)
  list(APPEND SRCS esp32p4_ml_demo.c)
endif()

if(CONFIG_ESP32P4_LCD_PANEL)
  list(APPEND SRCS esp32p4_lcd.c)
endif()

target_sources(board PRIVATE ${SRCS})
