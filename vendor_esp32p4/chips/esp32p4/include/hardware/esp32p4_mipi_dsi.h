/****************************************************************************
 * vendor_esp32p4/chips/esp32p4/include/hardware/esp32p4_mipi_dsi.h
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

#ifndef __VENDOR_ESP32P4_CHIPS_ESP32P4_INCLUDE_HARDWARE_ESP32P4_MIPI_DSI_H
#define __VENDOR_ESP32P4_CHIPS_ESP32P4_INCLUDE_HARDWARE_ESP32P4_MIPI_DSI_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdint.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* MIPI DSI Host Register Base Address */

#define ESP32P4_MIPI_DSI_HOST_BASE    0x60086000

/* MIPI DSI Bridge Register Base Address */

#define ESP32P4_MIPI_DSI_BRG_BASE     0x60086400

/* MIPI DSI PHY Register Base Address (through host registers) */

/* DSI Host Register Offsets */

#define DSI_HOST_VERSION              0x000
#define DSI_HOST_PWR_UP               0x004
#define DSI_HOST_CLKMGR_CFG           0x008
#define DSI_HOST_DPI_VCID             0x00c
#define DSI_HOST_DPI_COLOR_CODING     0x010
#define DSI_HOST_DPI_CFG_POL          0x014
#define DSI_HOST_DPI_LP_CMD_TIM       0x018
#define DSI_HOST_DBI_VCID             0x01c
#define DSI_HOST_DBI_CFG              0x020
#define DSI_HOST_DBI_PARTITIONING_EN  0x024
#define DSI_HOST_DBI_CMDSIZE          0x028
#define DSI_HOST_PCKHDL_CFG           0x02c
#define DSI_HOST_GEN_VCID             0x030
#define DSI_HOST_MODE_CFG             0x034
#define DSI_HOST_VID_MODE_CFG         0x038
#define DSI_HOST_VID_PKT_SIZE         0x03c
#define DSI_HOST_VID_NUM_CHUNKS       0x040
#define DSI_HOST_VID_NULL_SIZE        0x044
#define DSI_HOST_VID_HSA_TIME         0x048
#define DSI_HOST_VID_HBP_TIME         0x04c
#define DSI_HOST_VID_HLINE_TIME       0x050
#define DSI_HOST_VID_VSA_LINES        0x054
#define DSI_HOST_VID_VBP_LINES        0x058
#define DSI_HOST_VID_VFP_LINES        0x05c
#define DSI_HOST_VID_VACTIVE_LINES    0x060
#define DSI_HOST_CMD_MODE_CFG         0x068
#define DSI_HOST_GEN_HDR              0x06c
#define DSI_HOST_GEN_PLD_DATA         0x070
#define DSI_HOST_CMD_PKT_STATUS       0x074
#define DSI_HOST_TO_CNT_CFG           0x078
#define DSI_HOST_HS_RD_TIMEOUT_CNT    0x07c
#define DSI_HOST_LP_RD_TIMEOUT_CNT    0x080
#define DSI_HOST_HS_WR_TIMEOUT_CNT    0x084
#define DSI_HOST_LP_WR_TIMEOUT_CNT    0x088
#define DSI_HOST_BTA_TIMEOUT_CNT      0x08c
#define DSI_HOST_SDF_3D               0x090
#define DSI_HOST_LPCLK_CTRL           0x094
#define DSI_HOST_PHY_TMR_LPCLK_CFG   0x098
#define DSI_HOST_PHY_TMR_CFG         0x09c
#define DSI_HOST_PHY_RSTZ             0x0a0
#define DSI_HOST_PHY_IF_CFG           0x0a4
#define DSI_HOST_PHY_ULPS_CTRL        0x0a8
#define DSI_HOST_PHY_TX_TRIGGERS      0x0ac
#define DSI_HOST_PHY_STATUS           0x0b0
#define DSI_HOST_PHY_TMR_RD_CFG       0x0b4
#define DSI_HOST_INT_ST0              0x0b8
#define DSI_HOST_INT_ST1              0x0bc
#define DSI_HOST_INT_MASK0            0x0c0
#define DSI_HOST_INT_MASK1            0x0c4
#define DSI_HOST_INT_FORCE0           0x0c8
#define DSI_HOST_INT_FORCE1           0x0cc
#define DSI_HOST_VID_SHADOW_CTRL      0x100
#define DSI_HOST_DPI_VCID_ACT         0x10c
#define DSI_HOST_DPI_COLOR_CODING_ACT 0x110
#define DSI_HOST_DPI_CFG_POL_ACT      0x114
#define DSI_HOST_DPI_LP_CMD_TIM_ACT   0x118
#define DSI_HOST_VID_MODE_CFG_ACT     0x138
#define DSI_HOST_VID_PKT_SIZE_ACT     0x13c
#define DSI_HOST_VID_NUM_CHUNKS_ACT   0x140
#define DSI_HOST_VID_NULL_SIZE_ACT    0x144
#define DSI_HOST_VID_HSA_TIME_ACT     0x148
#define DSI_HOST_VID_HBP_TIME_ACT     0x14c
#define DSI_HOST_VID_HLINE_TIME_ACT   0x150
#define DSI_HOST_VID_VSA_LINES_ACT    0x154
#define DSI_HOST_VID_VBP_LINES_ACT    0x158
#define DSI_HOST_VID_VFP_LINES_ACT    0x15c
#define DSI_HOST_VID_VACTIVE_LINES_ACT 0x160

