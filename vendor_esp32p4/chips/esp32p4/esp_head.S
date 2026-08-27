/****************************************************************************
 * vendor/espressif/chip/esp32p4/esp_head.S
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
 * Pre-processor Definitions
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/irq.h>

/****************************************************************************
 * Assembly Macros
 ****************************************************************************/

/****************************************************************************
 * Public Symbols
 ****************************************************************************/

    .global     __start
    .global     _stext

    .section    .vectors, "ax"

/****************************************************************************
 * Name: __start
 *
 * Description:
 *   ESP32-P4 RISC-V HP Core 0 entry point.
 *   This is the first code executed after reset.
 *
 *   Startup sequence:
 *   1. Set up the initial stack pointer
 *   2. Clear the .bss section
 *   3. Copy initialized data from flash to SRAM
 *   4. Jump to C startup code (espressif_start)
 *
 ****************************************************************************/

    .type       __start, @function

__start:
_stext:
    /* Set the stack pointer to the idle thread stack top */

    la      sp, _ebss
    li      t0, CONFIG_IDLETHREAD_STACKSIZE
    add     sp, sp, t0

    /* Clear the .bss section */

    la      t0, _sbss
    la      t1, _ebss

1:
    bgeu    t0, t1, 2f
    sw      zero, 0(t0)
    addi    t0, t0, 4
    j       1b

2:
    /* Copy initialized data from flash to SRAM */

    la      t0, _eronly
    la      t1, _sdata
    la      t2, _edata

3:
    bgeu    t1, t2, 4f
    lw      t3, 0(t0)
    sw      t3, 0(t1)
    addi    t0, t0, 4
    addi    t1, t1, 4
    j       3b

4:
    /* Jump to C startup code */

    jal     x1, __espressif_start

    /* Should never reach here */

5:
    wfi
    j       5b

    .size   __start, . - __start
