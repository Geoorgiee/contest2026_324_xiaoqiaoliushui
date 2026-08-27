/****************************************************************************
 * vendor/espressif/chips/esp32p4/esp32p4_psram.c
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

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

#include "chip.h"
#include "esp32p4_psram.h"
#include "hardware/esp32p4_mspi.h"
#include "hardware/esp32p4_soc.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* PSRAM initialization timeout (iterations) */

#define PSRAM_TIMEOUT               10000
#define PSRAM_RESET_DELAY           10000
#define PSRAM_MODE_SWITCH_DELAY     1000

/* MMU configuration */

#define MMU_INVALID_ENTRY           0xFFFFFFFF
#define MMU_PSRAM_TARGET            0x02  /* PSRAM target ID */

/* Self-test patterns */

#define TEST_PATTERN_AA             0xAAAAAAAA
#define TEST_PATTERN_55             0x55555555
#define TEST_PATTERN_ADDR           0xDEADBEEF
#define TEST_PATTERN_INV            0x21524110

/* PSRAM density codes */

#define PSRAM_DENSITY_4MB           0x01
#define PSRAM_DENSITY_8MB           0x03
#define PSRAM_DENSITY_16MB          0x05
#define PSRAM_DENSITY_32MB          0x07
#define PSRAM_DENSITY_64MB          0x06

/* Register access macros */

#define PSRAM_READ_REG(offset)      \
  getreg32(MSPI_SPI0_BASE + (offset))
#define PSRAM_WRITE_REG(offset, val) \
  putreg32((val), MSPI_SPI0_BASE + (offset))
#define PSRAM_MODIFY_REG(offset, clear, set) \
  modifyreg32(MSPI_SPI0_BASE + (offset), (clear), (set))

/****************************************************************************
 * Private Types
 ****************************************************************************/

/* PSRAM mode register structure (AP-PSRAM hex mode) */

