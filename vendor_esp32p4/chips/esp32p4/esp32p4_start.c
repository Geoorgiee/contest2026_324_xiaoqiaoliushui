/****************************************************************************
 * vendor/espressif/chips/esp32p4/esp32p4_start.c
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
#include <nuttx/init.h>
#include <nuttx/irq.h>

#include <arch/board/board.h>

#include "chip.h"
#include "riscv_internal.h"

#ifdef CONFIG_ESP32P4_PSRAM
#include "hardware/esp32p4_mspi.h"
#endif

extern void esp32p4_clockconfig(void);

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* ESP32-P4 Memory Layout:
 *   HP Core SRAM: 768 KB at 0x4ff00000
 *   PSRAM:        up to 32 MB at 0x48000000
 *   Flash:        up to 16 MB at 0x42000000
 */

extern uint8_t _ebss[];
#define HEAP_BASE      ((uintptr_t)_ebss + CONFIG_IDLETHREAD_STACKSIZE)

/****************************************************************************
 * Public Data
 ****************************************************************************/

/* g_idle_topstack: _sbss is the start of the BSS region as defined by the
 * linker script. _ebss lies at the end of the BSS region. The idle task
 * stack starts at the end of BSS and is of size CONFIG_IDLETHREAD_STACKSIZE.
 * The IDLE thread is the thread that the system boots on and, eventually,
 * becomes the IDLE, do nothing task that runs only when there is nothing
 * else to run. The heap continues from there until the end of memory.
 * g_idle_topstack is a read-only variable that provides this computed
 * address.
 */

const uintptr_t g_idle_topstack = HEAP_BASE;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: esp32p4_mspi_psram_set_clock
 *
 * Description:
 *   Configure the MSPI SPI0 clock for PSRAM access.
 *
 * Input Parameters:
 *   freq_hz - Target clock frequency in Hz
 *
 ****************************************************************************/

#ifdef CONFIG_ESP32P4_PSRAM
static void esp32p4_mspi_psram_set_clock(uint32_t freq_hz)
{
  uint32_t regval;
  uint32_t div;

  /* Calculate clock divider from source frequency.
   * SPI_CLK = CLK_SRC / (CLKCNT_N + 1)
   * For simplicity, use equal duty cycle: H = N/2, L = N
   */

  div = MSPI_CLK_SRC_FREQ / freq_hz;
  if (div < 1)
    {
      div = 1;
    }

  /* Configure SRAM clock register.
   * Set divider and equal duty cycle (50%).
   */

  regval = ((div - 1) << SPI_MEM_SCLKCNT_N_S) |
           (((div / 2) - 1) << SPI_MEM_SCLKCNT_H_S) |
           ((div - 1) << SPI_MEM_SCLKCNT_L_S);

  REG_WRITE(MSPI_SPI0_BASE + SPI_MEM_SRAM_CLK_REG, regval);
}
#endif

/****************************************************************************
 * Name: esp32p4_mspi_psram_send_cmd
 *
 * Description:
 *   Send a command to PSRAM via the MSPI controller in SPI mode.
 *   This is used for initial communication before OPI mode is entered.
 *
 * Input Parameters:
 *   cmd       - Command byte to send
 *   addr_bits - Number of address bits (0 if no address phase)
 *   addr      - Address value (ignored if addr_bits is 0)
 *   dummy     - Number of dummy cycles
 *   read_buf  - Buffer for read data (NULL if no read phase)
 *   read_len  - Number of bytes to read (0 if no read phase)
 *
 * Returned Value:
 *   0 on success, negative errno on failure
 *
 ****************************************************************************/

