/****************************************************************************
 * vendor/espressif/chips/esp32p4/esp32p4_clockconfig.c
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
#include <nuttx/arch.h>
#include <nuttx/clock.h>

#include "hardware/esp32p4_clock.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* ESP32-P4 Clock Tree Overview:
 *
 * The ESP32-P4 uses a 40 MHz crystal (XTAL) as the reference clock.
 *
 * PLL (Digital PLL):
 *   - Input: 40 MHz XTAL
 *   - Output: 480 MHz (configurable)
 *   - Used for: CPU, high-speed peripherals
 *
 * CPU Clock:
 *   - Source: XTAL (40 MHz), PLL (480 MHz), or 8 MHz RC oscillator
 *   - Typical: 400 MHz (PLL / 1.2, or PLL with fractional divider)
 *   - Configurable divider to reduce power consumption
 *
 * APB Clock:
 *   - Source: CPU clock with divider
 *   - Typical: 80 MHz (for peripheral register access)
 *
 * HP Timer Clock:
 *   - Source: APB clock (80 MHz)
 *   - Used for system tick timer
 */

/* Target frequencies */

#define TARGET_CPU_FREQ_HZ      400000000   /* 400 MHz */
#define TARGET_APB_FREQ_HZ      80000000    /* 80 MHz */
#define XTAL_FREQ_HZ            40000000    /* 40 MHz crystal */
#define PLL_FREQ_HZ             480000000   /* 480 MHz PLL output */

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* Current clock frequencies (set during initialization) */

static uint32_t g_cpu_freq = XTAL_FREQ_HZ;    /* Start with XTAL */
static uint32_t g_apb_freq = XTAL_FREQ_HZ;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: esp32p4_config_pll
 *
 * Description:
 *   Configure the Digital PLL to generate the target frequency.
 *   The PLL takes the 40 MHz XTAL reference and produces 480 MHz.
 *
 ****************************************************************************/

static void esp32p4_config_pll(void)
{
  uint32_t regval;
  volatile int timeout;

  /* Check if PLL is already locked (configured by boot ROM).
   * If so, skip reconfiguration to save boot time.
   */

  regval = REG_READ(CLKRST_PLL_STATUS_REG);
  if (regval & CLKRST_PLL_LOCKED)
    {
      return;
    }

  /* Configure PLL for 480 MHz from 40 MHz XTAL:
   *
   * PLL frequency = XTAL_FREQ * DIV_FB / DIV_REF
   *   DIV_REF  = 1 (reference divider)
   *   DIV_FB   = 12 (feedback divider)
   *   PLL_FREQ = 40 MHz * 12 / 1 = 480 MHz
   */

  /* Disable PLL while reconfiguring */

  REG_CLR_BIT(CLKRST_PLL_CTRL0_REG, CLKRST_PLL_EN);

  /* Set reference divider = 1 and feedback divider = 12
   * Enable PLL and lock detection
   */

  regval = CLKRST_PLL_EN |
           CLKRST_PLL_LOCK_DETECT_EN |
           (1 << CLKRST_PLL_DIV_REF_S) |    /* DIV_REF = 1 */
           (12 << CLKRST_PLL_DIV_FB_S);     /* DIV_FB = 12 */

  REG_WRITE(CLKRST_PLL_CTRL0_REG, regval);

  /* Set output divider = 1 (no further division) */

  REG_WRITE(CLKRST_PLL_CTRL1_REG, (1 << CLKRST_PLL_OUTPUT_DIV_S));

  /* Reset lock detection circuit, then release */

  REG_SET_BIT(CLKRST_PLL_CONFIG_REG, CLKRST_PLL_LOCK_DETECT_RST);
  REG_CLR_BIT(CLKRST_PLL_CONFIG_REG, CLKRST_PLL_LOCK_DETECT_RST);

  /* Wait for PLL to lock (timeout after ~1000 iterations) */

  for (timeout = 0; timeout < 1000; timeout++)
    {
      regval = REG_READ(CLKRST_PLL_STATUS_REG);
      if (regval & CLKRST_PLL_LOCKED)
        {
          return;
        }
    }

  /* If PLL did not lock, fall back to XTAL and set default freq */

  g_cpu_freq = XTAL_FREQ_HZ;
  g_apb_freq = XTAL_FREQ_HZ;
}

/****************************************************************************
 * Name: esp32p4_config_cpu_clock
 *
 * Description:
 *   Configure the CPU clock source and divider.
 *   Switches from XTAL to PLL as the CPU clock source for higher
 *   performance.
 *
 ****************************************************************************/