struct psram_mode_reg_s
{
  uint8_t mr0;  /* Drive strength, read latency, latency type */
  uint8_t mr1;  /* Vendor ID */
  uint8_t mr2;  /* Density, device ID, KGD */
  uint8_t mr3;  /* SRF, RBX enable */
  uint8_t mr4;  /* PASR, refresh, write latency */
  uint8_t mr8;  /* Burst length, burst type, X16 mode */
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

static bool s_psram_initialized = false;
static uint32_t s_psram_size = 0;
static uint32_t s_psram_clk_freq = 0;
static uint32_t s_error_count = 0;

/* Default configuration */

static const struct esp32p4_psram_config_s g_default_config =
{
  .size = ESP32P4_PSRAM_SIZE,
  .clk_freq = ESP32P4_PSRAM_CLK_200MHZ,
  .opi_mode = true,
  .ecc_enable = false,
  .self_test = true,
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static void psram_set_clock(uint32_t freq_hz);
static int psram_send_cmd(uint16_t cmd, int addr_bits, uint32_t addr,
                          int dummy, uint8_t *read_buf, int read_len);
static void psram_reset(void);
static int psram_read_id(uint16_t *id);
static void psram_enter_opi(void);
static void psram_config_opi(void);
static void psram_config_cache(void);
static int psram_detect_size(uint16_t id);
static int psram_config_mmu(void);
static void psram_enable_mpll(void);

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: psram_enable_mpll
 *
 * Description:
 *   Enable the MPLL (MSPI PLL) clock at 400 MHz for PSRAM access.
 *
 ****************************************************************************/

static void psram_enable_mpll(void)
{
  /* Enable MPLL clock source.
   * MPLL provides 400 MHz clock for MSPI controller.
   * The clock tree is: XTAL (40 MHz) -> MPLL -> 400 MHz
   *
   * Note: In a real implementation, this would configure the clock
   * tree registers. For now, we assume MPLL is already enabled by
   * the boot ROM or early startup code.
   */

  /* TODO: Configure HP_SYS_CLKRST registers for MPLL enable */

  s_psram_clk_freq = ESP32P4_PSRAM_CLK_MPLL;
}

/****************************************************************************
 * Name: psram_set_clock
 *
 * Description:
 *   Configure the MSPI SPI0 clock for PSRAM access.
 *
 * Input Parameters:
 *   freq_hz - Target clock frequency in Hz
 *
 ****************************************************************************/

static void psram_set_clock(uint32_t freq_hz)
{
  uint32_t regval;
  uint32_t div;

  /* Calculate clock divider from source frequency.
   * SPI_CLK = CLK_SRC / (CLKCNT_N + 1)
   * For simplicity, use equal duty cycle: H = N/2, L = N
   */

  div = ESP32P4_PSRAM_CLK_MPLL / freq_hz;
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

  PSRAM_WRITE_REG(SPI_MEM_SRAM_CLK_REG, regval);
}

/****************************************************************************
 * Name: psram_send_cmd
 *
 * Description:
 *   Send a command to PSRAM via the MSPI controller.
 *
 * Input Parameters:
 *   cmd       - Command value
 *   addr_bits - Number of address bits (0 if no address phase)
 *   addr      - Address value
 *   dummy     - Number of dummy cycles
 *   read_buf  - Buffer for read data (NULL if no read phase)
 *   read_len  - Number of bytes to read
 *
 * Returned Value:
 *   0 on success, negative errno on failure
 *
 ****************************************************************************/

static int psram_send_cmd(uint16_t cmd, int addr_bits, uint32_t addr,
                          int dummy, uint8_t *read_buf, int read_len)
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

  PSRAM_WRITE_REG(SPI_MEM_USER_REG, regval);

  /* Configure command value and length */

  regval = (cmd & SPI_MEM_USR_COMMAND_VALUE_M) |
           ((7 << SPI_MEM_USR_COMMAND_BITLEN_S) &
            SPI_MEM_USR_COMMAND_BITLEN_M);

  PSRAM_WRITE_REG(SPI_MEM_USER2_REG, regval);

  /* Configure address and dummy cycles */

  if (addr_bits > 0)
    {
      regval = (((addr_bits - 1) << SPI_MEM_USR_ADDR_BITLEN_S) &
                SPI_MEM_USR_ADDR_BITLEN_M);
      PSRAM_WRITE_REG(SPI_MEM_USER1_REG, regval);
      PSRAM_WRITE_REG(SPI_MEM_ADDR_REG, addr);
    }

  if (dummy > 0)
    {
      PSRAM_MODIFY_REG(SPI_MEM_USER1_REG,
                       SPI_MEM_USR_DUMMY_CYCLELEN_M,
                       ((dummy - 1) << SPI_MEM_USR_DUMMY_CYCLELEN_S));
    }

  /* Set data length */

  if (read_len > 0)
    {
      PSRAM_WRITE_REG(SPI_MEM_MS_DLEN_REG,
                      ((read_len * 8 - 1) << SPI_MEM_MS_DATA_BITLEN_S));
    }

  /* Start the transaction */

  PSRAM_WRITE_REG(SPI_MEM_CMD_REG, SPI_MEM_USR);

  /* Wait for completion */

  for (timeout = 0; timeout < PSRAM_TIMEOUT; timeout++)
    {
      regval = PSRAM_READ_REG(SPI_MEM_CMD_REG);
      if (!(regval & SPI_MEM_USR))
        {
          break;
        }
    }

  if (timeout >= PSRAM_TIMEOUT)
    {
      s_error_count++;
      return -ETIMEDOUT;
    }

  /* Read response data if requested */

  if (read_buf != NULL && read_len > 0)
    {
      uint32_t rdata = PSRAM_READ_REG(SPI_MEM_R(0));
      int i;

      for (i = 0; i < read_len && i < 4; i++)
        {
          read_buf[i] = (rdata >> (i * 8)) & 0xff;
        }
    }

  return ESP32P4_PSRAM_OK;
}

/****************************************************************************
 * Name: psram_reset
 *
 * Description:
 *   Send reset command sequence to PSRAM.
 *   The reset sequence is: Reset Enable (0x66) followed by Reset (0x99).
 *
 ****************************************************************************/

static void psram_reset(void)
{
  /* Send Reset Enable command */

  psram_send_cmd(PSRAM_CMD_RESET_EN_OPI, 0, 0, 0, NULL, 0);

  /* Send Reset command */

  psram_send_cmd(PSRAM_CMD_RESET_OPI, 0, 0, 0, NULL, 0);

  /* Wait for reset to complete */

  volatile int i;
  for (i = 0; i < PSRAM_RESET_DELAY; i++)
    {
    }
}

/****************************************************************************
 * Name: psram_read_id
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

static int psram_read_id(uint16_t *id)
{
  uint8_t id_buf[2];
  int ret;

  /* Read ID using SPI mode command (0x9F).
   * The PSRAM responds with a 16-bit ID.
   */

