/****************************************************************************
 * vendor/espressif/chips/esp32p4/include/hardware/esp32p4_plic.h
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

#ifndef __VENDOR_ESPRESSIF_CHIPS_ESP32P4_INCLUDE_HARDWARE_ESP32P4_PLIC_H
#define __VENDOR_ESPRESSIF_CHIPS_ESP32P4_INCLUDE_HARDWARE_ESP32P4_PLIC_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "esp32p4_soc.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* ESP32-P4 uses the standard RISC-V PLIC.
 * The PLIC memory map is:
 *   0x000000 - Source priority registers (4 bytes each, source 0 unused)
 *   0x001000 - Pending bits (read-only)
 *   0x002000 - Enable bits (per context, 0x80 bytes each)
 *   0x200000 - Threshold and claim/complete (per context, 0x1000 bytes each)
 */

#define PLIC_BASE                   DR_REG_PLIC_BASE

/* Source priority register: 4 bytes per source, source 0 is reserved */

#define PLIC_PRIORITY_OFFSET        0x000000
#define PLIC_PRIORITY(source)       (PLIC_BASE + PLIC_PRIORITY_OFFSET + \
                                     ((source) << 2))

/* Pending interrupt register (read-only, 1 bit per source) */

#define PLIC_PENDING_OFFSET         0x001000
#define PLIC_PENDING                (PLIC_BASE + PLIC_PENDING_OFFSET)

/* Interrupt enable register (1 bit per source, context 0 = Hart 0 M-mode) */

#define PLIC_ENABLE_OFFSET          0x002000
#define PLIC_ENABLE(context)        (PLIC_BASE + PLIC_ENABLE_OFFSET + \
                                     ((context) << 7))

/* Threshold and claim/complete for each context */

#define PLIC_THRESHOLD_OFFSET       0x200000
#define PLIC_THRESHOLD(context)     (PLIC_BASE + PLIC_THRESHOLD_OFFSET + \
                                     ((context) << 12))

#define PLIC_CLAIM_OFFSET           0x200004
#define PLIC_CLAIM(context)         (PLIC_BASE + PLIC_CLAIM_OFFSET + \
                                     ((context) << 12))

#define PLIC_COMPLETE(context)      PLIC_CLAIM(context)

/* Hart 0 M-mode context */

#define PLIC_HART0_M                0

/* Number of PLIC sources supported by ESP32-P4 */

#define PLIC_NIRQS                  64

/* Maximum priority level */

#define PLIC_MAX_PRIORITY           7

/****************************************************************************
 * Inline Helper Functions
 ****************************************************************************/

static inline void plic_set_priority(int source, int priority)
{
  REG_WRITE(PLIC_PRIORITY(source), (uint32_t)priority);
}

static inline void plic_set_threshold(int context, int threshold)
{
  REG_WRITE(PLIC_THRESHOLD(context), (uint32_t)threshold);
}

static inline int plic_claim(int context)
{
  return (int)REG_READ(PLIC_CLAIM(context));
}

static inline void plic_complete(int context, int source)
{
  REG_WRITE(PLIC_COMPLETE(context), (uint32_t)source);
}

static inline void plic_enable_interrupt(int context, int source)
{
  unsigned int addr = PLIC_ENABLE(context) + ((source >> 5) << 2);
  uint32_t bit = 1u << (source & 0x1f);
  REG_SET_BIT(addr, bit);
}

static inline void plic_disable_interrupt(int context, int source)
{
  unsigned int addr = PLIC_ENABLE(context) + ((source >> 5) << 2);
  uint32_t bit = 1u << (source & 0x1f);
  REG_CLR_BIT(addr, bit);
}

#endif /* __VENDOR_ESPRESSIF_CHIPS_ESP32P4_INCLUDE_HARDWARE_ESP32P4_PLIC_H */
