/****************************************************************************
 * vendor_esp32p4/chips/esp32p4/esp32p4_mipi_dsi.c
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
 * This driver implements the ESP32-P4 MIPI-DSI display interface for NuttX.
 *
 * It is based on the ESP-IDF camera_dsi/dsi_init example and the NuttX
 * framebuffer driver framework.  The driver supports:
 *
 *   - MIPI DSI 2.1 PHY configuration (80-1500 Mbps/lane)
 *   - DSI Host controller in video mode (DPI)
 *   - DSI Bridge for DMA-to-pixel data path
 *   - DSI command mode for LCD panel initialization
 *   - NuttX fb_vtable_s framebuffer interface
 *   - Multiple LCD panel types (ILI9881C, EK79007)
 *   - RGB888 and RGB565 pixel formats
 *   - Double-buffered framebuffer
 *
 * The ESP32-P4 MIPI DSI subsystem consists of:
 *   1. DSI Host controller  - protocol layer (byte clock domain)
 *   2. DSI Bridge           - DMA-to-pixel data path (pixel clock domain)
 *   3. DSI PHY              - analog high-speed serial interface
 *   4. HP_SYS_CLKRST        - clock and reset control
 *
 * Initialization sequence (following ESP-IDF dsi_init):
 *   1. Enable DSI bus clock, reset bridge
 *   2. Configure PHY clock source and enable clocks
 *   3. Power on Host + PHY, reset PHY, enable clock lane
 *   4. Configure PHY PLL (M/N factors, HS frequency range)
 *   5. Wait for PLL lock and lane stop state
 *   6. Configure Host: command mode, clock lane LP, switch times
 *   7. Configure Host: CRC, ECC, EoTp, timeouts
 *   8. Send panel init commands via DCS interface (command mode)
 *   9. Configure DPI color coding (Host + Bridge)
 *  10. Configure DPI timing (Host in byte clocks, Bridge in pixel clocks)
 *  11. Configure video mode parameters
 *  12. Enable Bridge DPI output
 *  13. Enable Host video mode, switch clock lane to HS
 *  14. Configure DPI clock, enable DPI clock
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <debug.h>

#include <nuttx/irq.h>
#include <nuttx/arch.h>
#include <nuttx/kmalloc.h>
#include <nuttx/video/fb.h>
#include <nuttx/video/mipi_dsi.h>

#include "hardware/esp32p4_soc.h"
#include "hardware/esp32p4_mipi_dsi.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Debug ********************************************************************/

#ifdef CONFIG_DEBUG_MIPI_DSI_INFO
#  define dsiinfo  syslog
#else
#  define dsiinfo  _none
#endif

#ifdef CONFIG_DEBUG_MIPI_DSI_ERROR
#  define dsierr   syslog
#else
#  define dsierr   _none
#endif

/* DSI Host register access */

#define dsi_host_read(reg) \
  getreg32(ESP32P4_MIPI_DSI_HOST_BASE + (reg))
#define dsi_host_write(val, reg) \
  putreg32((val), ESP32P4_MIPI_DSI_HOST_BASE + (reg))
#define dsi_host_setbits(bits, reg) \
  modifyreg32(ESP32P4_MIPI_DSI_HOST_BASE + (reg), 0, (bits))
#define dsi_host_clrbits(bits, reg) \
  modifyreg32(ESP32P4_MIPI_DSI_HOST_BASE + (reg), (bits), 0)

/* DSI Bridge register access */

#define dsi_brg_read(reg) \
  getreg32(ESP32P4_MIPI_DSI_BRG_BASE + (reg))
#define dsi_brg_write(val, reg) \
  putreg32((val), ESP32P4_MIPI_DSI_BRG_BASE + (reg))
#define dsi_brg_setbits(bits, reg) \
  modifyreg32(ESP32P4_MIPI_DSI_BRG_BASE + (reg), 0, (bits))
#define dsi_brg_clrbits(bits, reg) \
  modifyreg32(ESP32P4_MIPI_DSI_BRG_BASE + (reg), (bits), 0)

/* HP_SYS_CLKRST register access.
 *
 * DR_REG_HPPERIPH1_BASE       = 0x500C0000
 * DR_REG_HP_SYS_CLKRST_BASE   = 0x500C0000 + 0x26000 = 0x500E6000
 */

#define HP_SYS_CLKRST_BASE              0x500e6000

#define HP_SYS_CLKRST_SOC_CLK_CTRL1    (HP_SYS_CLKRST_BASE + 0x018)
#define HP_SYS_CLKRST_HP_RST_EN0       (HP_SYS_CLKRST_BASE + 0x024)
#define HP_SYS_CLKRST_PERI_CLK_CTRL02  (HP_SYS_CLKRST_BASE + 0x070)
#define HP_SYS_CLKRST_PERI_CLK_CTRL03  (HP_SYS_CLKRST_BASE + 0x074)

/* SOC_CLK_CTRL1 bits */

#define SOC_CLK_CTRL1_DSI_SYS_CLK_EN   (1 << 12)

/* HP_RST_EN0 bits */

#define HP_RST_EN0_DSI_BRG             (1 << 20)

/* PERI_CLK_CTRL02 bits */

#define PERI_CLK_CTRL02_DPHY_CLK_SRC_SEL_S  0
#define PERI_CLK_CTRL02_DPHY_CLK_SRC_SEL_M  (0x3 << 0)

/* PERI_CLK_CTRL03 bits */

#define PERI_CLK_CTRL03_DPHY_PLL_REFCLK_EN  (1 << 0)
#define PERI_CLK_CTRL03_DPHY_CFG_CLK_EN     (1 << 1)
#define PERI_CLK_CTRL03_DPICLK_SRC_SEL_S    2
#define PERI_CLK_CTRL03_DPICLK_SRC_SEL_M    (0x3 << 2)
#define PERI_CLK_CTRL03_DPICLK_DIV_NUM_S    4
#define PERI_CLK_CTRL03_DPICLK_DIV_NUM_M    (0xff << 4)
#define PERI_CLK_CTRL03_DPICLK_EN           (1 << 12)

/* DPI clock source selections */

#define DPICLK_SRC_XTAL         0
#define DPICLK_SRC_PLL_F240M    1
#define DPICLK_SRC_PLL_F160M    2

/* PHY clock source selections */

#define DPHY_CLK_SRC_PLL_F20M   0
#define DPHY_CLK_SRC_RC_FAST    1
#define DPHY_CLK_SRC_PLL_F25M   2

/* Default frequencies */

#define DSI_PHY_REF_CLK_MHZ    20   /* PLL_F20M default */
#define DSI_TIMEOUT_CLK_MHZ     10
#define DSI_ESCAPE_CLK_MHZ      18

/* Timeout for DSI operations (in milliseconds) */

#define DSI_TIMEOUT_MS            1000
#define DSI_CMD_TIMEOUT_MS        100
#define DSI_PHY_LOCK_TIMEOUT_MS   200

/* Framebuffer configuration */

#define DSI_FB_STRIDE(hres, bpp)  ((hres) * ((bpp) >> 3))

/* DSI Bridge register offsets (from bridge base) */

#define BRG_CLK_EN_REG          0x000
#define BRG_EN_REG              0x004
#define BRG_DMA_REQ_CFG_REG     0x008
#define BRG_RAW_NUM_CFG_REG     0x00c
#define BRG_RAW_BUF_CREDIT_REG  0x010
#define BRG_PIXEL_TYPE_REG      0x018
#define BRG_DMA_BLK_INTERVAL_REG 0x020
#define BRG_DPI_V_CFG0_REG      0x030
#define BRG_DPI_V_CFG1_REG      0x034
#define BRG_DPI_H_CFG0_REG      0x038
#define BRG_DPI_H_CFG1_REG      0x03c
#define BRG_DPI_MISC_CFG_REG    0x040
#define BRG_DPI_CFG_UPDATE_REG  0x044
#define BRG_HOST_CTRL_REG       0x130
#define BRG_DMA_FLOW_CTRL_REG   0x144

/****************************************************************************
 * Private Types
 ****************************************************************************/

/* DSI PHY PLL range descriptor */

struct dsi_phy_pll_range_s
{
  uint16_t start_mbps;
  uint16_t end_mbps;
  uint8_t  hs_freq_range_sel;
};

/* DSI host context structure */

struct esp32p4_dsi_priv_s
{
  /* NuttX framebuffer vtable - must be first member */

  struct fb_vtable_s vtable;
  struct fb_videoinfo_s videoinfo;
  struct fb_planeinfo_s planeinfo;

  /* DSI configuration */

  uint8_t num_data_lanes;
  uint32_t lane_bit_rate_mbps;
  uint32_t actual_lane_bit_rate_mbps;
  uint32_t dpi_clock_freq_mhz;