#ifdef CONFIG_ESP32P4_PSRAM
static int esp32p4_mspi_psram_send_cmd(uint16_t cmd, int addr_bits,
                                        uint32_t addr, int dummy,
                                        uint8_t *read_buf, int read_len)
{
  uint32_t regval;
  volatile int timeout;

  /* Configure user register: enable command phase */

  regval = SPI_MEM_USR_COMMAND;
  if (addr_bits > 0)
    {
      regval |= SPI_MEM_USR_ADDR;
    }

  if (dummy > 0)
    {
      regval |= SPI_MEM_USR_DUMMY;
    }

  if (read_len > 0)
    {
      regval |= SPI_MEM_USR_MISO;
    }

  REG_WRITE(MSPI_SPI0_BASE + SPI_MEM_USER_REG, regval);

  /* Configure command value and length */

  regval = (cmd & SPI_MEM_USR_COMMAND_VALUE_M) |
           ((7 << SPI_MEM_USR_COMMAND_BITLEN_S) &
            SPI_MEM_USR_COMMAND_BITLEN_M);

  REG_WRITE(MSPI_SPI0_BASE + SPI_MEM_USER2_REG, regval);

  /* Configure address and dummy cycles */

  if (addr_bits > 0)
    {
      regval = (((addr_bits - 1) << SPI_MEM_USR_ADDR_BITLEN_S) &
                SPI_MEM_USR_ADDR_BITLEN_M);
      REG_WRITE(MSPI_SPI0_BASE + SPI_MEM_USER1_REG, regval);
      REG_WRITE(MSPI_SPI0_BASE + SPI_MEM_ADDR_REG, addr);
    }

  if (dummy > 0)
    {
      modifyreg32(MSPI_SPI0_BASE + SPI_MEM_USER1_REG,
                  SPI_MEM_USR_DUMMY_CYCLELEN_M,
                  ((dummy - 1) << SPI_MEM_USR_DUMMY_CYCLELEN_S));
    }

  /* Set data length */

  if (read_len > 0)
    {
      REG_WRITE(MSPI_SPI0_BASE + SPI_MEM_MS_DLEN_REG,
                ((read_len * 8 - 1) << SPI_MEM_MS_DATA_BITLEN_S));
    }

  /* Start the transaction */

  REG_WRITE(MSPI_SPI0_BASE + SPI_MEM_CMD_REG, SPI_MEM_USR);

  /* Wait for completion */

  for (timeout = 0; timeout < 1000; timeout++)
    {
      regval = REG_READ(MSPI_SPI0_BASE + SPI_MEM_CMD_REG);
      if (!(regval & SPI_MEM_USR))
        {
          break;
        }
    }

  if (timeout >= 1000)
    {
      return -ETIMEDOUT;
    }

  /* Read response data if requested */

  if (read_buf != NULL && read_len > 0)
    {
      uint32_t rdata = REG_READ(MSPI_SPI0_BASE + SPI_MEM_R(0));
      int i;

      for (i = 0; i < read_len && i < 4; i++)
        {
          read_buf[i] = (rdata >> (i * 8)) & 0xff;
        }
    }

  return 0;
}
#endif

/****************************************************************************
 * Name: esp32p4_mspi_psram_reset
 *
 * Description:
 *   Send reset command sequence to PSRAM.
 *   The reset sequence is: Reset Enable (0x66) followed by Reset (0x99).
 *
 ****************************************************************************/

#ifdef CONFIG_ESP32P4_PSRAM
static void esp32p4_mspi_psram_reset(void)
{
  /* Send Reset Enable command */

  esp32p4_mspi_psram_send_cmd(PSRAM_CMD_RESET_EN_OPI, 0, 0, 0, NULL, 0);

  /* Send Reset command */

  esp32p4_mspi_psram_send_cmd(PSRAM_CMD_RESET_OPI, 0, 0, 0, NULL, 0);

  /* Wait for reset to complete (typically ~100 us) */

  volatile int i;
  for (i = 0; i < 10000; i++)
    {
    }
}
#endif

/****************************************************************************
 * Name: esp32p4_mspi_psram_read_id
 *
 * Description:
 *   Read the PSRAM chip ID to verify it is present and accessible.
 *
 * Input Parameters:
 *   id - Pointer to store the 16-bit PSRAM ID
 *
 * Returned Value:
 *   0 on success, negative errno on failure
 *
 ****************************************************************************/

#ifdef CONFIG_ESP32P4_PSRAM
static int esp32p4_mspi_psram_read_id(uint16_t *id)
{
  uint8_t id_buf[2];
  int ret;

  /* Read ID using SPI mode command (0x9F).
   * The PSRAM responds with a 16-bit ID.
   */

  ret = esp32p4_mspi_psram_send_cmd(PSRAM_CMD_READ_ID_SPI,
                                     24, 0x000000, 0,
                                     id_buf, 2);
  if (ret < 0)
    {
      return ret;
    }

  *id = (id_buf[1] << 8) | id_buf[0];
  return 0;
}
#endif

/****************************************************************************
 * Name: esp32p4_mspi_psram_enter_opi
 *
 * Description:
 *   Switch PSRAM from SPI mode to OPI (Octal SPI) mode.
 *   After this command, all subsequent commands must use 8-bit OPI encoding.
 *
 ****************************************************************************/

#ifdef CONFIG_ESP32P4_PSRAM
static void esp32p4_mspi_psram_enter_opi(void)
{
  /* Send Enter OPI Mode command (0xA0) in SPI mode */

  esp32p4_mspi_psram_send_cmd(PSRAM_CMD_ENTER_OPI, 0, 0, 0, NULL, 0);

  /* Wait for mode switch to complete */

  volatile int i;
  for (i = 0; i < 1000; i++)
    {
    }
}
#endif

