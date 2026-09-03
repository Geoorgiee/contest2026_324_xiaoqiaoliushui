/****************************************************************************
 * vendor/espressif/chips/esp32p4/esp32p4_irq.c
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

#include <stdint.h>
#include <assert.h>
#include <nuttx/irq.h>
#include <nuttx/arch.h>
#include <nuttx/board.h>

#include <arch/irq.h>
#include <arch/csr.h>

#include "riscv_internal.h"
#include "hardware/esp32p4_plic.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* The ESP32-P4 uses a standard RISC-V PLIC (Platform-Level Interrupt
 * Controller) for external interrupt routing. The PLIC sits between
 * peripheral interrupt sources and the RISC-V Hart interrupt lines.
 *
 * ESP32-P4 has 64 external interrupt sources routed through the PLIC.
 * The PLIC supports priority-based preemption (7 priority levels).
 *
 * RISC-V external interrupt is connected to MIP.MEIP (bit 11 of the
 * mcause CSR for external interrupt = 11).
 */

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: up_irqinitialize
 *
 * Description:
 *   Initialize the ESP32-P4 PLIC (Platform-Level Interrupt Controller).
 *   This function is called during OS initialization to set up the
 *   interrupt subsystem.
 *
 *   Steps:
 *   1. Disable all PLIC interrupt sources
 *   2. Set all source priorities to 0 (disabled)
 *   3. Set the Hart threshold to 0 (all priorities accepted)
 *   4. Attach the default exception handlers (done by common code)
 *   5. Enable the timer interrupt for system tick
 *   6. Enable global interrupts
 *
 ****************************************************************************/

void up_irqinitialize(void)
{
  int irq;

  /* Step 1 & 2: Disable all interrupts and set priorities to 0.
   *
   * The PLIC has up to PLIC_NIRQS sources (source 0 is reserved).
   * We set priority 0 for all sources, which effectively disables them.
   */

  for (irq = 1; irq < PLIC_NIRQS; irq++)
    {
      plic_set_priority(irq, 0);
    }

  /* Disable all interrupts for Hart 0 M-mode context.
   * Each enable register covers 32 sources (32 bits per word).
   */

  REG_WRITE(PLIC_ENABLE(PLIC_HART0_M) + 0, 0);
  REG_WRITE(PLIC_ENABLE(PLIC_HART0_M) + 4, 0);

  /* Step 3: Set Hart 0 M-mode threshold to 0.
   * This means all interrupts with priority > 0 will be taken.
   */

  plic_set_threshold(PLIC_HART0_M, 0);

  /* Step 4: Attach default exception handlers.
   * The common NuttX code handles irq_attach and g_irqvector
   * initialization via irq_initialize(). We only need to set up
   * the PLIC hardware here.
   */

#ifdef CONFIG_ARCH_MINIMAL_VECTORTABLE
  /* If using minimal vector table, attach the default handlers
   * for the exceptions we care about.
   */

  irq_attach(RISCV_IRQ_IAMISALIGNED, riscv_exception, NULL);
  irq_attach(RISCV_IRQ_IAFAULT, riscv_exception, NULL);
  irq_attach(RISCV_IRQ_IINSTRUCTION, riscv_exception, NULL);
  irq_attach(RISCV_IRQ_BPOINT, riscv_exception, NULL);
  irq_attach(RISCV_IRQ_LAFAULT, riscv_exception, NULL);
  irq_attach(RISCV_IRQ_SAFAULT, riscv_exception, NULL);
#endif

  /* Step 5: Attach and enable the timer interrupt.
   * The system tick timer (HP Timer 0) will generate periodic interrupts.
   */

#ifdef CONFIG_ESP32P4_TIMER
  up_enable_irq(ESP32P4_IRQ_TIMER0);
#endif

  /* Attach the UART console interrupt if configured */

#ifdef CONFIG_ESP32P4_UART0
  up_enable_irq(ESP32P4_IRQ_UART0);
#endif

  /* Step 6: Enable global interrupts.
   * This sets the MIE.MEIE bit to enable external PLIC interrupts,
   * and the MIE.MTIE bit for timer interrupts.
   */

  up_irq_enable();
}