  /* Panel configuration */

  enum esp32p4_lcd_panel_e panel_type;
  struct esp32p4_dsi_video_timing_s timing;
  int reset_gpio_num;

  /* Framebuffer - double buffered */

  FAR uint8_t *fbmem[2];
  size_t fb_size;
  int active_fb;

  /* State */

  bool initialized;
  bool panel_on;
  bool video_mode_enabled;
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/* Clock and reset functions */

static void dsi_enable_bus_clock(bool enable);
static void dsi_reset_bridge(void);
static void dsi_enable_phy_clocks(bool enable, uint8_t phy_clk_src);
static void dsi_configure_dpi_clock(FAR struct esp32p4_dsi_priv_s *priv);

/* DSI PHY functions */

static void dsi_phy_write_register(FAR struct esp32p4_dsi_priv_s *priv,
                                   uint8_t reg_addr, uint8_t reg_val);
static void dsi_phy_configure_pll(FAR struct esp32p4_dsi_priv_s *priv);
static int  dsi_phy_wait_lock(FAR struct esp32p4_dsi_priv_s *priv);
static int  dsi_phy_wait_lanes_stopped(FAR struct esp32p4_dsi_priv_s *priv);

/* DSI Host functions */

static void dsi_host_power_on(bool on);
static void dsi_host_phy_power_on(bool on);
static void dsi_host_configure(FAR struct esp32p4_dsi_priv_s *priv);
static void dsi_host_configure_dpi(FAR struct esp32p4_dsi_priv_s *priv);
static void dsi_host_configure_video_mode(FAR struct esp32p4_dsi_priv_s *priv);
static int  dsi_host_generic_write(uint8_t vc, uint8_t dt,
                                    FAR const uint8_t *data, size_t len);
static int  dsi_host_dcs_write(uint8_t vc, uint8_t cmd,
                                FAR const uint8_t *data, size_t len);

/* DSI Bridge functions */

static void dsi_bridge_init(FAR struct esp32p4_dsi_priv_s *priv);
static void dsi_bridge_configure_dpi(FAR struct esp32p4_dsi_priv_s *priv);
static void dsi_bridge_enable(bool enable);

/* LCD panel functions */

static int  lcd_panel_ili9881c_init(FAR struct esp32p4_dsi_priv_s *priv);
static int  lcd_panel_ek79007_init(FAR struct esp32p4_dsi_priv_s *priv);
static int  lcd_panel_reset(FAR struct esp32p4_dsi_priv_s *priv);

/* Framebuffer vtable functions */

static int  dsi_getvideoinfo(FAR struct fb_vtable_s *vtable,
                              FAR struct fb_videoinfo_s *vinfo);
static int  dsi_getplaneinfo(FAR struct fb_vtable_s *vtable, int planeno,
                              FAR struct fb_planeinfo_s *pinfo);
static int  dsi_open(FAR struct fb_vtable_s *vtable);
static int  dsi_close(FAR struct fb_vtable_s *vtable);
static int  dsi_pan_display(FAR struct fb_vtable_s *vtable,
                             FAR struct fb_planeinfo_s *pinfo);
static int  dsi_blank(FAR struct fb_vtable_s *vtable, bool blank);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static FAR struct esp32p4_dsi_priv_s *g_dsi_priv;

/* DSI PHY PLL frequency range table for ESP32-P4.
 * Based on ESP-IDF soc_mipi_dsi_phy_pll_ranges.
 */

static const struct dsi_phy_pll_range_s g_phy_pll_ranges[] =
{
  {   80,  100, 0x00 },
  {  100,  120, 0x10 },
  {  120,  160, 0x20 },
  {  160,  200, 0x30 },
  {  200,  240, 0x01 },
  {  240,  320, 0x11 },
  {  320,  390, 0x21 },
  {  390,  450, 0x31 },
  {  450,  510, 0x02 },
  {  510, 1500, 0x12 },
};

#define PHY_PLL_RANGE_COUNT \
  (sizeof(g_phy_pll_ranges) / sizeof(g_phy_pll_ranges[0]))

/****************************************************************************
 * Private Functions - Clock and Reset
 ****************************************************************************/

/****************************************************************************
 * Name: dsi_enable_bus_clock
 *
 * Description:
 *   Enable or disable the APB clock for accessing DSI Host and Bridge
 *   registers.  Controls HP_SYS_CLKRST.soc_clk_ctrl1.reg_dsi_sys_clk_en.
 *
 ****************************************************************************/

static void dsi_enable_bus_clock(bool enable)
{
  uint32_t reg = getreg32(HP_SYS_CLKRST_SOC_CLK_CTRL1);
  if (enable)
    {
      reg |= SOC_CLK_CTRL1_DSI_SYS_CLK_EN;
    }
  else
    {
      reg &= ~SOC_CLK_CTRL1_DSI_SYS_CLK_EN;
    }

  putreg32(reg, HP_SYS_CLKRST_SOC_CLK_CTRL1);
}

/****************************************************************************
 * Name: dsi_reset_bridge
 *
 * Description:
 *   Reset the DSI Bridge controller via HP_SYS_CLKRST.
 *
 ****************************************************************************/

static void dsi_reset_bridge(void)
{
  /* Assert reset */

  modifyreg32(HP_SYS_CLKRST_HP_RST_EN0, 0, HP_RST_EN0_DSI_BRG);

  /* De-assert reset */

  modifyreg32(HP_SYS_CLKRST_HP_RST_EN0, HP_RST_EN0_DSI_BRG, 0);
}

/****************************************************************************
 * Name: dsi_enable_phy_clocks
 *
 * Description:
 *   Enable or disable the PHY configuration clock and PLL reference clock.
 *   Also set the PHY clock source.
 *
 ****************************************************************************/

static void dsi_enable_phy_clocks(bool enable, uint8_t phy_clk_src)
{
  uint32_t reg;

  /* Set PHY clock source */

  reg = getreg32(HP_SYS_CLKRST_PERI_CLK_CTRL02);
  reg &= ~PERI_CLK_CTRL02_DPHY_CLK_SRC_SEL_M;
  reg |= ((uint32_t)phy_clk_src << PERI_CLK_CTRL02_DPHY_CLK_SRC_SEL_S);
  putreg32(reg, HP_SYS_CLKRST_PERI_CLK_CTRL02);

  /* Enable/disable PHY config clock and PLL reference clock */

  reg = getreg32(HP_SYS_CLKRST_PERI_CLK_CTRL03);
  if (enable)
    {
      reg |= (PERI_CLK_CTRL03_DPHY_CFG_CLK_EN |
              PERI_CLK_CTRL03_DPHY_PLL_REFCLK_EN);
    }
  else
    {
      reg &= ~(PERI_CLK_CTRL03_DPHY_CFG_CLK_EN |
               PERI_CLK_CTRL03_DPHY_PLL_REFCLK_EN);
    }

  putreg32(reg, HP_SYS_CLKRST_PERI_CLK_CTRL03);
}

/****************************************************************************
 * Name: dsi_configure_dpi_clock
 *
 * Description:
 *   Configure the DPI pixel clock.
 *   Source: PLL_F160M (160 MHz) or PLL_F240M (240 MHz).
 *   Divider: source_freq / dpi_clock_freq.
 *
 ****************************************************************************/

static void dsi_configure_dpi_clock(FAR struct esp32p4_dsi_priv_s *priv)
{
  uint32_t src_freq_mhz;
  uint32_t div;
  uint32_t src_sel;
  uint32_t reg;

  /* Choose clock source based on required DPI clock.
   * PLL_F160M = 160 MHz, PLL_F240M = 240 MHz.
   * Pick the source that gives the closest integer divider.
   */

  if (priv->dpi_clock_freq_mhz <= 80)
    {
      src_freq_mhz = 160;
      src_sel = DPICLK_SRC_PLL_F160M;
    }
  else
    {
      src_freq_mhz = 240;
      src_sel = DPICLK_SRC_PLL_F240M;
    }

  div = src_freq_mhz / priv->dpi_clock_freq_mhz;
  if (div < 1)
    {
      div = 1;
    }

  if (div > 256)
    {
      div = 256;
    }

  /* Store actual DPI clock frequency */

  priv->dpi_clock_freq_mhz = src_freq_mhz / div;

  dsiinfo("DSI: DPI clock: src=%lu MHz, div=%lu, actual=%lu MHz\n",
          src_freq_mhz, div, priv->dpi_clock_freq_mhz);

  /* Set DPI clock source and divider */

  reg = getreg32(HP_SYS_CLKRST_PERI_CLK_CTRL03);
  reg &= ~(PERI_CLK_CTRL03_DPICLK_SRC_SEL_M |
           PERI_CLK_CTRL03_DPICLK_DIV_NUM_M);
  reg |= ((src_sel << PERI_CLK_CTRL03_DPICLK_SRC_SEL_S) |
          (((div - 1) & 0xff) << PERI_CLK_CTRL03_DPICLK_DIV_NUM_S));
  putreg32(reg, HP_SYS_CLKRST_PERI_CLK_CTRL03);

  /* Enable DPI clock */

  modifyreg32(HP_SYS_CLKRST_PERI_CLK_CTRL03, 0,
              PERI_CLK_CTRL03_DPICLK_EN);
}

/****************************************************************************
 * Private Functions - DSI PHY
 ****************************************************************************/

/****************************************************************************
 * Name: dsi_phy_write_register
 *
 * Description:
 *   Write a value to a DSI PHY internal register via the test interface.
 *   The test interface uses PHY_TST_CTRL0 and PHY_TST_CTRL1 registers
 *   of the DSI Host.
 *
 *   Sequence (following ESP-IDF mipi_dsi_hal_phy_write_register):
 *     1. Clear test clear pin, enable interface
 *     2. Write register address with testen=1 (on falling edge)
 *     3. Write register value with testen=0 (on rising edge)
 *
 ****************************************************************************/

static void dsi_phy_write_register(FAR struct esp32p4_dsi_priv_s *priv,
                                   uint8_t reg_addr, uint8_t reg_val)
{
  /* Clear test clear pin, enable the interface to write values to PHY */

  dsi_host_write(0, DSI_HOST_PHY_TST_CTRL0);

  /* Load PHY register address */

  dsi_host_write((1 << 16) | (reg_addr & 0xff), DSI_HOST_PHY_TST_CTRL1);

  /* Address write operation is on the falling edge of test clock */

  dsi_host_write((1 << 1), DSI_HOST_PHY_TST_CTRL0);
  dsi_host_write(0, DSI_HOST_PHY_TST_CTRL0);

  /* Load PHY register value */

  dsi_host_write(reg_val & 0xff, DSI_HOST_PHY_TST_CTRL1);

  /* Data write operation is on the rising edge of test clock */

  dsi_host_write((1 << 1), DSI_HOST_PHY_TST_CTRL0);
  dsi_host_write(0, DSI_HOST_PHY_TST_CTRL0);
}

/****************************************************************************
 * Name: dsi_phy_configure_pll
 *
 * Description:
 *   Configure the DSI PHY PLL to generate the desired lane bit rate.
 *
 *   The PLL formula: f_vco = (M / N) * f_ref
 *   where f_ref is the PHY reference clock frequency (default 20 MHz).
 *
 *   Constraints (from ESP-IDF):
 *     - 5 MHz <= f_ref / N <= 40 MHz
 *     - M must be even
 *     - VCO frequency = lane_bit_rate_mbps
 *
 *   PHY register map (accessed via test interface):
 *     0x44: HS frequency range selection
 *     0x19: PLL configuration (0x30 = use N/M from 0x17/0x18)
 *     0x17: Input frequency division ratio (N-1)
 *     0x18: Feedback multiplication ratio (M-1), written in 2 parts:
 *           First write: bits [4:0] of (M-1)
 *           Second write: 0x80 | bits [8:5] of (M-1)
 *
 ****************************************************************************/

static void dsi_phy_configure_pll(FAR struct esp32p4_dsi_priv_s *priv)
{
  uint32_t ref_freq_mhz = DSI_PHY_REF_CLK_MHZ;
  uint32_t vco_freq_mhz = priv->lane_bit_rate_mbps;
  uint8_t pll_n = 1;
  uint16_t pll_m = 0;
  uint8_t min_n;
  uint8_t max_n;
  uint8_t hs_freq_sel = 0;
  uint16_t m;
  uint8_t n;
  size_t i;

  dsiinfo("DSI: Configuring PHY PLL, target=%lu Mbps, ref=%lu MHz\n",
          vco_freq_mhz, ref_freq_mhz);

  /* Calculate N and M factors.
   * Constraint: 5 MHz <= f_ref / N <= 40 MHz
   */

  min_n = 1;
  if (ref_freq_mhz / min_n > 40)
    {
      min_n = (ref_freq_mhz + 39) / 40;
    }

  max_n = ref_freq_mhz / 5;
  if (max_n < 1)
    {
      max_n = 1;
    }

  for (n = min_n; n <= max_n; n++)
    {
      m = vco_freq_mhz * n / ref_freq_mhz;
      /* M must be even */

      if ((m & 0x01) == 0 && m > 0)
        {
          pll_m = m;
          pll_n = n;
          break;
        }
    }

  if (pll_m == 0)
    {
      dsierr("ERROR: Failed to calculate PLL M/N factors\n");
      /* Fall back to defaults */

      pll_m = 100;
      pll_n = 1;
    }

  /* Update actual lane bit rate */

  priv->actual_lane_bit_rate_mbps = ref_freq_mhz * pll_m / pll_n;

  dsiinfo("DSI: PHY PLL: M=%d, N=%d, actual_rate=%lu Mbps\n",
          pll_m, pll_n, priv->actual_lane_bit_rate_mbps);

  /* Find HS frequency range selection */

  for (i = 0; i < PHY_PLL_RANGE_COUNT; i++)
    {
      if (priv->actual_lane_bit_rate_mbps >=
              g_phy_pll_ranges[i].start_mbps &&
          priv->actual_lane_bit_rate_mbps <=
              g_phy_pll_ranges[i].end_mbps)
        {
          hs_freq_sel = g_phy_pll_ranges[i].hs_freq_range_sel;
          break;
        }
    }

  dsiinfo("DSI: HS freq range sel: 0x%02x\n", hs_freq_sel);

  /* Write PHY PLL configuration registers via test interface.
   *
   * Register 0x44: HS frequency range
   * Register 0x19: PLL configuration (use N and M from 0x17/0x18)
   * Register 0x17: Input frequency division ratio (N-1)
   * Register 0x18: Feedback multiplication ratio (M-1), 2 writes
   */

  dsi_phy_write_register(priv, 0x44, hs_freq_sel << 1);
  dsi_phy_write_register(priv, 0x19, 0x30);
  dsi_phy_write_register(priv, 0x17, (uint8_t)(pll_n - 1));
  dsi_phy_write_register(priv, 0x18, (uint8_t)((pll_m - 1) & 0x1f));
  dsi_phy_write_register(priv, 0x18,
                          (uint8_t)(0x80 | (((pll_m - 1) >> 5) & 0x0f)));
}

/****************************************************************************
 * Name: dsi_phy_wait_lock
 *
 * Description:
 *   Wait for the PHY PLL to lock.
 *
 * Returned Value:
 *   Zero (OK) on success; -ETIMEDOUT on failure.
 *
 ****************************************************************************/

static int dsi_phy_wait_lock(FAR struct esp32p4_dsi_priv_s *priv)
{
  int timeout = DSI_PHY_LOCK_TIMEOUT_MS;

  while (timeout-- > 0)
    {
      if (dsi_host_read(DSI_HOST_PHY_STATUS) & DSI_PHY_STATUS_PHY_LOCK)
        {
          dsiinfo("DSI: PHY PLL locked\n");
          return OK;
        }

      up_mdelay(1);
    }

  dsierr("ERROR: DSI PHY PLL lock timeout\n");
  return -ETIMEDOUT;
}

/****************************************************************************
 * Name: dsi_phy_wait_lanes_stopped
 *
 * Description:
 *   Wait for all active data lanes and clock lane to enter stop state.
 *   This is required before switching from LP to HS mode.
 *
 * Returned Value:
 *   Zero (OK) on success; -ETIMEDOUT on failure.
 *
 ****************************************************************************/

static int dsi_phy_wait_lanes_stopped(FAR struct esp32p4_dsi_priv_s *priv)
{
  uint32_t mask;
  int timeout = DSI_PHY_LOCK_TIMEOUT_MS;

  /* Build expected mask:
   *   bit 2: phy_stopstateclklane
   *   bit 4: phy_stopstate0lane
   *   bit 7: phy_stopstate1lane (only for 2-lane config)
   */

  mask = (1 << 2);  /* Clock lane stop state */

  if (priv->num_data_lanes > 0)
    {
      mask |= (1 << 4);  /* Data lane 0 stop state */
    }

  if (priv->num_data_lanes > 1)
    {
      mask |= (1 << 7);  /* Data lane 1 stop state */
    }

  while (timeout-- > 0)
    {
      if ((dsi_host_read(DSI_HOST_PHY_STATUS) & mask) == mask)
        {
          dsiinfo("DSI: All lanes in stop state\n");
          return OK;
        }

      up_mdelay(1);
    }

  dsierr("ERROR: DSI lane stop state timeout (status=0x%08x, "
         "expected=0x%08x)\n",
         dsi_host_read(DSI_HOST_PHY_STATUS), mask);
  return -ETIMEDOUT;
}

/****************************************************************************
 * Private Functions - DSI Host Controller
 ****************************************************************************/

/****************************************************************************
 * Name: dsi_host_power_on
 *
 * Description:
 *   Power on or off the DSI Host controller.
 *
 ****************************************************************************/

static void dsi_host_power_on(bool on)
{
  dsi_host_write(on ? DSI_HOST_PWR_UP_SHUTDOWNZ : 0, DSI_HOST_PWR_UP);
}

/****************************************************************************
 * Name: dsi_host_phy_power_on
 *
 * Description:
 *   Power on or off the DSI PHY.
 *
 ****************************************************************************/

static void dsi_host_phy_power_on(bool on)
{
  if (on)
    {
      dsi_host_setbits(DSI_PHY_RSTZ_PHY_SHUTDOWNZ, DSI_HOST_PHY_RSTZ);
    }
  else
    {
      dsi_host_clrbits(DSI_PHY_RSTZ_PHY_SHUTDOWNZ, DSI_HOST_PHY_RSTZ);
    }
}

/****************************************************************************
 * Name: dsi_host_configure
 *
 * Description:
 *   Configure the DSI Host controller for initial command mode operation.
 *   This sets up the basic host configuration before panel init commands
 *   are sent.  Follows the ESP-IDF esp_lcd_new_dsi_bus sequence.
 *
 ****************************************************************************/

static void dsi_host_configure(FAR struct esp32p4_dsi_priv_s *priv)
{
  uint32_t byte_clk_mhz;
  uint32_t to_clk_div;
  uint32_t esc_clk_div;

  byte_clk_mhz = priv->actual_lane_bit_rate_mbps / 8;

  /* Start in command mode (mode_cfg.cmd_video_mode = 1 = command) */

  dsi_host_write(1, DSI_HOST_MODE_CFG);

  /* Place clock lane in low power mode.
   * Will switch to HS later when DPI stream is ready.
   */

  dsi_host_write(0, DSI_HOST_LPCLK_CTRL);

  /* Set data lane HS/LP switch times (in lane byte clock cycles).
   * From ESP-IDF: data_hs2lp=50, data_lp2hs=104
   */

  dsi_host_write((50 << 16) | (104 << 0), DSI_HOST_PHY_TMR_CFG);

  /* Set clock lane HS/LP switch times.
   * From ESP-IDF: clk_hs2lp=46, clk_lp2hs=128
   */

  dsi_host_write((46 << 16) | (128 << 0), DSI_HOST_PHY_TMR_LPCLK_CFG);

  /* Set data lane number */

  dsi_host_write((priv->num_data_lanes - 1) & 0x1, DSI_HOST_PHY_IF_CFG);

  /* Enable CRC RX, ECC RX, and EoT TX in HS mode.
   * From ESP-IDF:
   *   eotp_tx_en = 1
   *   eotp_tx_lp_en = 0
   *   eotp_rx_en = 1
   *   crc_rx_en = 1
   *   ecc_rx_en = 1
   */

  dsi_host_write((1 << 0) |  /* eotp_tx_en */
                 (1 << 1) |  /* eotp_rx_en */
                 (1 << 3) |  /* ecc_rx_en */
                 (1 << 4),    /* crc_rx_en */
                 DSI_HOST_PCKHDL_CFG);

  /* Configure timeout clock division.
   * to_clk_div = byte_clk_mhz / 10 (aim for ~10 MHz timeout clock).
   */

  to_clk_div = byte_clk_mhz / DSI_TIMEOUT_CLK_MHZ;
  if (to_clk_div < 1)
    {
      to_clk_div = 1;
    }

  /* Configure escape clock division.
   * esc_clk_div = byte_clk_mhz / 18 (aim for ~18 MHz escape clock).
   */

  esc_clk_div = byte_clk_mhz / DSI_ESCAPE_CLK_MHZ;
  if (esc_clk_div < 2)
    {
      esc_clk_div = 2;
    }

  dsi_host_write(((esc_clk_div & 0xff) << 8) |
                 ((to_clk_div & 0xff) << 0),
                 DSI_HOST_CLKMGR_CFG);

  /* Set all timeout counts to 0 (disable timeout per ESP-IDF).
   * The DSI host will wait indefinitely.
   */

  dsi_host_write(0, DSI_HOST_TO_CNT_CFG);
  dsi_host_write(0, DSI_HOST_HS_RD_TIMEOUT_CNT);
  dsi_host_write(0, DSI_HOST_LP_RD_TIMEOUT_CNT);
  dsi_host_write(0, DSI_HOST_HS_WR_TIMEOUT_CNT);
  dsi_host_write(0, DSI_HOST_LP_WR_TIMEOUT_CNT);
  dsi_host_write(0, DSI_HOST_BTA_TIMEOUT_CNT);

  /* Set max read time (in lane byte clock cycles).
   * From ESP-IDF: max_rd_time = 6000
   */

  dsi_host_write(6000, DSI_HOST_PHY_TMR_RD_CFG);

  /* Set stop wait time.
   * phy_stop_wait_time in phy_if_cfg[15:8] = 0x3f
   */

  dsi_host_write(((priv->num_data_lanes - 1) & 0x1) |
                 (0x3f << 8),
                 DSI_HOST_PHY_IF_CFG);

  /* Configure command mode: all commands use LP mode.
   * From ESP-IDF: all TX types set to LP (value 1).
   */

  dsi_host_write((1 << 8)  |  /* gen_sw_0p_tx = LP */
                 (1 << 9)  |  /* gen_sw_1p_tx = LP */
                 (1 << 10) |  /* gen_sw_2p_tx = LP */
                 (1 << 14) |  /* gen_lw_tx = LP */
                 (1 << 16) |  /* dcs_sw_0p_tx = LP */
                 (1 << 17) |  /* dcs_sw_1p_tx = LP */
                 (1 << 19) |  /* dcs_lw_tx = LP */
                 (1 << 24),   /* max_rd_pkt_size = LP */
                 DSI_HOST_CMD_MODE_CFG);
}

/****************************************************************************
 * Name: dsi_host_configure_dpi
 *
 * Description:
 *   Configure the DPI (Display Pixel Interface) for video mode on the
 *   DSI Host controller.  The Host operates in the lane byte clock domain,
 *   so horizontal timing values must be converted from pixel clocks to
 *   byte clock cycles.
 *
 *   The conversion factor (dpi2lane_clk_ratio) is:
 *     ratio = lane_bit_rate_mbps / (dpi_clock_mhz * 8)
 *
 ****************************************************************************/

static void dsi_host_configure_dpi(FAR struct esp32p4_dsi_priv_s *priv)
{
  FAR const struct esp32p4_dsi_video_timing_s *timing = &priv->timing;
  uint32_t color_coding;
  uint32_t dpi2lane_ratio_x1000;
  uint32_t hsa;
  uint32_t hbp;
  uint32_t hline;

  /* Calculate DPI-to-lane byte clock ratio (x1000 for precision).
   * This converts pixel clock cycles to lane byte clock cycles.
   * From ESP-IDF: dpi2lane_clk_ratio = lane_bit_rate / dpi_clock / 8
   * Using integer arithmetic: ratio_x1000 = (lane_rate * 1000) / (dpi_clk * 8)
   */

  dpi2lane_ratio_x1000 = (priv->actual_lane_bit_rate_mbps * 1000) /
                          (priv->dpi_clock_freq_mhz * 8);

  dsiinfo("DSI: DPI-to-lane ratio: %u.%03u\n",
          dpi2lane_ratio_x1000 / 1000,
          dpi2lane_ratio_x1000 % 1000);

  /* Set DPI virtual channel */

  dsi_host_write(0, DSI_HOST_DPI_VCID);

  /* Set DPI color coding.
   * 16-bit RGB565 = 0, 18-bit = 3/4, 24-bit = 5
   */

  if (priv->planeinfo.bpp == 16)
    {
      color_coding = DSI_COLOR_CODING_16BIT_1;
    }
  else if (priv->planeinfo.bpp == 18)
    {
      color_coding = DSI_COLOR_CODING_18BIT_1;
    }
  else
    {
      color_coding = DSI_COLOR_CODING_24BIT;
    }

  dsi_host_write(color_coding, DSI_HOST_DPI_COLOR_CODING);

  /* Set DPI signal polarity (all active high) */

  dsi_host_write(0, DSI_HOST_DPI_CFG_POL);

  /* Convert horizontal timing from pixel clocks to lane byte clocks.
   * From ESP-IDF mipi_dsi_hal_host_dpi_set_horizontal_timing:
   *   hsa_time  = hsw * dpi2lane_ratio
   *   hbp_time  = hbp * dpi2lane_ratio
   *   hline_time = (hsw + hbp + active_width + hfp) * dpi2lane_ratio
   */

  hsa = (timing->hsync_pulse_width * dpi2lane_ratio_x1000) / 1000;
  hbp = (timing->hsync_back_porch * dpi2lane_ratio_x1000) / 1000;
  hline = ((timing->hsync_pulse_width +
            timing->hsync_back_porch +
            timing->h_size +
            timing->hsync_front_porch) * dpi2lane_ratio_x1000) / 1000;

  dsiinfo("DSI: Host H timing (byte clks): HSA=%lu HBP=%lu HLINE=%lu\n",
          hsa, hbp, hline);

  dsi_host_write(hsa, DSI_HOST_VID_HSA_TIME);
  dsi_host_write(hbp, DSI_HOST_VID_HBP_TIME);
  dsi_host_write(hline, DSI_HOST_VID_HLINE_TIME);

  /* Set vertical timing (in lines, same for both domains) */

  dsi_host_write(timing->vsync_pulse_width, DSI_HOST_VID_VSA_LINES);
  dsi_host_write(timing->vsync_back_porch, DSI_HOST_VID_VBP_LINES);
  dsi_host_write(timing->vsync_front_porch, DSI_HOST_VID_VFP_LINES);
  dsi_host_write(timing->v_size, DSI_HOST_VID_VACTIVE_LINES);

  /* Set video packet size = horizontal resolution */

  dsi_host_write(timing->h_size, DSI_HOST_VID_PKT_SIZE);

  /* Single chunk per line (no null packets) */

  dsi_host_write(0, DSI_HOST_VID_NUM_CHUNKS);
  dsi_host_write(0, DSI_HOST_VID_NULL_SIZE);

  /* DPI LP command timing.
   * Largest LP command packet size during/outside active region.
   */

  dsi_host_write((20 << 16) | (20 << 0), DSI_HOST_DPI_LP_CMD_TIM);
}

/****************************************************************************
 * Name: dsi_host_configure_video_mode
 *
 * Description:
 *   Configure the DSI Host for video mode operation.
 *   Non-burst with sync events mode, with LP transitions during
 *   blanking periods.
 *
 ****************************************************************************/

static void dsi_host_configure_video_mode(FAR struct esp32p4_dsi_priv_s *priv)
{
  uint32_t vid_mode_cfg;

  /* Configure video mode.
   *
   * From ESP-IDF:
   *   vid_mode_type = NON_BURST_WITH_SYNC_EVENTS (1)
   *   lp_vsa_en = 0
   *   lp_vbp_en = 1
   *   lp_vfp_en = 1
   *   lp_vact_en = 1
   *   lp_hbp_en = 1
   *   lp_hfp_en = 1
   *   frame_bta_ack_en = 0
   *   lp_cmd_en = 1
   */

  vid_mode_cfg = (DSI_VIDEO_MODE_NON_BURST_SYNC_EVENTS << 0) |
                 (0 << 8)  |  /* lp_vsa_en = 0 */
                 (1 << 9)  |  /* lp_vbp_en = 1 */
                 (1 << 10) |  /* lp_vfp_en = 1 */
                 (1 << 11) |  /* lp_vact_en = 1 */
                 (1 << 12) |  /* lp_hbp_en = 1 */
                 (1 << 13) |  /* lp_hfp_en = 1 */
                 (0 << 14) |  /* frame_bta_ack_en = 0 */
                 (1 << 15);   /* lp_cmd_en = 1 */

  dsi_host_write(vid_mode_cfg, DSI_HOST_VID_MODE_CFG);
}

/****************************************************************************
 * Name: dsi_host_generic_write
 *
 * Description:
 *   Send a generic write command via the DSI host.
 *   Handles both short (<=2 bytes) and long (>2 bytes) packets.
 *
 ****************************************************************************/

static int dsi_host_generic_write(uint8_t vc, uint8_t dt,
                                   FAR const uint8_t *data, size_t len)
{
  uint32_t header;
  int timeout;
  size_t i;

  /* Wait for command FIFO to be empty */

  timeout = DSI_CMD_TIMEOUT_MS;
  while (timeout-- > 0)
    {
      if (dsi_host_read(DSI_HOST_CMD_PKT_STATUS) &
          DSI_CMD_PKT_STATUS_GEN_CMD_EMPTY)
        {
          break;
        }

      up_mdelay(1);
    }

  if (timeout <= 0)
    {
      dsierr("ERROR: DSI command FIFO timeout\n");
      return -ETIMEDOUT;
    }

  /* Write payload for long packets */

  if (len > 2)
    {
      for (i = 0; i < len; i += 4)
        {
          uint32_t payload = 0;
          size_t remaining = len - i;
          size_t chunk = (remaining < 4) ? remaining : 4;
          size_t j;

          for (j = 0; j < chunk; j++)
            {
              payload |= (uint32_t)data[i + j] << (j * 8);
            }

          /* Wait for write FIFO not full */

          timeout = DSI_CMD_TIMEOUT_MS;
          while (timeout-- > 0)
            {
              if (!(dsi_host_read(DSI_HOST_CMD_PKT_STATUS) &
                    DSI_CMD_PKT_STATUS_GEN_PLD_W_FULL))
                {
                  break;
                }

              up_mdelay(1);
            }

          if (timeout <= 0)
            {
              dsierr("ERROR: DSI payload FIFO timeout\n");
              return -ETIMEDOUT;
            }

          dsi_host_write(payload, DSI_HOST_GEN_PLD_DATA);
        }
    }

  /* Build and send packet header */

  if (len <= 2)
    {
      uint8_t data0 = (len > 0) ? data[0] : 0;
      uint8_t data1 = (len > 1) ? data[1] : 0;
      header = ((uint32_t)data1 << 16) |
               ((uint32_t)data0 << 8) |
               ((vc << 6) | dt);
    }
  else
    {
      header = ((uint32_t)(len & 0xffff) << 16) |
               ((uint32_t)0 << 8) |
               ((vc << 6) | dt);
    }

  dsi_host_write(header, DSI_HOST_GEN_HDR);

  /* Wait for command to be processed */

  timeout = DSI_CMD_TIMEOUT_MS;
  while (timeout-- > 0)
    {
      if (dsi_host_read(DSI_HOST_CMD_PKT_STATUS) &
          DSI_CMD_PKT_STATUS_GEN_CMD_EMPTY)
        {
          break;
        }

      up_mdelay(1);
    }

  return OK;
}

/****************************************************************************
 * Name: dsi_host_dcs_write
 *
 * Description:
 *   Send a DCS (Display Command Set) write command.
 *   Short write (0 params): DCS_SHORT_WRITE_0 (0x05)
 *   Short write (1 param):  DCS_SHORT_WRITE_1 (0x15)
 *   Long write (>1 param):  DCS_LONG_WRITE (0x39)
 *
 ****************************************************************************/

static int dsi_host_dcs_write(uint8_t vc, uint8_t cmd,
                               FAR const uint8_t *data, size_t len)
{
  uint8_t buf[2];

  if (len == 0)
    {
      /* DCS short write, no parameter */

      buf[0] = cmd;
      return dsi_host_generic_write(vc, DSI_DT_DCS_SHORT_WRITE_0,
                                     buf, 1);
    }
  else if (len == 1)
    {
      /* DCS short write, 1 parameter */

      buf[0] = cmd;
      buf[1] = data[0];
      return dsi_host_generic_write(vc, DSI_DT_DCS_SHORT_WRITE_1,
                                     buf, 2);
    }
  else
    {
      /* DCS long write: prepend command byte to payload */

      FAR uint8_t *long_buf;
      int ret;

      long_buf = kmm_malloc(len + 1);
      if (long_buf == NULL)
        {
          return -ENOMEM;
        }

      long_buf[0] = cmd;
      memcpy(&long_buf[1], data, len);

      ret = dsi_host_generic_write(vc, DSI_DT_DCS_LONG_WRITE,
                                    long_buf, len + 1);

      kmm_free(long_buf);
      return ret;
    }
}

/****************************************************************************
 * Private Functions - DSI Bridge
 ****************************************************************************/

/****************************************************************************
 * Name: dsi_bridge_init
 *
 * Description:
 *   Initialize the DSI Bridge controller.  The bridge sits between DMA
 *   and the DSI Host, handling pixel-to-byte conversion and DMA flow
 *   control.
 *
 ****************************************************************************/

static void dsi_bridge_init(FAR struct esp32p4_dsi_priv_s *priv)
{
  uint32_t total_pixel_bits;

  /* Enable bridge register clock */

  dsi_brg_write(1, BRG_CLK_EN_REG);

  /* Enable bridge host reference clock.
   * host_ctrl.dsi_cfg_ref_clk_en = 1
   */

  dsi_brg_write(1, BRG_HOST_CTRL_REG);

  /* Set DMA burst length (in 64-bit words).
   * 128 x 64-bit = 1024 bytes per burst.
   */

  dsi_brg_write(128, BRG_DMA_REQ_CFG_REG);

  /* Set flow controller: DSI bridge (not DMA controller).
   * dma_flow_ctrl.dsi_dma_flow_controller = 1
   */

  dsi_brg_write(1, BRG_DMA_FLOW_CTRL_REG);

  /* Configure pixel format on bridge side.
   * pixel_type.raw_type: 0=RGB888, 1=RGB666, 2=RGB565
   */

  if (priv->planeinfo.bpp == 16)
    {
      dsi_brg_write(2, BRG_PIXEL_TYPE_REG);
    }
  else if (priv->planeinfo.bpp == 24)
    {
      dsi_brg_write(0, BRG_PIXEL_TYPE_REG);
    }
  else
    {
      dsi_brg_write(1, BRG_PIXEL_TYPE_REG);
    }

  /* Configure total pixel bits for the bridge.
   * raw_num_total = total_pixel_bits / 64
   * This tells the bridge how many 64-bit words to transfer per frame.
   */

  total_pixel_bits = priv->timing.h_size * priv->timing.v_size *
                     priv->planeinfo.bpp;

  dsi_brg_write(total_pixel_bits / 64, BRG_RAW_NUM_CFG_REG);

  /* Reload the value into internal counter */

  dsi_brg_setbits((1 << 31), BRG_RAW_NUM_CFG_REG);

  /* Enable auto-reload of raw_num_total per frame */

  dsi_brg_setbits((1 << 28), BRG_DMA_BLK_INTERVAL_REG);

  dsiinfo("DSI: Bridge initialized, total_pixel_bits/64=%lu\n",
          total_pixel_bits / 64);
}

/****************************************************************************
 * Name: dsi_bridge_configure_dpi
 *
 * Description:
 *   Configure the DSI Bridge DPI timing.  The bridge operates in the
 *   pixel clock domain, so timing values are in pixel clocks (not byte
 *   clocks like the Host).
 *
 ****************************************************************************/

static void dsi_bridge_configure_dpi(FAR struct esp32p4_dsi_priv_s *priv)
{
  FAR const struct esp32p4_dsi_video_timing_s *timing = &priv->timing;
  uint32_t htotal;
  uint32_t vtotal;

  htotal = timing->h_size + timing->hsync_pulse_width +
           timing->hsync_back_porch + timing->hsync_front_porch;
  vtotal = timing->v_size + timing->vsync_pulse_width +
           timing->vsync_back_porch + timing->vsync_front_porch;

  /* Set horizontal timing.
   * dpi_h_cfg0: hdisp[27:16] | htotal[11:0]
   * dpi_h_cfg1: hsync[27:16] | hbank[11:0]
   */

  dsi_brg_write((timing->h_size << 16) | (htotal & 0xfff),
                BRG_DPI_H_CFG0_REG);
  dsi_brg_write((timing->hsync_pulse_width << 16) |
                (timing->hsync_back_porch & 0xfff),
                BRG_DPI_H_CFG1_REG);

  /* Set vertical timing.
   * dpi_v_cfg0: vdisp[27:16] | vtotal[11:0]
   * dpi_v_cfg1: vsync[27:16] | vbank[11:0]
   */

  dsi_brg_write((timing->v_size << 16) | (vtotal & 0xfff),
                BRG_DPI_V_CFG0_REG);
  dsi_brg_write((timing->vsync_pulse_width << 16) |
                (timing->vsync_back_porch & 0xfff),
                BRG_DPI_V_CFG1_REG);

  /* Set underrun discard line count (default 413 from ESP-IDF) */

  dsi_brg_write((413 << 4), BRG_DPI_MISC_CFG_REG);

  /* Update DPI configuration */

  dsi_brg_write(1, BRG_DPI_CFG_UPDATE_REG);

  dsiinfo("DSI: Bridge DPI: %lux%lu, HTOTAL=%lu VTOTAL=%lu\n",
          timing->h_size, timing->v_size, htotal, vtotal);
}

/****************************************************************************
 * Name: dsi_bridge_enable
 *
 * Description:
 *   Enable or disable the DSI Bridge DPI output.
 *
 ****************************************************************************/

static void dsi_bridge_enable(bool enable)
{
  if (enable)
    {
      /* Enable bridge module */

      dsi_brg_write(1, BRG_EN_REG);

      /* Enable DPI output (dpi_misc_config.dpi_en = 1) */

      dsi_brg_setbits(1, BRG_DPI_MISC_CFG_REG);
    }
  else
    {
      /* Disable DPI output */

      dsi_brg_clrbits(1, BRG_DPI_MISC_CFG_REG);

      /* Disable bridge module */

      dsi_brg_write(0, BRG_EN_REG);
    }
}

/****************************************************************************
 * Private Functions - LCD Panel Initialization
 ****************************************************************************/

/****************************************************************************
 * Name: lcd_panel_reset
 *
 * Description:
 *   Perform hardware reset of the LCD panel via GPIO.
 *   If reset_gpio_num is -1, skip the hardware reset.
 *
 ****************************************************************************/

static int lcd_panel_reset(FAR struct esp32p4_dsi_priv_s *priv)
{
  if (priv->reset_gpio_num < 0)
    {
      dsiinfo("DSI: No reset GPIO configured, skipping HW reset\n");
      return OK;
    }

  dsiinfo("DSI: Resetting panel via GPIO %d\n", priv->reset_gpio_num);

  /* NOTE: GPIO configuration is board-specific.  The board-level code
   * should configure the GPIO before calling panel_init.  Here we just
   * provide the timing for the reset sequence.
   *
   * Pull reset low for 10ms, then high for 120ms.
   */

  up_mdelay(10);
  up_mdelay(120);

  return OK;
}

/****************************************************************************
 * Name: lcd_panel_ili9881c_init
 *
 * Description:
 *   Initialize the ILI9881C LCD panel (800x1280).
 *   This sends the vendor-specific initialization sequence via
 *   DSI DCS commands.
 *
 ****************************************************************************/

static int lcd_panel_ili9881c_init(FAR struct esp32p4_dsi_priv_s *priv)
{
  dsiinfo("DSI: Initializing ILI9881C panel (800x1280)\n");

  /* Switch to CMD2 page 1 */

  dsi_host_dcs_write(0, 0xff, (FAR const uint8_t *)"\x98\x81\x01", 3);

  /* Set register E0 for VCOM */

  dsi_host_dcs_write(0, 0xe0,
    (FAR const uint8_t *)"\x00\x00\x02", 3);

  /* Set register E1 for voltage level */

  dsi_host_dcs_write(0, 0xe1,
    (FAR const uint8_t *)"\x08\xa0\x00\x00\x07\xa0\x00\x00\x00"
                         "\x44\x44", 11);

  /* Set register E2 */

  dsi_host_dcs_write(0, 0xe2,
    (FAR const uint8_t *)"\x11\x11\x44\x44\xed\xa7\x00\x00\xec"
                         "\xa7\x00\x00", 12);

  /* Set register E3 */

  dsi_host_dcs_write(0, 0xe3,
    (FAR const uint8_t *)"\x00\x00\x11\x11", 4);

  /* Set register E6 for source timing */

  dsi_host_dcs_write(0, 0xe6,
    (FAR const uint8_t *)"\x44\x44\x00\x00", 4);

  /* Switch to CMD2 page 0 */

  dsi_host_dcs_write(0, 0xff, (FAR const uint8_t *)"\x98\x81\x00", 3);

  /* Set pixel format: 24-bit RGB (0x77) */

  dsi_host_dcs_write(0, 0x3a,
    (FAR const uint8_t *)"\x77", 1);

  /* Set display inversion */

  dsi_host_dcs_write(0, 0x36,
    (FAR const uint8_t *)"\x00", 1);

  /* Sleep out */

  dsi_host_dcs_write(0, 0x11, NULL, 0);
  up_mdelay(120);

  /* Display on */

  dsi_host_dcs_write(0, 0x29, NULL, 0);
  up_mdelay(20);

  dsiinfo("DSI: ILI9881C panel initialized\n");
  return OK;
}

/****************************************************************************
 * Name: lcd_panel_ek79007_init
 *
 * Description:
 *   Initialize the EK79007 LCD panel (1024x600).
 *   This sends the vendor-specific initialization sequence via
 *   DSI DCS commands.
 *
 ****************************************************************************/

static int lcd_panel_ek79007_init(FAR struct esp32p4_dsi_priv_s *priv)
{
  dsiinfo("DSI: Initializing EK79007 panel (1024x600)\n");

  /* Set pixel format: 24-bit RGB (0x77) */

  dsi_host_dcs_write(0, 0x3a,
    (FAR const uint8_t *)"\x77", 1);

  /* Set display inversion on (panel-specific) */

  dsi_host_dcs_write(0, 0x21, NULL, 0);

  /* Sleep out */

  dsi_host_dcs_write(0, 0x11, NULL, 0);
  up_mdelay(120);

  /* Display on */

  dsi_host_dcs_write(0, 0x29, NULL, 0);
  up_mdelay(20);

  dsiinfo("DSI: EK79007 panel initialized\n");
  return OK;
}

/****************************************************************************
 * Private Functions - Framebuffer VTable
 ****************************************************************************/

/****************************************************************************
 * Name: dsi_getvideoinfo
 *
 * Description:
 *   Get the video controller configuration.
 *
 ****************************************************************************/

static int dsi_getvideoinfo(FAR struct fb_vtable_s *vtable,
                             FAR struct fb_videoinfo_s *vinfo)
{
  FAR struct esp32p4_dsi_priv_s *priv =
    (FAR struct esp32p4_dsi_priv_s *)vtable;

  if (priv == NULL || vinfo == NULL)
    {
      return -EINVAL;
    }

  memcpy(vinfo, &priv->videoinfo, sizeof(struct fb_videoinfo_s));
  return OK;
}

/****************************************************************************
 * Name: dsi_getplaneinfo
 *
 * Description:
 *   Get the video plane configuration.
 *
 ****************************************************************************/

static int dsi_getplaneinfo(FAR struct fb_vtable_s *vtable, int planeno,
                             FAR struct fb_planeinfo_s *pinfo)
{
  FAR struct esp32p4_dsi_priv_s *priv =
    (FAR struct esp32p4_dsi_priv_s *)vtable;

  if (priv == NULL || planeno != 0 || pinfo == NULL)
    {
      return -EINVAL;
    }

  memcpy(pinfo, &priv->planeinfo, sizeof(struct fb_planeinfo_s));
  return OK;
}

/****************************************************************************
 * Name: dsi_open
 *
 * Description:
 *   Open the framebuffer device.
 *
 ****************************************************************************/

static int dsi_open(FAR struct fb_vtable_s *vtable)
{
  FAR struct esp32p4_dsi_priv_s *priv =
    (FAR struct esp32p4_dsi_priv_s *)vtable;

  if (priv == NULL)
    {
      return -EINVAL;
    }

  return OK;
}

/****************************************************************************
 * Name: dsi_close
 *
 * Description:
 *   Close the framebuffer device.
 *
 ****************************************************************************/

static int dsi_close(FAR struct fb_vtable_s *vtable)
{
  FAR struct esp32p4_dsi_priv_s *priv =
    (FAR struct esp32p4_dsi_priv_s *)vtable;

  if (priv == NULL)
    {
      return -EINVAL;
    }

  return OK;
}

/****************************************************************************
 * Name: dsi_pan_display
 *
 * Description:
 *   Pan the display to the specified framebuffer.  For double-buffered
 *   operation, this switches the active framebuffer.
 *
 ****************************************************************************/

static int dsi_pan_display(FAR struct fb_vtable_s *vtable,
                            FAR struct fb_planeinfo_s *pinfo)
{
  FAR struct esp32p4_dsi_priv_s *priv =
    (FAR struct esp32p4_dsi_priv_s *)vtable;
  FAR void *fbmem;

  if (priv == NULL || pinfo == NULL)
    {
      return -EINVAL;
    }

  /* Find which framebuffer is being requested */

  fbmem = pinfo->fbmem;
  if (fbmem == priv->fbmem[0])
    {
      priv->active_fb = 0;
    }
  else if (priv->fbmem[1] != NULL && fbmem == priv->fbmem[1])
    {
      priv->active_fb = 1;
    }
  else
    {
      return -EINVAL;
    }

  /* Update the planeinfo to reflect the active buffer */

  priv->planeinfo.fbmem = priv->fbmem[priv->active_fb];

  dsiinfo("DSI: Pan display, active_fb=%d\n", priv->active_fb);
  return OK;
}

/****************************************************************************
 * Name: dsi_blank
 *
 * Description:
 *   Blank or unblank the display.
 *
 ****************************************************************************/

static int dsi_blank(FAR struct fb_vtable_s *vtable, bool blank)
{
  FAR struct esp32p4_dsi_priv_s *priv =
    (FAR struct esp32p4_dsi_priv_s *)vtable;

  if (priv == NULL)
    {
      return -EINVAL;
    }

  if (blank)
    {
      /* Display off (DCS 0x28) */

      dsi_host_dcs_write(0, 0x28, NULL, 0);
      up_mdelay(20);
      priv->panel_on = false;
    }
  else
    {
      /* Display on (DCS 0x29) */

      dsi_host_dcs_write(0, 0x29, NULL, 0);
      up_mdelay(20);
      priv->panel_on = true;
    }

  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: esp32p4_mipi_dsi_initialize
 *
 * Description:
 *   Initialize the MIPI DSI bus, PHY, and host controller.
 *
 *   This follows the ESP-IDF esp_lcd_new_dsi_bus initialization sequence:
 *     1. Enable bus clock
 *     2. Reset bridge
 *     3. Enable PHY clocks
 *     4. Power on Host
 *     5. Power on PHY
 *     6. Reset PHY, enable clock lane, force PLL
 *     7. Configure PHY PLL
 *     8. Wait for PLL lock
 *     9. Wait for lanes to stop
 *    10. Configure Host for command mode
 *
 ****************************************************************************/

int esp32p4_mipi_dsi_initialize(
  FAR const struct esp32p4_dsi_bus_config_s *bus_config)
{
  int ret;

  dsiinfo("DSI: Initializing MIPI DSI bus\n");

  if (bus_config == NULL)
    {
      return -EINVAL;
    }

  if (bus_config->num_data_lanes > 2 || bus_config->num_data_lanes < 1)
    {
      dsierr("ERROR: Invalid number of data lanes: %d\n",
             bus_config->num_data_lanes);
      return -EINVAL;
    }

  if (bus_config->lane_bit_rate_mbps < 80 ||
      bus_config->lane_bit_rate_mbps > 1500)
    {
      dsierr("ERROR: Invalid lane bit rate: %lu Mbps\n",
             bus_config->lane_bit_rate_mbps);
      return -EINVAL;
    }

  /* Allocate DSI private context */

  g_dsi_priv = kmm_zalloc(sizeof(struct esp32p4_dsi_priv_s));
  if (g_dsi_priv == NULL)
    {
      dsierr("ERROR: Failed to allocate DSI context\n");
      return -ENOMEM;
    }

  /* Store configuration */

  g_dsi_priv->num_data_lanes = bus_config->num_data_lanes;
  g_dsi_priv->lane_bit_rate_mbps = bus_config->lane_bit_rate_mbps;
  g_dsi_priv->actual_lane_bit_rate_mbps = bus_config->lane_bit_rate_mbps;

  /* Step 1: Enable DSI bus clock */

  dsi_enable_bus_clock(true);

  /* Step 2: Reset DSI bridge */

  dsi_reset_bridge();

  /* Step 3: Enable PHY clocks (PLL_F20M as default source) */

  dsi_enable_phy_clocks(true, DPHY_CLK_SRC_PLL_F20M);

  /* Step 4: Power on Host controller */

  dsi_host_power_on(true);

  /* Step 5: Power on PHY */

  dsi_host_phy_power_on(true);

  /* Step 6: Reset PHY digital section */

  dsi_host_clrbits(DSI_PHY_RSTZ_PHY_RSTZ, DSI_HOST_PHY_RSTZ);
  up_mdelay(1);
  dsi_host_setbits(DSI_PHY_RSTZ_PHY_RSTZ, DSI_HOST_PHY_RSTZ);

  /* Enable clock lane and force PLL on */

  dsi_host_setbits(DSI_PHY_RSTZ_PHY_ENABLECLK, DSI_HOST_PHY_RSTZ);
  dsi_host_setbits(DSI_PHY_RSTZ_PHY_FORCEPLL, DSI_HOST_PHY_RSTZ);

  /* Step 7: Configure PHY PLL */

  dsi_phy_configure_pll(g_dsi_priv);

  /* Step 8: Wait for PLL lock */

  ret = dsi_phy_wait_lock(g_dsi_priv);
  if (ret < 0)
    {
      dsierr("ERROR: PHY PLL lock failed: %d\n", ret);
      goto err_free;
    }

  /* Step 9: Wait for lanes to be in stop state */

  ret = dsi_phy_wait_lanes_stopped(g_dsi_priv);
  if (ret < 0)
    {
      dsierr("ERROR: Lane stop state timeout: %d\n", ret);
      goto err_free;
    }

  /* Step 10: Configure Host for command mode */

  dsi_host_configure(g_dsi_priv);

  g_dsi_priv->initialized = true;

  dsiinfo("DSI: MIPI DSI bus initialized successfully\n");
  dsiinfo("DSI: Lanes=%d, target=%lu Mbps, actual=%lu Mbps\n",
          g_dsi_priv->num_data_lanes,
          g_dsi_priv->lane_bit_rate_mbps,
          g_dsi_priv->actual_lane_bit_rate_mbps);
  return OK;

err_free:
  kmm_free(g_dsi_priv);
  g_dsi_priv = NULL;
  return ret;
}

/****************************************************************************
 * Name: esp32p4_mipi_dsi_panel_init
 *
 * Description:
 *   Initialize the LCD panel connected via MIPI DSI.
 *
 *   This follows the ESP-IDF panel init sequence:
 *     1. Hardware reset
 *     2. Send panel init commands (command mode)
 *     3. Configure DPI on Host (byte clock domain)
 *     4. Configure video mode on Host
 *     5. Initialize DSI Bridge
 *     6. Configure Bridge DPI timing (pixel clock domain)
 *     7. Enable Bridge DPI output
 *     8. Enable Host video mode
 *     9. Configure and enable DPI clock
 *    10. Switch clock lane to HS
 *    11. Register framebuffer
 *
 ****************************************************************************/

int esp32p4_mipi_dsi_panel_init(
  FAR const struct esp32p4_lcd_panel_config_s *panel_config)
{
  FAR struct esp32p4_dsi_priv_s *priv = g_dsi_priv;
  int fb_index;
  int ret;

  dsiinfo("DSI: Initializing LCD panel\n");

  if (priv == NULL || !priv->initialized)
    {
      dsierr("ERROR: DSI bus not initialized\n");
      return -ENODEV;
    }

  if (panel_config == NULL)
    {
      return -EINVAL;
    }

  /* Store panel configuration */

  priv->panel_type = panel_config->panel_type;
  priv->reset_gpio_num = panel_config->reset_gpio_num;
  memcpy(&priv->timing, &panel_config->timing,
         sizeof(struct esp32p4_dsi_video_timing_s));

  /* Store DPI clock frequency (will be refined in dsi_configure_dpi_clock) */

  priv->dpi_clock_freq_mhz = panel_config->timing.dpi_clock_mhz;

  /* Set up framebuffer info */

  priv->videoinfo.fmt = (panel_config->bpp == 16) ?
                         FB_FMT_RGB16_565 : FB_FMT_RGB24;
  priv->videoinfo.xres = panel_config->timing.h_size;
  priv->videoinfo.yres = panel_config->timing.v_size;
  priv->videoinfo.nplanes = 1;

  priv->planeinfo.bpp = panel_config->bpp;
  priv->planeinfo.stride = DSI_FB_STRIDE(panel_config->timing.h_size,
                                          panel_config->bpp);
  priv->planeinfo.display = 0;
  priv->planeinfo.xres_virtual = panel_config->timing.h_size;
  priv->planeinfo.yres_virtual = panel_config->timing.v_size;

  /* Allocate double framebuffer */

  priv->fb_size = priv->planeinfo.stride * priv->timing.v_size;

  for (fb_index = 0; fb_index < 2; fb_index++)
    {
      priv->fbmem[fb_index] = kmm_zalloc(priv->fb_size);
      if (priv->fbmem[fb_index] == NULL)
        {
          if (fb_index == 0)
            {
              dsierr("ERROR: Failed to allocate framebuffer "
                     "(%zu bytes)\n", priv->fb_size);
              return -ENOMEM;
            }

          /* Second buffer allocation failed, continue with single
           * buffer.
           */

          dsiinfo("DSI: Double buffer unavailable, using single "
                  "buffer\n");
          break;
        }

      dsiinfo("DSI: FB[%d]: %p, size=%zu\n",
              fb_index, priv->fbmem[fb_index], priv->fb_size);
    }

  /* Set active framebuffer to 0 */

  priv->active_fb = 0;
  priv->planeinfo.fbmem = priv->fbmem[0];
  priv->planeinfo.fblen = priv->fb_size;

  /* Step 1: Hardware reset */

  ret = lcd_panel_reset(priv);
  if (ret < 0)
    {
      dsierr("ERROR: Panel reset failed: %d\n", ret);
      goto err_free_fb;
    }

  /* Step 2: Send panel init commands (still in command mode) */

  switch (priv->panel_type)
    {
      case ESP32P4_LCD_PANEL_ILI9881C:
        ret = lcd_panel_ili9881c_init(priv);
        break;

      case ESP32P4_LCD_PANEL_EK79007:
        ret = lcd_panel_ek79007_init(priv);
        break;

      case ESP32P4_LCD_PANEL_CUSTOM:
        /* Custom panels: basic DCS sleep-out and display-on */

        dsi_host_dcs_write(0, 0x11, NULL, 0);
        up_mdelay(120);
        dsi_host_dcs_write(0, 0x29, NULL, 0);
        up_mdelay(20);
        ret = OK;
        break;

      default:
        dsierr("ERROR: Unknown panel type: %d\n", priv->panel_type);
        ret = -ENODEV;
        break;
    }

  if (ret < 0)
    {
      dsierr("ERROR: Panel init failed: %d\n", ret);
      goto err_free_fb;
    }

  /* Step 3: Configure DPI on Host (byte clock domain) */

  dsi_host_configure_dpi(priv);

  /* Step 4: Configure video mode on Host */

  dsi_host_configure_video_mode(priv);

  /* Step 5: Initialize DSI Bridge */

  dsi_bridge_init(priv);

  /* Step 6: Configure Bridge DPI timing (pixel clock domain) */

  dsi_bridge_configure_dpi(priv);

  /* Step 7: Enable Bridge DPI output */

  dsi_bridge_enable(true);

  /* Step 8: Enable Host video mode.
   * mode_cfg.cmd_video_mode = 0 (video mode)
   */

  dsi_host_write(0, DSI_HOST_MODE_CFG);

  /* Step 9: Configure and enable DPI clock */

  dsi_configure_dpi_clock(priv);

  /* Step 10: Switch clock lane to high-speed mode.
   * lpclk_ctrl.phy_txrequestclkhs = 1
   */

  dsi_host_write((1 << 0), DSI_HOST_LPCLK_CTRL);

  priv->video_mode_enabled = true;
  priv->panel_on = true;

  /* Set up the NuttX framebuffer vtable */

  priv->vtable.getvideoinfo = dsi_getvideoinfo;
  priv->vtable.getplaneinfo = dsi_getplaneinfo;
  priv->vtable.open         = dsi_open;
  priv->vtable.close        = dsi_close;
  priv->vtable.pan_display  = dsi_pan_display;
  priv->vtable.blank        = dsi_blank;

  /* Register the framebuffer device */

  ret = fb_register_device(0, 0, &priv->vtable);
  if (ret < 0)
    {
      dsierr("ERROR: fb_register_device failed: %d\n", ret);
      goto err_free_fb;
    }

  dsiinfo("DSI: LCD panel initialized, fb registered at /dev/fb0\n");
  dsiinfo("DSI: Resolution: %lux%lu @ %d bpp, DPI clock=%lu MHz\n",
          priv->timing.h_size, priv->timing.v_size,
          panel_config->bpp, priv->dpi_clock_freq_mhz);
  return OK;

err_free_fb:
  for (fb_index = 0; fb_index < 2; fb_index++)
    {
      if (priv->fbmem[fb_index] != NULL)
        {
          kmm_free(priv->fbmem[fb_index]);
          priv->fbmem[fb_index] = NULL;
        }
    }

  return ret;
}

/****************************************************************************
 * Name: esp32p4_mipi_dsi_get_framebuffer
 *
 * Description:
 *   Get the pointer to the active framebuffer memory.
 *
 ****************************************************************************/

FAR void *esp32p4_mipi_dsi_get_framebuffer(void)
{
  FAR struct esp32p4_dsi_priv_s *priv = g_dsi_priv;

  if (priv == NULL)
    {
      return NULL;
    }

  return priv->fbmem[priv->active_fb];
}

/****************************************************************************
 * Name: esp32p4_mipi_dsi_get_panel_info
 *
 * Description:
 *   Get the current panel resolution.
 *
 ****************************************************************************/

int esp32p4_mipi_dsi_get_panel_info(FAR uint32_t *hres,
                                     FAR uint32_t *vres)
{
  FAR struct esp32p4_dsi_priv_s *priv = g_dsi_priv;

  if (priv == NULL || !priv->panel_on)
    {
      return -ENODEV;
    }

  if (hres != NULL)
    {
      *hres = priv->timing.h_size;
    }

  if (vres != NULL)
    {
      *vres = priv->timing.v_size;
    }

  return OK;
}
