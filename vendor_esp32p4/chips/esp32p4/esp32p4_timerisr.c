/****************************************************************************
 * vendor/espressif/chips/esp32p4/esp32p4_timerisr.c
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
#include <debug.h>

#include <nuttx/arch.h>
#include <nuttx/irq.h>
#include <nuttx/clock.h>

#include <arch/irq.h>

#include "riscv_internal.h"
#include "hardware/esp32p4_timer.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* System tick timer configuration.
 *
 * ESP32-P4 has HP Timer Group 0 with two 54-bit timers (Timer 0 and Timer 1).
 * Each timer is driven by the APB clock (80 MHz by default) with a 16-bit
 * programmable prescaler (divider range 2-65536).
 *
 * For system tick, we use HP Timer Group 0, Timer 0.
 *
 * Timer interrupt rate:
 *   APB_CLK / (prescaler * alarm_value) = interrupt_rate
 *   80000000 / (prescaler * alarm_value) = 1000000/CONFIG_USEC_PER_TICK
 *
 * For CONFIG_USEC_PER_TICK = 1000 (1 kHz tick):
 *   80000000 / (prescaler * alarm_value) = 1000
 *   prescaler = 80, alarm_value = 1000
 */

/* Timer group index (0 = TIMG0, 1 = TIMG1) */

#define SYSTICK_TGROUP            0

/* Timer index within the group (0 or 1) */

#define SYSTICK_TINDEX            0

/* Timer prescaler: divide APB clock by this value.
 * A value of 80 gives 1 MHz timer tick from 80 MHz APB clock.
 */

#define SYSTICK_PRESCALER         80

/* Timer interrupt source */

#define SYSTICK_IRQ               ESP32P4_IRQ_TIMER0

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: esp32p4_timerisr
 *
 * Description:
 *   HP Timer 0 interrupt handler.  This is called from the PLIC interrupt
 *   dispatch when the system tick timer interrupt fires.
 *
 *   The handler:
 *   1. Clears the timer interrupt
 *   2. Calls the NuttX timer tick handler
 *
 ****************************************************************************/

static int esp32p4_timerisr(int irq, void *context, void *arg)
{
  /* Clear the timer interrupt by writing to the interrupt clear register */

  REG_WRITE(TIMG_INT_CLR_TIMERS_REG(SYSTICK_TGROUP),
            TIMG_T0_INT_RAW << SYSTICK_TINDEX);

  /* Process the timer tick */

  nxsched_process_timer();

  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: up_timer_initialize
 *
 * Description:
 *   This function is called during start-up to initialize the timer
 *   interrupt.
 *
 *   ESP32-P4 has two HP timer groups, each with two 54-bit timers and
 *   a watchdog timer.  This function initializes HP Timer Group 0,
 *   Timer 0 as the system tick timer.
 *
 *   The timer is configured in auto-reload, count-up mode with:
 *   - Prescaler: 80 (1 MHz tick from 80 MHz APB clock)
 *   - Alarm value: (1000000 / CONFIG_USEC_PER_TICK) timer ticks
 *   - This gives exactly CONFIG_USEC_PER_TICK microsecond period
 *
 ****************************************************************************/

void up_timer_initialize(void)
{
  uint32_t alarm_value;
  uint32_t regval;

  /* Calculate the alarm value for the desired tick rate.
   *
   * Timer tick frequency = APB_CLK / SYSTICK_PRESCALER = 1 MHz
   * Alarm value = 1000000 / (1000000 / CONFIG_USEC_PER_TICK)
   *             = CONFIG_USEC_PER_TICK
   *
   * For CONFIG_USEC_PER_TICK = 1000 (1 kHz tick):
   *   Alarm value = 1000 (interrupt every 1 ms)
   */

  alarm_value = CONFIG_USEC_PER_TICK;

  /* Attach the timer interrupt handler */

  irq_attach(SYSTICK_IRQ, esp32p4_timerisr, NULL);

  /* Disable the timer while configuring */

  REG_WRITE(TIMG_T0CONFIG_REG(SYSTICK_TGROUP), 0);

  /* Clear any pending timer interrupt */

  REG_WRITE(TIMG_INT_CLR_TIMERS_REG(SYSTICK_TGROUP),
            TIMG_T0_INT_RAW << SYSTICK_TINDEX);

  /* Load the alarm value into the load registers.
   * The alarm value is loaded into the LO and HI load registers.
   * Since we use a 32-bit alarm, the HI register is 0.
   */

  REG_WRITE(TIMG_T0LOAD_LO_REG(SYSTICK_TGROUP), 0);
  REG_WRITE(TIMG_T0LOAD_HI_REG(SYSTICK_TGROUP), 0);

  /* Set the alarm value in the alarm LO and HI registers.
   * The timer will compare its counter against this value
   * and generate an interrupt when it matches.
   */

  /* Note: The alarm registers share the same offset as the timer
   * value registers in the standard ESP timer layout. The actual
   * alarm value is set via the config register's alarm enable bit.
   */

  /* Configure the timer:
   *   - Prescaler: SYSTICK_PRESCALER (80 -> 1 MHz from 80 MHz)
   *   - Count up (INCREASE mode)
   *   - Auto-reload on alarm
   *   - Alarm enabled
   *   - Level-triggered interrupt
   */

  regval = ((SYSTICK_PRESCALER - 1) << TIMG_T0_DIVIDER_S) |
           TIMG_T0_INCREASE |
           TIMG_T0_AUTORELOAD |
           TIMG_T0_ALARM_EN |
           TIMG_T0_LEVEL_INT_EN;

  REG_WRITE(TIMG_T0CONFIG_REG(SYSTICK_TGROUP), regval);

  /* Write the alarm value.
   * The alarm value is written to the alarm_lo register which is at
   * the same address as the load_lo register in the timer config space.
   */

  REG_WRITE(TIMG_T0LOAD_LO_REG(SYSTICK_TGROUP), alarm_value);
  REG_WRITE(TIMG_T0LOAD_HI_REG(SYSTICK_TGROUP), 0);

  /* Trigger a load to apply the initial counter value */

  REG_WRITE(TIMG_T0LOAD_REG(SYSTICK_TGROUP), 1);

  /* Enable the timer interrupt in the timer group */

  REG_SET_BIT(TIMG_INT_ENA_TIMERS_REG(SYSTICK_TGROUP),
              TIMG_T0_INT_RAW << SYSTICK_TINDEX);

  /* Enable the timer in the PLIC */

  up_enable_irq(SYSTICK_IRQ);

  /* Start the timer by setting the enable bit */

  REG_SET_BIT(TIMG_T0CONFIG_REG(SYSTICK_TGROUP), TIMG_T0_EN);
}