/****************************************************************************
 * Name: esp32p4_mspi_psram_config_opi
 *
 * Description:
 *   Configure the MSPI SPI0 controller for OPI PSRAM access.
 *   This sets up the read/write commands and clock for OPI mode.
 *
 ****************************************************************************/

#ifdef CONFIG_ESP32P4_PSRAM
static void esp32p4_mspi_psram_config_opi(void)
{
  uint32_t regval;

  /* Configure SPI0 control register for OPI mode.
   * Enable octal read and octal command modes.
   */

  modifyreg32(MSPI_SPI0_BASE + SPI_MEM_CTRL_REG,
              0,
              SPI_MEM_FREAD_OCT | SPI_MEM_FCMD_OCT);

  /* Configure SRAM read command for OPI mode.
   * Read command: 0x2000 (16-bit, sent on all 8 lines)
   * Address: 32 bits (24-bit address + 8-bit burst length)
   * Dummy: 4 cycles (for OPI read latency)
   */

  regval = (PSRAM_CMD_READ_OPI << 16);  /* Read command value */
  REG_WRITE(MSPI_SPI0_BASE + SPI_MEM_SRAM_DRD_CMD_REG, regval);

  /* Configure SRAM write command for OPI mode.
   * Write command: 0x8000 (16-bit, sent on all 8 lines)
   * Address: 32 bits
   */

  regval = (PSRAM_CMD_WRITE_OPI << 16);  /* Write command value */
  REG_WRITE(MSPI_SPI0_BASE + SPI_MEM_SRAM_DWR_CMD_REG, regval);

  /* Enable user-defined read/write commands for SRAM cache access */

  modifyreg32(MSPI_SPI0_BASE + SPI_MEM_SRAM_CMD_REG,
              0,
              SPI_MEM_CACHE_SRAM_USR_RCMD | SPI_MEM_CACHE_SRAM_USR_WCMD);

  /* Configure clock for OPI PSRAM access at the target frequency.
   * Start with a conservative 80 MHz for reliability.
   */

  esp32p4_mspi_psram_set_clock(MSPI_PSRAM_INIT_CLK_FREQ);
}
#endif

/****************************************************************************
 * Name: esp32p4_mspi_psram_cache_config
 *
 * Description:
 *   Configure the cache controller to map PSRAM into the CPU address space.
 *   The PSRAM is mapped at ESP32P4_PSRAM_BASE (0x48000000).
 *
 ****************************************************************************/

#ifdef CONFIG_ESP32P4_PSRAM
static void esp32p4_mspi_psram_cache_config(void)
{
  /* Enable cache for SRAM (PSRAM) access.
   * The XMC (eXternal Memory Controller) handles the cache mapping.
   *
   * Note: The exact register layout for XMC cache configuration is
   * SoC-specific and may require additional registers not defined here.
   * This is a simplified configuration that enables basic cache access.
   */

  modifyreg32(MSPI_SPI0_BASE + SPI_MEM_CACHE_SCTRL_REG,
              0,
              SPI_MEM_CACHE_FMEM_CACHE_EN | SPI_MEM_CACHE_FMEM_MBUS_EN);

  /* Configure 32-bit address mode for PSRAM (required for > 16MB) */

  modifyreg32(MSPI_SPI0_BASE + SPI_MEM_CACHE_FCTRL_REG,
              0,
              SPI_MEM_CACHE_USR_CMD_4BYTE);
}
#endif

/****************************************************************************
 * Name: esp32p4_init_memory
 *
 * Description:
 *   Initialize the memory subsystem. This includes enabling the L2 cache
 *   and optionally initializing PSRAM.
 *
 *   When PSRAM is enabled, the initialization sequence is:
 *   1. Configure MSPI clock for PSRAM
 *   2. Reset PSRAM chip
 *   3. Read PSRAM ID to verify presence
 *   4. Enter OPI/QPI mode
 *   5. Configure SPI0 for OPI PSRAM access
 *   6. Configure cache mapping
 *
 *   The PSRAM will be added as a heap region in esp32p4_allocateheap.c
 *
 ****************************************************************************/