  ret = psram_send_cmd(PSRAM_CMD_READ_ID_SPI,
                       24, 0x000000, 0,
                       id_buf, 2);
  if (ret < 0)
    {
      return ret;
    }

  *id = (id_buf[1] << 8) | id_buf[0];
  return ESP32P4_PSRAM_OK;
}

/****************************************************************************
 * Name: psram_enter_opi
 *
 * Description:
 *   Switch PSRAM from SPI mode to OPI (Octal SPI) mode.
 *
 ****************************************************************************/

static void psram_enter_opi(void)
{
  /* Send Enter OPI Mode command (0xA0) in SPI mode */

  psram_send_cmd(PSRAM_CMD_ENTER_OPI, 0, 0, 0, NULL, 0);

  /* Wait for mode switch to complete */

  volatile int i;
  for (i = 0; i < PSRAM_MODE_SWITCH_DELAY; i++)
    {
    }
}

/****************************************************************************
 * Name: psram_config_opi
 *
 * Description:
 *   Configure the MSPI SPI0 controller for OPI PSRAM access.
 *
 ****************************************************************************/

static void psram_config_opi(void)
{
  uint32_t regval;

  /* Configure SPI0 control register for OPI mode.
   * Enable octal read and octal command modes.
   */

  PSRAM_MODIFY_REG(SPI_MEM_CTRL_REG, 0,
                   SPI_MEM_FREAD_OCT | SPI_MEM_FCMD_OCT);

  /* Configure SRAM read command for OPI mode.
   * Read command: 0x2000 (16-bit, sent on all 8 lines)
   * Address: 32 bits (24-bit address + 8-bit burst length)
   * Dummy: 4 cycles (for OPI read latency)
   */

  regval = (PSRAM_CMD_READ_OPI << 16);  /* Read command value */
  PSRAM_WRITE_REG(SPI_MEM_SRAM_DRD_CMD_REG, regval);

  /* Configure SRAM write command for OPI mode.
   * Write command: 0x8000 (16-bit, sent on all 8 lines)
   * Address: 32 bits
   */

  regval = (PSRAM_CMD_WRITE_OPI << 16);  /* Write command value */
  PSRAM_WRITE_REG(SPI_MEM_SRAM_DWR_CMD_REG, regval);

  /* Enable user-defined read/write commands for SRAM cache access */

  PSRAM_MODIFY_REG(SPI_MEM_SRAM_CMD_REG, 0,
                   SPI_MEM_CACHE_SRAM_USR_RCMD | SPI_MEM_CACHE_SRAM_USR_WCMD);

  /* Configure clock for OPI PSRAM access at the target frequency.
   * Start with a conservative 80 MHz for reliability.
   */

  psram_set_clock(ESP32P4_PSRAM_CLK_80MHZ);
}

/****************************************************************************
 * Name: psram_config_cache
 *
 * Description:
 *   Configure the cache controller to map PSRAM into the CPU address space.
 *
 ****************************************************************************/