/* DSI Bridge Register Offsets */

#define DSI_BRG_CLK_EN                0x000
#define DSI_BRG_EN                    0x004
#define DSI_BRG_DMA_REQ_CFG           0x008
#define DSI_BRG_INT_ENA               0x00c
#define DSI_BRG_INT_RAW               0x010
#define DSI_BRG_INT_ST                0x014
#define DSI_BRG_INT_CLR              0x018

/* DSI Host Power Up Register Bits */

#define DSI_HOST_PWR_UP_SHUTDOWNZ     (1 << 0)

/* DSI Host Mode Config Register Bits */

#define DSI_HOST_MODE_CFG_CMD_VIDEO   (1 << 0)  /* 0=video, 1=command */

/* DSI PHY Register Bits */

#define DSI_PHY_RSTZ_PHY_SHUTDOWNZ   (1 << 0)
#define DSI_PHY_RSTZ_PHY_RSTZ        (1 << 1)
#define DSI_PHY_RSTZ_PHY_ENABLECLK   (1 << 2)
#define DSI_PHY_RSTZ_PHY_FORCEPLL    (1 << 3)

#define DSI_PHY_STATUS_PHY_LOCK       (1 << 0)

/* DSI Host Color Coding Values */

#define DSI_COLOR_CODING_16BIT_1      0
#define DSI_COLOR_CODING_16BIT_2      1
#define DSI_COLOR_CODING_16BIT_3      2
#define DSI_COLOR_CODING_18BIT_1      3
#define DSI_COLOR_CODING_18BIT_2      4
#define DSI_COLOR_CODING_24BIT        5

/* DSI Video Mode Types */

#define DSI_VIDEO_MODE_NON_BURST_SYNC_PULSES  0
#define DSI_VIDEO_MODE_NON_BURST_SYNC_EVENTS  1
#define DSI_VIDEO_MODE_BURST                   2

/* DSI Data Types for Generic Interface */

#define DSI_DT_DCS_SHORT_WRITE_0      0x05
#define DSI_DT_DCS_SHORT_WRITE_1      0x15
#define DSI_DT_DCS_LONG_WRITE         0x39
#define DSI_DT_DCS_READ_0             0x06
#define DSI_DT_GENERIC_SHORT_WRITE_0  0x03
#define DSI_DT_GENERIC_SHORT_WRITE_1  0x13
#define DSI_DT_GENERIC_SHORT_WRITE_2  0x23
#define DSI_DT_GENERIC_LONG_WRITE     0x29

/* DSI Command Packet Status Bits */

#define DSI_CMD_PKT_STATUS_GEN_CMD_EMPTY    (1 << 0)
#define DSI_CMD_PKT_STATUS_GEN_CMD_FULL     (1 << 1)
#define DSI_CMD_PKT_STATUS_GEN_PLD_W_EMPTY  (1 << 2)
#define DSI_CMD_PKT_STATUS_GEN_PLD_W_FULL   (1 << 3)
#define DSI_CMD_PKT_STATUS_GEN_PLD_R_EMPTY  (1 << 4)
#define DSI_CMD_PKT_STATUS_GEN_PLD_R_FULL   (1 << 5)
#define DSI_CMD_PKT_STATUS_GEN_RD_CMD_BUSY  (1 << 6)

/* Default LCD Panel Configurations */

/* ILI9881C: 800x1280 @ 60Hz, DPI clock = 80MHz */

#define ILI9881C_HRES             800
#define ILI9881C_VRES             1280
#define ILI9881C_DPI_CLK_MHZ      80
#define ILI9881C_HSYNC             40
#define ILI9881C_HBP              140
#define ILI9881C_HFP               40
#define ILI9881C_VSYNC              4
#define ILI9881C_VBP               16
#define ILI9881C_VFP               16

/* EK79007: 1024x600 @ 60Hz, DPI clock = 48MHz */

