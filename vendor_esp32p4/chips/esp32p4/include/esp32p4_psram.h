/****************************************************************************
 * vendor/espressif/chips/esp32p4/include/esp32p4_psram.h
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

#ifndef __VENDOR_ESPRESSIF_CHIPS_ESP32P4_INCLUDE_ESP32P4_PSRAM_H
#define __VENDOR_ESPRESSIF_CHIPS_ESP32P4_INCLUDE_ESP32P4_PSRAM_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdint.h>
#include <stdbool.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* PSRAM Memory Map */

#define ESP32P4_PSRAM_BASE          0x48000000
#define ESP32P4_PSRAM_MAX_SIZE      (64 * 1024 * 1024)  /* 64 MB max */

/* MMU Configuration */

#define ESP32P4_MMU_PAGE_SIZE       (64 * 1024)         /* 64 KB pages */
#define ESP32P4_MMU_PAGE_MASK       (ESP32P4_MMU_PAGE_SIZE - 1)
#define ESP32P4_MMU_PAGE_SHIFT      16
#define ESP32P4_MMU_ENTRY_COUNT     1024

/* PSRAM Clock Configuration */

#define ESP32P4_PSRAM_CLK_MPLL      400000000   /* 400 MHz MPLL */
#define ESP32P4_PSRAM_CLK_80MHZ     80000000    /* 80 MHz init clock */
#define ESP32P4_PSRAM_CLK_200MHZ    200000000   /* 200 MHz max OPI */

/* PSRAM Status Codes */

#define ESP32P4_PSRAM_OK            0
#define ESP32P4_PSRAM_ERR_NOT_FOUND -1
#define ESP32P4_PSRAM_ERR_INIT      -2
#define ESP32P4_PSRAM_ERR_MMAP      -3
#define ESP32P4_PSRAM_ERR_DMA       -4
#define ESP32P4_PSRAM_ERR_TEST      -5

/****************************************************************************
 * Public Types
 ****************************************************************************/

/* PSRAM configuration structure */

struct esp32p4_psram_config_s
{
  uint32_t size;           /* PSRAM size in bytes */
  uint32_t clk_freq;       /* Clock frequency in Hz */
  bool     opi_mode;       /* true for OPI, false for QPI */
  bool     ecc_enable;     /* Enable ECC */
  bool     self_test;      /* Run self-test on init */
};

/* PSRAM statistics */

struct esp32p4_psram_stats_s
{
  uint32_t total_size;     /* Total PSRAM size in bytes */
  uint32_t used_size;      /* Used PSRAM size in bytes */
  uint32_t free_size;      /* Free PSRAM size in bytes */
  uint32_t dma_capable;    /* DMA-capable size in bytes */
  uint32_t error_count;    /* Number of errors detected */
};

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Name: esp32p4_psram_init
 *
 * Description:
 *   Initialize the PSRAM subsystem. This function:
 *   1. Enables MPLL clock (400 MHz)
 *   2. Configures MSPI controller for PSRAM access
 *   3. Initializes PSRAM chip (reset, ID read, OPI mode entry)
 *   4. Configures MMU page table for PSRAM mapping
 *   5. Optionally runs self-test
 *
 * Input Parameters:
 *   config - PSRAM configuration (NULL for defaults)
 *
 * Returned Value:
 *   0 on success, negative errno on failure
 *
 ****************************************************************************/

int esp32p4_psram_init(const struct esp32p4_psram_config_s *config);

/****************************************************************************
 * Name: esp32p4_psram_is_initialized
 *
 * Description:
 *   Check if PSRAM is initialized and available.
 *
 * Returned Value:
 *   true if PSRAM is initialized, false otherwise
 *
 ****************************************************************************/

bool esp32p4_psram_is_initialized(void);

/****************************************************************************
 * Name: esp32p4_psram_get_size
 *
 * Description:
 *   Get the size of the initialized PSRAM.
 *
 * Returned Value:
 *   PSRAM size in bytes, or 0 if not initialized
 *
 ****************************************************************************/

uint32_t esp32p4_psram_get_size(void);

/****************************************************************************
 * Name: esp32p4_psram_get_stats
 *
 * Description:
 *   Get PSRAM usage statistics.
 *
 * Input Parameters:
 *   stats - Pointer to store statistics
 *
 * Returned Value:
 *   0 on success, negative errno on failure
 *
 ****************************************************************************/

int esp32p4_psram_get_stats(struct esp32p4_psram_stats_s *stats);

/****************************************************************************
 * Name: esp32p4_psram_self_test
 *
 * Description:
 *   Run a self-test on the PSRAM. This writes patterns to memory
 *   and verifies them. The test is destructive - it overwrites
 *   the first 4KB of PSRAM.
 *
 * Returned Value:
 *   0 on success, negative errno on failure
 *
 ****************************************************************************/

int esp32p4_psram_self_test(void);

/****************************************************************************
 * Name: esp32p4_psram_dma_test
 *
 * Description:
 *   Test DMA access to PSRAM. This verifies that the DMA controller
 *   can read and write to PSRAM correctly.
 *
 * Returned Value:
 *   0 on success, negative errno on failure
 *
 ****************************************************************************/

int esp32p4_psram_dma_test(void);

/****************************************************************************
 * Name: esp32p4_psram_mmap
 *
 * Description:
 *   Map a region of PSRAM into the CPU address space.
 *
 * Input Parameters:
 *   phys_addr - Physical PSRAM address (0-based offset)
 *   size      - Size of region to map
 *   vaddr     - Pointer to store virtual address
 *
 * Returned Value:
 *   0 on success, negative errno on failure
 *
 ****************************************************************************/

int esp32p4_psram_mmap(uint32_t phys_addr, uint32_t size, void **vaddr);

/****************************************************************************
 * Name: esp32p4_psram_get_heap_base
 *
 * Description:
 *   Get the base address of the PSRAM heap region.
 *
 * Returned Value:
 *   Base address of PSRAM heap, or NULL if not initialized
 *
 ****************************************************************************/

void *esp32p4_psram_get_heap_base(void);

/****************************************************************************
 * Name: esp32p4_psram_get_heap_size
 *
 * Description:
 *   Get the size of the PSRAM heap region.
 *
 * Returned Value:
 *   Size of PSRAM heap in bytes, or 0 if not initialized
 *
 ****************************************************************************/

uint32_t esp32p4_psram_get_heap_size(void);

#endif /* __VENDOR_ESPRESSIF_CHIPS_ESP32P4_INCLUDE_ESP32P4_PSRAM_H */