static void psram_config_cache(void)
{
  /* Enable cache for SRAM (PSRAM) access.
   * The XMC (eXternal Memory Controller) handles the cache mapping.
   */

  PSRAM_MODIFY_REG(SPI_MEM_CACHE_SCTRL_REG, 0,
                   SPI_MEM_CACHE_FMEM_CACHE_EN | SPI_MEM_CACHE_FMEM_MBUS_EN);

  /* Configure 32-bit address mode for PSRAM (required for > 16MB) */

  PSRAM_MODIFY_REG(SPI_MEM_CACHE_FCTRL_REG, 0,
                   SPI_MEM_CACHE_USR_CMD_4BYTE);
}

/****************************************************************************
 * Name: psram_detect_size
 *
 * Description:
 *   Detect PSRAM size from density code in mode register.
 *
 * Input Parameters:
 *   id - PSRAM ID containing density information
 *
 * Returned Value:
 *   PSRAM size in bytes, or 0 if unknown
 *
 ****************************************************************************/

static int psram_detect_size(uint16_t id)
{
  /* Extract density from ID (bits 2:0 of high byte) */

  uint8_t density = (id >> 8) & 0x07;

  switch (density)
    {
      case PSRAM_DENSITY_4MB:
        return 4 * 1024 * 1024;

      case PSRAM_DENSITY_8MB:
        return 8 * 1024 * 1024;

      case PSRAM_DENSITY_16MB:
        return 16 * 1024 * 1024;

      case PSRAM_DENSITY_32MB:
        return 32 * 1024 * 1024;

      case PSRAM_DENSITY_64MB:
        return 64 * 1024 * 1024;

      default:
        return 0;
    }
}

/****************************************************************************
 * Name: psram_config_mmu
 *
 * Description:
 *   Configure MMU page table for PSRAM mapping.
 *   Maps PSRAM physical addresses to virtual addresses starting at
 *   ESP32P4_PSRAM_BASE (0x48000000).
 *
 * Returned Value:
 *   0 on success, negative errno on failure
 *
 ****************************************************************************/