/****************************************************************************
 * Name: up_enable_irq
 *
 * Description:
 *   Enable the interrupt specified by 'irq' in the PLIC.
 *   This sets the corresponding enable bit and assigns a default
 *   priority if the source was previously disabled.
 *
 * Input Parameters:
 *   irq - The IRQ number to enable (must be >= 16, the first PLIC source)
 *
 ****************************************************************************/

void up_enable_irq(int irq)
{
  int context = PLIC_HART0_M;

  DEBUGASSERT(irq >= ESP32P4_IRQ_FIRST && irq < NR_IRQS);

  /* Set priority to 1 if it is currently 0 (disabled).
   * Priority 1 is the lowest non-zero priority. Higher priorities
   * can be set by individual drivers.
   */

  if (REG_READ(PLIC_PRIORITY(irq)) == 0)
    {
      plic_set_priority(irq, 1);
    }

  /* Set the enable bit for this IRQ source.
   * Each enable register covers 32 sources.
   */

  plic_enable_interrupt(context, irq);
}

/****************************************************************************
 * Name: up_disable_irq
 *
 * Description:
 *   Disable the interrupt specified by 'irq' in the PLIC.
 *   This clears the corresponding enable bit. The priority is left
 *   unchanged so it can be quickly re-enabled.
 *
 * Input Parameters:
 *   irq - The IRQ number to disable
 *
 ****************************************************************************/

void up_disable_irq(int irq)
{
  int context = PLIC_HART0_M;

  DEBUGASSERT(irq >= 0 && irq < NR_IRQS);

  /* Clear the enable bit for this IRQ source */

  plic_disable_interrupt(context, irq);

  /* Also set priority to 0 so the PLIC does not consider this source */

  plic_set_priority(irq, 0);
}

/****************************************************************************
 * Name: up_irq_enable
 *
 * Description:
 *   Enable interrupts globally.
 *
 * Input Parameters:
 *   None.
 *
 * Returned Value:
 *   The interrupt state prior to enabling interrupts.
 *
 ****************************************************************************/

irqstate_t up_irq_enable(void)
{
  irqstate_t flags;

  /* Read mstatus & set machine interrupt enable (MIE) in mstatus */

  flags = READ_AND_SET_CSR(CSR_MSTATUS, MSTATUS_MIE);
  return flags;
}

/****************************************************************************
 * Name: riscv_dispatch_irq
 *
 * Description:
 *   Dispatch the IRQ to the appropriate handler.
 *   Called from the RISC-V trap handler when an external interrupt is
 *   detected (mcause = 0x8000000b for M-mode external interrupt).
 *
 *   The dispatch sequence is:
 *   1. Read the PLIC claim register to get the highest-priority pending
 *      interrupt source number.
 *   2. Call riscv_doirq() which handles the common NuttX IRQ dispatch
 *      (including g_irqvector lookup, statistics, etc.)
 *   3. Write the source number back to the claim register (complete).
 *
 * Input Parameters:
 *   mcause - The RISC-V mcause CSR value
 *   regs   - Pointer to the saved register context on the stack
 *
 ****************************************************************************/

void riscv_dispatch_irq(uintreg_t mcause, uintreg_t *regs)
{
  int irq;

  /* Read the claim register to get the highest-priority pending
   * interrupt source for Hart 0 M-mode.
   */

  while ((irq = plic_claim(PLIC_HART0_M)) > 0)
    {
      /* Dispatch through the common NuttX IRQ handler.
       * riscv_doirq handles g_irqvector lookup, interrupt statistics,
       * and context switching.
       */

      regs = riscv_doirq(irq + RISCV_IRQ_ASYNC, regs);

      /* Complete the interrupt by writing back the source number.
       * This tells the PLIC that we are done servicing this interrupt
       * and allows it to re-assert if the source is still pending.
       */

      plic_complete(PLIC_HART0_M, irq);
    }
}
