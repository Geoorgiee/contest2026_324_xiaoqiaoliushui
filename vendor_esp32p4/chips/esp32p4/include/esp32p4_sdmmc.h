/****************************************************************************
 * vendor_esp32p4/chips/esp32p4/include/esp32p4_sdmmc.h
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

#ifndef __VENDOR_ESP32P4_CHIPS_ESP32P4_INCLUDE_ESP32P4_SDMMC_H
#define __VENDOR_ESP32P4_CHIPS_ESP32P4_INCLUDE_ESP32P4_SDMMC_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/sdio.h>

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Name: sdmmc_initialize
 *
 * Description:
 *   Initialize the SDMMC controller and return an SDIO device interface.
 *
 *   This function:
 *   1. Configures the SDMMC host controller registers
 *   2. Sets up GPIO pin muxing for the SDMMC bus (CLK, CMD, D0-D3)
 *   3. Initializes the SDMMC DMA engine (if enabled)
 *   4. Performs SD card detection and initialization
 *   5. Returns a pointer to the sdio_dev_s interface
 *
 * Input Parameters:
 *   slot - SDMMC slot number (0 for the primary slot)
 *
 * Returned Value:
 *   A pointer to the sdio_dev_s interface on success;
 *   NULL on failure.
 *
 ****************************************************************************/

struct sdio_dev_s *sdmmc_initialize(int slot);

#endif /* __VENDOR_ESP32P4_CHIPS_ESP32P4_INCLUDE_ESP32P4_SDMMC_H */