#define EK79007_HRES             1024
#define EK79007_VRES              600
#define EK79007_DPI_CLK_MHZ       48
#define EK79007_HSYNC              10
#define EK79007_HBP               120
#define EK79007_HFP               120
#define EK79007_VSYNC               1
#define EK79007_VBP                20
#define EK79007_VFP                10

/****************************************************************************
 * Public Types
 ****************************************************************************/

/* LCD panel type enumeration */

enum esp32p4_lcd_panel_e
{
  ESP32P4_LCD_PANEL_ILI9881C = 0, /* ILI9881C: 800x1280 */
  ESP32P4_LCD_PANEL_EK79007,      /* EK79007: 1024x600 */
  ESP32P4_LCD_PANEL_CUSTOM,       /* Custom panel */
};

/* Video timing configuration */

struct esp32p4_dsi_video_timing_s
{
  uint32_t h_size;            /* Horizontal resolution */
  uint32_t v_size;            /* Vertical resolution */
  uint32_t hsync_pulse_width; /* HSYNC width in pixel clocks */
  uint32_t hsync_back_porch;  /* HSYNC back porch */
  uint32_t hsync_front_porch; /* HSYNC front porch */
  uint32_t vsync_pulse_width; /* VSYNC width in lines */
  uint32_t vsync_back_porch;  /* VSYNC back porch */
  uint32_t vsync_front_porch; /* VSYNC front porch */
  uint32_t dpi_clock_mhz;    /* DPI clock in MHz */
};

/* MIPI DSI bus configuration */

struct esp32p4_dsi_bus_config_s
{
  uint8_t num_data_lanes;       /* Number of data lanes (1 or 2) */
  uint32_t lane_bit_rate_mbps;  /* Lane bit rate in Mbps */
  uint32_t phy_clk_freq_hz;     /* PHY config clock frequency */
};

/* LCD panel device configuration */

struct esp32p4_lcd_panel_config_s
{
  enum esp32p4_lcd_panel_e panel_type;  /* Panel type */
  int reset_gpio_num;                    /* Reset GPIO pin (-1 if not used) */
  struct esp32p4_dsi_video_timing_s timing; /* Video timing */
  uint8_t bpp;                           /* Bits per pixel (16 or 24) */
};

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifdef __ASSEMBLY__

#  define EXTERN extern
#else

#  ifdef __cplusplus
#    define EXTERN extern "C"
extern "C"
{
#  else
#    define EXTERN extern
#  endif

/****************************************************************************
 * Name: esp32p4_mipi_dsi_initialize
 *
 * Description:
 *   Initialize the MIPI DSI bus, PHY, and host controller.
 *
 * Input Parameters:
 *   bus_config - Pointer to DSI bus configuration
 *
 * Returned Value:
 *   Zero (OK) on success; a negated errno on failure.
 *
 ****************************************************************************/

int esp32p4_mipi_dsi_initialize(
  FAR const struct esp32p4_dsi_bus_config_s *bus_config);

/****************************************************************************
 * Name: esp32p4_mipi_dsi_panel_init
 *
 * Description:
 *   Initialize the LCD panel connected via MIPI DSI.
 *   This function configures the DSI host for video mode and sends
 *   the panel initialization sequence.
 *
 * Input Parameters:
 *   panel_config - Pointer to LCD panel configuration
 *
 * Returned Value:
 *   Zero (OK) on success; a negated errno on failure.
 *
 ****************************************************************************/

int esp32p4_mipi_dsi_panel_init(
  FAR const struct esp32p4_lcd_panel_config_s *panel_config);

/****************************************************************************
 * Name: esp32p4_mipi_dsi_get_framebuffer
 *
 * Description:
 *   Get the pointer to the allocated framebuffer memory.
 *
 * Returned Value:
 *   Pointer to framebuffer memory, or NULL if not initialized.
 *
 ****************************************************************************/

FAR void *esp32p4_mipi_dsi_get_framebuffer(void);

/****************************************************************************
 * Name: esp32p4_mipi_dsi_get_panel_info
 *
 * Description:
 *   Get the current panel resolution and timing information.
 *
 * Input Parameters:
 *   hres - Pointer to store horizontal resolution
 *   vres - Pointer to store vertical resolution
 *
 * Returned Value:
 *   Zero (OK) on success; a negated errno on failure.
 *
 ****************************************************************************/

int esp32p4_mipi_dsi_get_panel_info(FAR uint32_t *hres,
                                     FAR uint32_t *vres);

#  undef EXTERN
#  ifdef __cplusplus
}
#  endif
#endif /* __ASSEMBLY__ */

#endif /* __VENDOR_ESP32P4_CHIPS_ESP32P4_INCLUDE_HARDWARE_ESP32P4_MIPI_DSI_H */
