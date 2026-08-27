/****************************************************************************
 * vendor/espressif/chip/esp32p4/chip.h
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

#ifndef __VENDOR_ESPRESSIF_CHIP_ESP32P4_CHIP_H
#define __VENDOR_ESPRESSIF_CHIP_ESP32P4_CHIP_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* ESP32-P4 Memory Map */

#define ESP32P4_SRAM_BASE       0x4ff00000
#define ESP32P4_SRAM_SIZE       (768 * 1024)  /* 768 KB SRAM */

#define ESP32P4_PSRAM_BASE      0x48000000
#define ESP32P4_PSRAM_SIZE      (32 * 1024 * 1024)  /* 32 MB PSRAM */

#define ESP32P4_FLASH_BASE      0x42000000
#define ESP32P4_FLASH_SIZE      (16 * 1024 * 1024)  /* 16 MB Flash */

/* ESP32-P4 CPU Configuration */

#define ESP32P4_HP_CORE_FREQ    400000000  /* 400 MHz HP Core */
#define ESP32P4_LP_CORE_FREQ    40000000   /* 40 MHz LP Core */

/****************************************************************************
 * Public Types
 ****************************************************************************/

/****************************************************************************
 * Public Data
 ****************************************************************************/

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#endif /* __VENDOR_ESPRESSIF_CHIP_ESP32P4_CHIP_H */
