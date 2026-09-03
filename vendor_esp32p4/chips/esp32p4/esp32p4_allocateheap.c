/****************************************************************************
 * vendor/espressif/chips/esp32p4/esp32p4_allocateheap.c
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

#include <nuttx/arch.h>
#include <nuttx/mm/mm.h>
#include <nuttx/board.h>

#include "chip.h"
#include "esp32p4_psram.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* ESP32-P4 Memory Regions:
 *
 * HP SRAM:
 *   Base: 0x4FF00000
 *   Size: 768 KB (0xC0000)
 *   Used by: .data, .bss, IDLE thread stack, heap
 *
 * PSRAM (if available):
 *   Base: 0x48000000 (through cache mapping)
 *   Size: Up to 32 MB
 *   Used by: Additional heap region
 *
 * Flash (XIP):
 *   Base: 0x42000000 (through cache mapping)
 *   Size: Up to 16 MB
 *   Used by: .text, .rodata, .init_array
 */

#define ESP32P4_SRAM_START      ESP32P4_SRAM_BASE
#define ESP32P4_SRAM_END        (ESP32P4_SRAM_BASE + ESP32P4_SRAM_SIZE)

#ifdef CONFIG_ESP32P4_PSRAM
#define ESP32P4_PSRAM_START     ESP32P4_PSRAM_BASE
#define ESP32P4_PSRAM_END       (ESP32P4_PSRAM_BASE + ESP32P4_PSRAM_SIZE)
#endif

/****************************************************************************
 * Public Data
 ****************************************************************************/

/* Defined in esp32p4_start.c */

extern const uintptr_t g_idle_topstack;

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: riscv_addregion
 *
 * Description:
 *   This function is called during architecture initialization to add
 *   additional memory regions to the heap.  For ESP32-P4, this adds
 *   PSRAM as an additional heap region when available.
 *
 ****************************************************************************/

void riscv_addregion(void)
{
#ifdef CONFIG_ESP32P4_PSRAM
  /* Add PSRAM as an additional heap region if available */

  if (ESP32P4_PSRAM_END > ESP32P4_PSRAM_START)
    {
      mm_addregion(g_mmheap,
                   (FAR void *)ESP32P4_PSRAM_START,
                   ESP32P4_PSRAM_END - ESP32P4_PSRAM_START);
    }
#endif
}

/****************************************************************************
 * Name: up_allocate_heap
 *
 * Description:
 *   This function will be called to dynamically set aside the heap region.
 *
 *   - For the normal "flat" build, this function returns the size of the
 *     single heap.
 *   - For the protected build (CONFIG_BUILD_PROTECTED=y) with both kernel-
 *     and user-space heaps (CONFIG_MM_KERNEL_HEAP=y), this function
 *     provides the size of the unprotected, user-space heap.
 *   - For the kernel build (CONFIG_BUILD_KERNEL=y), this function provides
 *     the size of the protected, kernel-space heap.
 *
 *   The following memory map is assumed for the flat build:
 *
 *     .text region    Flash (XIP, read-only)
 *     .rodata region  Flash (XIP, read-only)
 *     .data region    SRAM (copied from flash at startup), size at link time
 *     .bss  region    SRAM (zeroed at startup), size at link time
 *     IDLE thread stack  SRAM, size CONFIG_IDLETHREAD_STACKSIZE
 *     Heap            SRAM (and PSRAM if enabled), extends to end of memory
 *
 ****************************************************************************/

void up_allocate_heap(FAR void **heap_start, size_t *heap_size)
{
  /* Start heap after the BSS section and IDLE thread stack.
   * g_idle_topstack is computed in esp32p4_start.c as:
   *   _ebss + CONFIG_IDLETHREAD_STACKSIZE
   */

  *heap_start = (FAR void *)(g_idle_topstack);

  /* Calculate heap size: end of available memory - heap start.
   *
   * In the default (flat) build without PSRAM, the heap occupies
   * the remaining HP SRAM after the BSS and IDLE stack.
   *
   * With PSRAM enabled, the primary heap uses PSRAM which provides
   * much more space. The SRAM can optionally be added as a second
   * region via up_addregion() for faster access.
   */

#ifdef CONFIG_ESP32P4_PSRAM
  /* Primary heap in PSRAM (much larger, ~32 MB) */

  *heap_size = ESP32P4_PSRAM_END - g_idle_topstack;
#else
  /* Primary heap in HP SRAM */

  *heap_size = ESP32P4_SRAM_END - g_idle_topstack;
#endif
}

/****************************************************************************
 * Name: up_addregion
 *
 * Description:
 *   Memory may be added in non-contiguous chunks.  Additional chunks are
 *   added by calling this function.
 *
 *   When PSRAM is enabled, the primary heap is in PSRAM (up to 32 MB).
 *   The remaining HP SRAM space (typically small but fast) is added as
 *   a secondary heap region.
 *
 ****************************************************************************/

#if CONFIG_MM_REGIONS > 1
void up_addregion(void)
{
  /* When PSRAM is the primary heap, add HP SRAM as a secondary region.
   *
   * The SRAM region is faster for time-critical allocations because it
   * does not go through the cache subsystem.  The NuttX heap manager
   * will use the first (default) region unless the allocation explicitly
   * targets a specific region.
   */

#ifdef CONFIG_ESP32P4_PSRAM
  /* Add the HP SRAM region that is not used by BSS/stack.
   *
   * The start address is g_idle_topstack (same as the PSRAM heap start).
   * The SRAM extends from ESP32P4_SRAM_BASE to ESP32P4_SRAM_END.
   * However, the linker places .data and .bss at the start of SRAM,
   * so the available SRAM for heap is from g_idle_topstack to SRAM_END.
   */

  if (ESP32P4_SRAM_END > g_idle_topstack)
    {
      mm_addregion(g_mmheap,
                   (FAR void *)g_idle_topstack,
                   ESP32P4_SRAM_END - g_idle_topstack);
    }
#endif
}
#endif