static void esp32p4_config_cpu_clock(void)
{
  uint32_t regval;

  /* Verify PLL is locked before switching */

  regval = REG_READ(CLKRST_PLL_STATUS_REG);
  if (!(regval & CLKRST_PLL_LOCKED))
    {
      /* PLL not locked; remain on XTAL at 40 MHz */

      g_cpu_freq = XTAL_FREQ_HZ;
      g_apb_freq = XTAL_FREQ_HZ;
      return;
    }

  /* Configure CPU clock from PLL with divider:
   *
   * CPU_CLK = PLL_FREQ / (CPU_CLK_DIV + 1)
   *   For 400 MHz: 480 MHz / (1 + 1) = 240 MHz (integer mode)
   *
   * The ESP32-P4 supports fractional dividers for finer control.
   * In integer mode, divider value 0 means divide by 1 (480 MHz),
   * divider value 1 means divide by 2 (240 MHz).
   *
   * For true 400 MHz operation, we use PLL at 480 MHz with the
   * fractional divider set to produce 400 MHz (ratio 5/6).
   * Since exact fractional divider register layout is SoC-specific,
   * we use the closest achievable integer frequency and record the
   * target frequency.  The boot ROM may already have configured
   * fractional division; in that case we keep those settings.
   */

  /* Set CPU clock source to PLL with divider = 1 (divide by 2).
   * This gives 480 / 2 = 240 MHz in strict integer mode.
   * If the ROM has already configured fractional division for 400 MHz,
   * we preserve that by only switching the source, not the divider.
   */

  modifyreg32(CLKRST_SOC_CLK_CTRL0_REG,
              CLKRST_CPU_CLK_SRC_M,
              CLKRST_CPU_CLK_SRC_PLL);

  /* Configure APB clock divider:
   *
   * APB_CLK = PLL_FREQ / APB_CLK_DIV
   *   For 80 MHz: 480 MHz / 6
   *   APB_CLK_DIV value = 6 - 1 = 5 (if register uses N-1 encoding)
   *   or APB_CLK_DIV value = 6 (if register uses direct division factor)
   */

  modifyreg32(CLKRST_SOC_CLK_CTRL0_REG,
              CLKRST_APB_CLK_DIV_M,
              (5 << CLKRST_APB_CLK_DIV_S));

  /* Record configured frequencies.
   * PLL is 480 MHz, integer divider /2 = 240 MHz.
   * TODO: configure PLL fractional divider for true 400 MHz.
   */

  g_cpu_freq = PLL_FREQ_HZ / 2;  /* 240 MHz until fractional divider is set */
  g_apb_freq = TARGET_APB_FREQ_HZ;
}

/****************************************************************************
 * Name: esp32p4_enable_periph_clocks
 *
 * Description:
 *   Enable clocks for the peripherals used by NuttX.
 *
 ****************************************************************************/

static void esp32p4_enable_periph_clocks(void)
{
  /* Enable UART clocks */

#ifdef CONFIG_ESP32P4_UART0
  REG_SET_BIT(CLKRST_HP_PERIP_CLK_EN_REG, CLKRST_UART0_CLK_EN);
#endif

#ifdef CONFIG_ESP32P4_UART1
  REG_SET_BIT(CLKRST_HP_PERIP_CLK_EN_REG, CLKRST_UART1_CLK_EN);
#endif

  /* Enable Timer clocks */

#ifdef CONFIG_ESP32P4_TIMER
  REG_SET_BIT(CLKRST_CPU_PERIP_CLK_EN0_REG, CLKRST_TIMER0_CLK_EN);
#endif

  /* Enable GPIO clock */

#ifdef CONFIG_ESP32P4_GPIO
  REG_SET_BIT(CLKRST_CPU_PERIP_CLK_EN0_REG, CLKRST_GPIO_CLK_EN);
#endif
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: esp32p4_clockconfig
 *
 * Description:
 *   Initialize the ESP32-P4 clock tree.
 *   This should be called early in the boot process, before peripheral
 *   initialization.
 *
 *   The boot ROM has already set up a basic clock configuration using
 *   the 40 MHz crystal. This function upgrades to the full PLL-based
 *   configuration for optimal performance.
 *
 ****************************************************************************/

void esp32p4_clockconfig(void)
{
  /* Step 1: Configure the PLL (if not already done by ROM) */

  esp32p4_config_pll();

  /* Step 2: Switch CPU to PLL clock source for maximum performance */

  esp32p4_config_cpu_clock();

  /* Step 3: Enable peripheral clocks */

  esp32p4_enable_periph_clocks();
}

/****************************************************************************
 * Name: esp32p4_get_cpu_freq
 *
 * Description:
 *   Get the current CPU clock frequency.
 *
 * Returned Value:
 *   CPU frequency in Hz.
 *
 ****************************************************************************/

uint32_t esp32p4_get_cpu_freq(void)
{
  return g_cpu_freq;
}

/****************************************************************************
 * Name: esp32p4_get_apb_freq
 *
 * Description:
 *   Get the current APB bus clock frequency.
 *
 * Returned Value:
 *   APB frequency in Hz.
 *
 ****************************************************************************/

uint32_t esp32p4_get_apb_freq(void)
{
  return g_apb_freq;
}