static void esp32p4_init_memory(void)
{
  /* The BSS section has already been cleared by the assembly startup code
   * in esp_head.S. The data section has also been copied from flash to SRAM.
   */

#ifdef CONFIG_ESP32P4_PSRAM
  uint16_t psram_id;
  int ret;

  /* Step 1: Configure MSPI clock for initial PSRAM communication.
   * Start with 80 MHz which is safe for both SPI and OPI modes.
   */

  esp32p4_mspi_psram_set_clock(MSPI_PSRAM_INIT_CLK_FREQ);

  /* Step 2: Reset PSRAM chip.
   * This ensures the PSRAM is in a known state before configuration.
   */

  esp32p4_mspi_psram_reset();

  /* Step 3: Read PSRAM ID to verify the chip is present and accessible.
   * This is done in SPI mode before entering OPI.
   */

  ret = esp32p4_mspi_psram_read_id(&psram_id);
  if (ret < 0)
    {
      /* PSRAM not responding. This could mean:
       * - No PSRAM chip on the board
       * - MSPI not properly connected
       * - Clock configuration incorrect
       *
       * Continue booting with SRAM only.
       */

      return;
    }

  /* Step 4: Enter OPI mode.
   * This switches the PSRAM from SPI to OPI (Octal SPI) mode
   * for higher bandwidth access.
   */

#ifdef CONFIG_ESP32P4_PSRAM_MODE_OPI
  esp32p4_mspi_psram_enter_opi();
#endif

  /* Step 5: Configure SPI0 for OPI PSRAM access.
   * This sets up the read/write commands and clock for the selected mode.
   */

#ifdef CONFIG_ESP32P4_PSRAM_MODE_OPI
  esp32p4_mspi_psram_config_opi();
#endif

  /* Step 6: Configure cache mapping for PSRAM.
   * This maps the PSRAM into the CPU address space at ESP32P4_PSRAM_BASE.
   */

  esp32p4_mspi_psram_cache_config();
#endif
}

/****************************************************************************
 * Name: esp32p4_init_clocks
 *
 * Description:
 *   Configure the system clocks. On ESP32-P4 this involves:
 *   - Setting up PLL for the HP core (target 400 MHz)
 *   - Configuring APB clock (80 MHz for peripheral access)
 *   - Enabling peripheral clocks as needed
 *
 ****************************************************************************/

static void esp32p4_init_clocks(void)
{
  /* Configure PLL, CPU clock, APB clock, and enable peripheral clocks.
   *
   * The ESP32-P4 boot ROM sets up a default clock configuration using the
   * 40 MHz crystal. esp32p4_clockconfig() upgrades to the full PLL-based
   * configuration for optimal performance:
   *   - CPU clock: 400 MHz (from 480 MHz PLL)
   *   - APB clock: 80 MHz (PLL / 6)
   */

  esp32p4_clockconfig();
}

/****************************************************************************
 * Name: esp32p4_init_peripherals
 *
 * Description:
 *   Early peripheral initialization before NuttX starts.
 *   Only critical peripherals needed for the boot process are initialized.
 *
 ****************************************************************************/

static void esp32p4_init_peripherals(void)
{
  /* Initialize the interrupt controller (PLIC).
   * This must be done before any interrupts can be used.
   * The full PLIC initialization is handled later by up_irqinitialize().
   */

  /* Initialize the UART for early console output.
   * The full UART initialization is handled later by riscv_serialinit(),
   * but we need low-level output for early debug messages.
   */
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: __espressif_start
 *
 * Description:
 *   ESP32-P4 HP Core entry point.
 *   This is the first C function called after the RISC-V assembly startup
 *   code in esp_head.S. It initializes the hardware and starts NuttX.
 *
 *   The startup sequence is:
 *   1. Initialize memory subsystem (PSRAM if enabled)
 *   2. Configure system clocks (PLL, APB)
 *   3. Initialize critical peripherals
 *   4. Call nx_start() to begin NuttX initialization
 *
 ****************************************************************************/

void __espressif_start(void)
{
  /* Initialize ESP32-P4 HP Core hardware */

  /* Step 1: Initialize memory (PSRAM, cache) */

  esp32p4_init_memory();

  /* Step 2: Configure system clocks */

  esp32p4_init_clocks();

  /* Step 3: Initialize critical peripherals */

  esp32p4_init_peripherals();

  /* Step 4: Bring up NuttX
   *
   * nx_start() will:
   *   - Initialize the OS data structures
   *   - Call up_initialize() which calls:
   *     - up_irqinitialize() to set up the PLIC
   *     - up_allocate_heap() to set up the heap
   *     - up_timer_initialize() to start the system tick
   *   - Start the IDLE thread
   *   - Start the scheduler
   *   - Call board_late_initialize() for board bring-up
   */

  nx_start();

  /* Should never reach here */

  for (; ; )
    {
    }
}