static int psram_config_mmu(void)
{
  uint32_t num_pages;
  uint32_t i;
  uint32_t *mmu_table;

  /* Calculate number of pages needed */

  num_pages = (s_psram_size + ESP32P4_MMU_PAGE_SIZE - 1) /
              ESP32P4_MMU_PAGE_SIZE;

  if (num_pages > ESP32P4_MMU_ENTRY_COUNT)
    {
      num_pages = ESP32P4_MMU_ENTRY_COUNT;
    }

  /* Get MMU table base address.
   * The MMU table is typically located at a fixed address in SRAM.
   * For ESP32-P4, the PSRAM MMU table is separate from the flash MMU.
   */

  /* TODO: Get actual MMU table address from hardware registers */

  /* Configure MMU entries.
   * Each entry maps a 64KB page from physical to virtual address.
   *
   * Entry format (simplified):
   *   [31:20] - Reserved
   *   [19:16] - Target ID (0x02 for PSRAM)
   *   [15:0]  - Physical page number
   */

  for (i = 0; i < num_pages; i++)
    {
      /* Map virtual page i to physical page i in PSRAM */

      /* TODO: Write MMU entry to actual hardware register */

      /* For now, this is a placeholder that demonstrates the mapping logic */
    }

  /* Invalidate cache after MMU reconfiguration */

  /* TODO: Invalidate L1 cache */

  return ESP32P4_PSRAM_OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: esp32p4_psram_init
 *
 * Description:
 *   Initialize the PSRAM subsystem.
 *
 ****************************************************************************/

int esp32p4_psram_init(const struct esp32p4_psram_config_s *config)
{
  uint16_t psram_id;
  int ret;

  /* Use default configuration if none provided */

  if (config == NULL)
    {
      config = &g_default_config;
    }

  /* Check if already initialized */

  if (s_psram_initialized)
    {
      return ESP32P4_PSRAM_OK;
    }

  /* Step 1: Enable MPLL clock (400 MHz) */

  psram_enable_mpll();

  /* Step 2: Configure MSPI clock for initial PSRAM communication */

  psram_set_clock(ESP32P4_PSRAM_CLK_80MHZ);

  /* Step 3: Reset PSRAM chip */

  psram_reset();

  /* Step 4: Read PSRAM ID to verify the chip is present */

  ret = psram_read_id(&psram_id);
  if (ret < 0)
    {
      /* PSRAM not responding */

      return ESP32P4_PSRAM_ERR_NOT_FOUND;
    }

  /* Step 5: Detect PSRAM size */

  s_psram_size = psram_detect_size(psram_id);
  if (s_psram_size == 0)
    {
      /* Use configured size as fallback */

      s_psram_size = config->size;
    }

  /* Step 6: Enter OPI mode if configured */

  if (config->opi_mode)
    {
      psram_enter_opi();
      psram_config_opi();
    }

  /* Step 7: Configure cache mapping */

  psram_config_cache();

  /* Step 8: Configure MMU page table */

  ret = psram_config_mmu();
  if (ret < 0)
    {
      return ESP32P4_PSRAM_ERR_MMAP;
    }

  /* Step 9: Run self-test if configured */

  if (config->self_test)
    {
      ret = esp32p4_psram_self_test();
      if (ret < 0)
        {
          return ESP32P4_PSRAM_ERR_TEST;
        }
    }

  /* Mark as initialized */

  s_psram_initialized = true;

  return ESP32P4_PSRAM_OK;
}

/****************************************************************************
 * Name: esp32p4_psram_is_initialized
 *
 * Description:
 *   Check if PSRAM is initialized and available.
 *
 ****************************************************************************/

bool esp32p4_psram_is_initialized(void)
{
  return s_psram_initialized;
}

/****************************************************************************
 * Name: esp32p4_psram_get_size
 *
 * Description:
 *   Get the size of the initialized PSRAM.
 *
 ****************************************************************************/

uint32_t esp32p4_psram_get_size(void)
{
  if (!s_psram_initialized)
    {
      return 0;
    }

  return s_psram_size;
}

/****************************************************************************
 * Name: esp32p4_psram_get_stats
 *
 * Description:
 *   Get PSRAM usage statistics.
 *
 ****************************************************************************/

int esp32p4_psram_get_stats(struct esp32p4_psram_stats_s *stats)
{
  if (stats == NULL)
    {
      return -EINVAL;
    }

  if (!s_psram_initialized)
    {
      return -ENODEV;
    }

  stats->total_size = s_psram_size;
  stats->used_size = 0;  /* TODO: Calculate from heap */
  stats->free_size = s_psram_size;
  stats->dma_capable = s_psram_size;  /* PSRAM is DMA-capable */
  stats->error_count = s_error_count;

  return ESP32P4_PSRAM_OK;
}

/****************************************************************************
 * Name: esp32p4_psram_self_test
 *
 * Description:
 *   Run a self-test on the PSRAM.
 *
 ****************************************************************************/

int esp32p4_psram_self_test(void)
{
  volatile uint32_t *psram_ptr = (volatile uint32_t *)ESP32P4_PSRAM_BASE;
  uint32_t test_size = 4096;  /* Test first 4KB */
  uint32_t i;
  uint32_t errors = 0;

  if (!s_psram_initialized)
    {
      return -ENODEV;
    }

  /* Phase 1: Write test pattern (0xAAAAAAAA) */

  for (i = 0; i < test_size / sizeof(uint32_t); i++)
    {
      psram_ptr[i] = TEST_PATTERN_AA;
    }

  /* Verify test pattern */

  for (i = 0; i < test_size / sizeof(uint32_t); i++)
    {
      if (psram_ptr[i] != TEST_PATTERN_AA)
        {
          errors++;
        }
    }

  /* Phase 2: Write test pattern (0x55555555) */

  for (i = 0; i < test_size / sizeof(uint32_t); i++)
    {
      psram_ptr[i] = TEST_PATTERN_55;
    }

  /* Verify test pattern */

  for (i = 0; i < test_size / sizeof(uint32_t); i++)
    {
      if (psram_ptr[i] != TEST_PATTERN_55)
        {
          errors++;
        }
    }

  /* Phase 3: Address-based pattern test */

  for (i = 0; i < test_size / sizeof(uint32_t); i++)
    {
      psram_ptr[i] = (uint32_t)&psram_ptr[i] ^ TEST_PATTERN_ADDR;
    }

  /* Verify address-based pattern */

  for (i = 0; i < test_size / sizeof(uint32_t); i++)
    {
      uint32_t expected = (uint32_t)&psram_ptr[i] ^ TEST_PATTERN_ADDR;
      if (psram_ptr[i] != expected)
        {
          errors++;
        }
    }

  /* Phase 4: Walking ones test */

  for (i = 0; i < 32; i++)
    {
      psram_ptr[0] = (1 << i);
      if (psram_ptr[0] != (1 << i))
        {
          errors++;
        }
    }

  /* Clear test area */

  memset((void *)ESP32P4_PSRAM_BASE, 0, test_size);

  if (errors > 0)
    {
      s_error_count += errors;
      return ESP32P4_PSRAM_ERR_TEST;
    }

  return ESP32P4_PSRAM_OK;
}

/****************************************************************************
 * Name: esp32p4_psram_dma_test
 *
 * Description:
 *   Test DMA access to PSRAM.
 *
 ****************************************************************************/

int esp32p4_psram_dma_test(void)
{
  /* TODO: Implement DMA test using GDMA controller
   *
   * The test should:
   * 1. Allocate a DMA buffer in internal SRAM
   * 2. Write test pattern to PSRAM
   * 3. Use DMA to copy from PSRAM to internal SRAM
   * 4. Verify the copied data matches
   * 5. Use DMA to copy from internal SRAM to PSRAM
   * 6. Verify the PSRAM data matches
   */

  if (!s_psram_initialized)
    {
      return -ENODEV;
    }

  /* For now, return success as DMA is not yet implemented */

  return ESP32P4_PSRAM_OK;
}

/****************************************************************************
 * Name: esp32p4_psram_mmap
 *
 * Description:
 *   Map a region of PSRAM into the CPU address space.
 *
 ****************************************************************************/

int esp32p4_psram_mmap(uint32_t phys_addr, uint32_t size, void **vaddr)
{
  if (!s_psram_initialized)
    {
      return -ENODEV;
    }

  if (vaddr == NULL)
    {
      return -EINVAL;
    }

  /* Check if the requested region is within PSRAM bounds */

  if (phys_addr + size > s_psram_size)
    {
      return -EINVAL;
    }

  /* The PSRAM is already mapped at ESP32P4_PSRAM_BASE.
   * Calculate the virtual address from the physical offset.
   */

  *vaddr = (void *)(ESP32P4_PSRAM_BASE + phys_addr);

  return ESP32P4_PSRAM_OK;
}

/****************************************************************************
 * Name: esp32p4_psram_get_heap_base
 *
 * Description:
 *   Get the base address of the PSRAM heap region.
 *
 ****************************************************************************/

void *esp32p4_psram_get_heap_base(void)
{
  if (!s_psram_initialized)
    {
      return NULL;
    }

  return (void *)ESP32P4_PSRAM_BASE;
}

/****************************************************************************
 * Name: esp32p4_psram_get_heap_size
 *
 * Description:
 *   Get the size of the PSRAM heap region.
 *
 ****************************************************************************/

uint32_t esp32p4_psram_get_heap_size(void)
{
  if (!s_psram_initialized)
    {
      return 0;
    }

  return s_psram_size;
}
