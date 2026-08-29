/****************************************************************************
 * vendor_esp32p4/chips/esp32p4/esp32p4_sdmmc.c
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
 * This driver implements the ESP32-P4 SDMMC controller for NuttX.
 *
 * It is based on the ESP-IDF sdmmc example and the NuttX MMC/SD driver
 * framework.  The driver supports:
 *
 *   - SDMMC host controller initialization
 *   - SD card detection and initialization
 *   - 1-bit and 4-bit SD bus modes
 *   - Configurable bus frequency
 *   - IDMAC DMA transfers (CONFIG_ESP32P4_SDMMC_DMA)
 *   - NuttX sdio_dev_s interface for MMC/SD block driver
 *
 * The ESP32-P4 SDMMC subsystem consists of:
 *   1. SDMMC Host Controller (DesignWare MCI) - SD protocol engine
 *   2. IDMAC (Internal DMA Controller) - DMA for data transfers
 *      using a ring of 64-byte aligned descriptors
 *   3. GPIO matrix - pin muxing for SD bus signals
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

#include <nuttx/arch.h>
#include <nuttx/sdio.h>
#include <nuttx/irq.h>
#include <nuttx/kmalloc.h>

#include "hardware/esp32p4_soc.h"
#include "hardware/esp32p4_gpio.h"
#include "esp32p4_sdmmc.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* SDMMC Host Controller Base Address
 *
 * On ESP32-P4 the SDMMC/SDHOST controller is accessed through the
 * High-Performance Peripheral bus at 0x50083000 (cacheable).
 * The 0x60007xxx range is the legacy APB mapping and is not correct
 * for ESP32-P4.  The register layout follows the DesignWare Mobile
 * Storage Host (MCI) controller v3.x specification.
 */

#define SDMMC_BASE                  0x50083000

/* SDMMC Host Controller Registers (DesignWare MCI layout) */

#define SDMMC_CTRL_REG              (SDMMC_BASE + 0x000)
#define SDMMC_CLKDIV_REG           (SDMMC_BASE + 0x008)
#define SDMMC_CLKENA_REG           (SDMMC_BASE + 0x010)
#define SDMMC_TMOUT_REG            (SDMMC_BASE + 0x014)
#define SDMMC_CTYPE_REG            (SDMMC_BASE + 0x018)
#define SDMMC_BLKSIZ_REG           (SDMMC_BASE + 0x01C)
#define SDMMC_BYTCNT_REG           (SDMMC_BASE + 0x020)
#define SDMMC_INTMASK_REG          (SDMMC_BASE + 0x024)
#define SDMMC_CMDARG_REG           (SDMMC_BASE + 0x028)
#define SDMMC_CMD_REG              (SDMMC_BASE + 0x02C)
#define SDMMC_RESP0_REG            (SDMMC_BASE + 0x030)
#define SDMMC_RESP1_REG            (SDMMC_BASE + 0x034)
#define SDMMC_RESP2_REG            (SDMMC_BASE + 0x038)
#define SDMMC_RESP3_REG            (SDMMC_BASE + 0x03C)
#define SDMMC_MINTSTS_REG          (SDMMC_BASE + 0x040)
#define SDMMC_RINTSTS_REG          (SDMMC_BASE + 0x044)
#define SDMMC_STATUS_REG           (SDMMC_BASE + 0x048)
#define SDMMC_FIFOTH_REG           (SDMMC_BASE + 0x04C)
#define SDMMC_CDETECT_REG          (SDMMC_BASE + 0x050)
#define SDMMC_WRTPRT_REG           (SDMMC_BASE + 0x054)
#define SDMMC_TCBCNT_REG           (SDMMC_BASE + 0x05C)
#define SDMMC_TBBCNT_REG           (SDMMC_BASE + 0x060)
#define SDMMC_DEBNCE_REG           (SDMMC_BASE + 0x064)
#define SDMMC_UHS_REG              (SDMMC_BASE + 0x074)
#define SDMMC_RST_N_REG            (SDMMC_BASE + 0x078)

/* IDMAC (Internal DMA Controller) Registers */

#define SDMMC_BMOD_REG             (SDMMC_BASE + 0x080)
#define SDMMC_PLDMND_REG           (SDMMC_BASE + 0x084)
#define SDMMC_DBADDR_REG           (SDMMC_BASE + 0x088)
#define SDMMC_IDSTS_REG            (SDMMC_BASE + 0x08C)
#define SDMMC_IDINTEN_REG          (SDMMC_BASE + 0x090)
#define SDMMC_DSCADDR_REG          (SDMMC_BASE + 0x094)
#define SDMMC_BUFADDR_REG          (SDMMC_BASE + 0x098)

/* SDMMC FIFO base address for data transfers */

#define SDMMC_FIFO_REG             (SDMMC_BASE + 0x200)

/* CTRL register bits */

#define SDMMC_CTRL_RESET            (1 << 0)
#define SDMMC_CTRL_FIFO_RESET       (1 << 1)
#define SDMMC_CTRL_DMA_RESET        (1 << 2)
#define SDMMC_CTRL_INT_ENABLE       (1 << 4)
#define SDMMC_CTRL_DMA_ENABLE       (1 << 5)
#define SDMMC_CTRL_USE_IDMAC        (1 << 25) /* Use Internal DMA Controller */

/* CMD register bits */

#define SDMMC_CMD_START             (1 << 31)
#define SDMMC_CMD_USE_HOLD_REG     (1 << 29)
#define SDMMC_CMD_VOLT_SWITCH      (1 << 28)
#define SDMMC_CMD_BOOT_MODE        (1 << 27)
#define SDMMC_CMD_DISABLE_BOOT     (1 << 26)
#define SDMMC_CMD_EXPECT_BOOT_ACK  (1 << 25)
#define SDMMC_CMD_ENABLE_BOOT      (1 << 24)
#define SDMMC_CMD_CCS_EXP          (1 << 23)
#define SDMMC_CMD_CEATA_RD         (1 << 22)
#define SDMMC_CMD_UPDATE_CLK_ONLY  (1 << 21)
#define SDMMC_CMD_SEND_INIT        (1 << 15)
#define SDMMC_CMD_STOP_ABORT       (1 << 14)
#define SDMMC_CMD_WAIT_PRVDATA     (1 << 13)
#define SDMMC_CMD_SEND_AUTO_STOP   (1 << 12)
#define SDMMC_CMD_STREAM_MODE      (1 << 11)
#define SDMMC_CMD_DATA_WRITE       (1 << 10)
#define SDMMC_CMD_DATA_EXP         (1 << 9)
#define SDMMC_CMD_CHECK_CRC        (1 << 8)
#define SDMMC_CMD_RESP_LONG        (1 << 7)
#define SDMMC_CMD_RESP_EXP         (1 << 6)

/* INTMASK / RINTSTS register bits */

#define SDMMC_INT_SDIO              (1 << 16)
#define SDMMC_INT_EBE               (1 << 15)
#define SDMMC_INT_ACD               (1 << 14)
#define SDMMC_INT_SBE               (1 << 13)
#define SDMMC_INT_HLE               (1 << 12)
#define SDMMC_INT_FRUN              (1 << 11)
#define SDMMC_INT_HTO               (1 << 10)
#define SDMMC_INT_DRTO              (1 << 9)
#define SDMMC_INT_RTO               (1 << 8)
#define SDMMC_INT_DCRC              (1 << 7)
#define SDMMC_INT_RCRC              (1 << 6)
#define SDMMC_INT_RXDR              (1 << 5)
#define SDMMC_INT_TXDR              (1 << 4)
#define SDMMC_INT_DATA_OVER         (1 << 3)
#define SDMMC_INT_CMD_DONE          (1 << 2)
#define SDMMC_INT_RE                (1 << 1)

#define SDMMC_INT_ERROR_MASK        (SDMMC_INT_EBE | SDMMC_INT_SBE | \
                                     SDMMC_INT_HLE | SDMMC_INT_FRUN | \
                                     SDMMC_INT_HTO | SDMMC_INT_DRTO | \
                                     SDMMC_INT_RTO | SDMMC_INT_DCRC | \
                                     SDMMC_INT_RCRC | SDMMC_INT_RE)

/* STATUS register bits */

#define SDMMC_STATUS_DATA_BUSY      (1 << 9)
#define SDMMC_STATUS_FIFO_FULL      (1 << 3)
#define SDMMC_STATUS_FIFO_EMPTY     (1 << 2)

/* CTYPE register bits */

#define SDMMC_CTYPE_1BIT            0x00000000
#define SDMMC_CTYPE_4BIT            0x00000001
#define SDMMC_CTYPE_8BIT            0x00000010

/* BMOD register bits (Bus Mode / DMA Control) */

#define SDMMC_BMOD_SWR              (1 << 0)   /* Software Reset */
#define SDMMC_BMOD_FB               (1 << 1)   /* Fixed Burst */
#define SDMMC_BMOD_DE               (1 << 7)   /* IDMAC Enable */

/* IDSTS register bits (IDMAC Status) */

#define SDMMC_IDSTS_TI              (1 << 0)   /* Transmit Interrupt */
#define SDMMC_IDSTS_RI              (1 << 1)   /* Receive Interrupt */
#define SDMMC_IDSTS_FBE             (1 << 2)   /* Fatal Bus Error */
#define SDMMC_IDSTS_DU              (1 << 4)   /* Descriptor Unavailable */
#define SDMMC_IDSTS_CES             (1 << 5)   /* Card Error Summary */
#define SDMMC_IDSTS_NIS             (1 << 8)   /* Normal Interrupt Summary */
#define SDMMC_IDSTS_AIS             (1 << 9)   /* Abnormal Interrupt Summary */

#define SDMMC_IDSTS_ERROR_MASK      (SDMMC_IDSTS_FBE | SDMMC_IDSTS_CES | \
                                     SDMMC_IDSTS_DU | SDMMC_IDSTS_AIS)
#define SDMMC_IDSTS_DONE_MASK       (SDMMC_IDSTS_TI | SDMMC_IDSTS_RI | \
                                     SDMMC_IDSTS_NIS)

/* IDINTEN register bits (IDMAC Interrupt Enable) */

#define SDMMC_IDINTEN_TI            (1 << 0)   /* TX Interrupt Enable */
#define SDMMC_IDINTEN_RI            (1 << 1)   /* RX Interrupt Enable */
#define SDMMC_IDINTEN_FBE           (1 << 2)   /* Fatal Bus Error Enable */
#define SDMMC_IDINTEN_DU            (1 << 4)   /* Desc Unavailable Enable */
#define SDMMC_IDINTEN_CES           (1 << 5)   /* Card Error Summary Enable */
#define SDMMC_IDINTEN_NI            (1 << 8)   /* Normal Int Summary Enable */
#define SDMMC_IDINTEN_AI            (1 << 9)   /* Abnormal Int Summary Enable */

/* DMA descriptor flags (DES0 word) */

#define SDMMC_DES0_OWN              (1 << 31)  /* Owned by IDMAC */
#define SDMMC_DES0_CES              (1 << 30)  /* Card Error Summary */
#define SDMMC_DES0_END_OF_RING      (1 << 26)  /* End of Ring */
#define SDMMC_DES0_CHAIN            (1 << 4)   /* Second Address Chained */
#define SDMMC_DES0_FIRST_DESC       (1 << 3)   /* First Descriptor */
#define SDMMC_DES0_LAST_DESC        (1 << 2)   /* Last Descriptor */
#define SDMMC_DES0_DIS_INT          (1 << 1)   /* Disable Interrupt on Compl */

/* DMA maximum buffer length per descriptor */

#define SDMMC_DMA_MAX_BUF_LEN       4096

/* DMA descriptor count (number of descriptors in the ring) */

#define SDMMC_DMA_DESC_COUNT        4

/* CDETECT register bits */

#define SDMMC_CDETECT_CARD_ABSENT   (1 << 0)  /* bit0=1 means no card inserted */

/* CLKENA register bits */

#define SDMMC_CLKENA_ENABLE         (1 << 0)
#define SDMMC_CLKENA_LOW_POWER     (1 << 16)

/* Default timeout values */

#define SDMMC_DEFAULT_CMD_TIMEOUT   0xffffffff
#define SDMMC_DEFAULT_DATA_TIMEOUT  0xffffffff

/* FIFO threshold values */

#define SDMMC_FIFOTH_DEFAULT        0x003f0020

/* Debounce count */

#define SDMMC_DEBNCE_DEFAULT        0x00ffffff

/* SDMMC GPIO pin definitions for ESP32-P4 EVB
 *
 * These are the default SDMMC pin assignments on the ESP32-P4 EVB.
 * CLK:  GPIO 13
 * CMD:  GPIO 14
 * D0:   GPIO 15
 * D1:   GPIO 16
 * D2:   GPIO 17
 * D3:   GPIO 18
 */

#define SDMMC_PIN_CLK               13
#define SDMMC_PIN_CMD               14
#define SDMMC_PIN_D0                15
#define SDMMC_PIN_D1                16
#define SDMMC_PIN_D2                17
#define SDMMC_PIN_D3                18

/* SDMMC bus width */

#define SDMMC_BUS_WIDTH_1BIT        1
#define SDMMC_BUS_WIDTH_4BIT        4

/* SD command indices */

#define SDMMC_CMD0_GO_IDLE          0
#define SDMMC_CMD2_ALL_SEND_CID     2
#define SDMMC_CMD3_SEND_REL_ADDR    3
#define SDMMC_CMD7_SELECT_CARD      7
#define SDMMC_CMD8_SEND_IF_COND     8
#define SDMMC_CMD9_SEND_CSD         9
#define SDMMC_CMD12_STOP_TRANS      12
#define SDMMC_CMD16_SET_BLOCKLEN    16
#define SDMMC_CMD17_READ_SINGLE     17
#define SDMMC_CMD18_READ_MULTIPLE   18
#define SDMMC_CMD24_WRITE_SINGLE    24
#define SDMMC_CMD25_WRITE_MULTIPLE  25
#define SDMMC_CMD55_APP_CMD         55
#define SDMMC_ACMD41_SD_SEND_OP     41

/****************************************************************************
 * Private Types
 ****************************************************************************/

/* IDMAC DMA Descriptor Structure (64 bytes, cache-line aligned)
 *
 * Each descriptor describes a single buffer transfer.  The IDMAC walks
 * a linked list of these descriptors to perform the data move between
 * the SDMMC FIFO and system memory without CPU intervention.
 *
 * The layout matches the DesignWare MCI IDMAC descriptor specification:
 *   Word 0 (DES0) - Control and status flags
 *   Word 1 (DES1) - Buffer sizes
 *   Word 2 (DES2) - Buffer 1 pointer
 *   Word 3 (DES3) - Buffer 2 / next descriptor pointer
 *   Bytes 16..63  - Padding to reach 64-byte cache-line alignment
 *                    (required by ESP32-P4 L1 cache)
 */

struct sdmmc_dma_desc_s
{
  volatile uint32_t  des0;              /* Control / Status flags */
  volatile uint32_t  des1;              /* Buffer1 size | Buffer2 size */
  volatile uint32_t  buffer1_ptr;       /* Physical address of buffer 1 */
  volatile uint32_t  next_desc_ptr;     /* Physical address of next descriptor */
  uint32_t           reserved[12];      /* Pad to 64 bytes (ESP32-P4 cache line) */
};

/* Verify the descriptor is exactly 64 bytes */

_Static_assert(sizeof(struct sdmmc_dma_desc_s) == 64,
               "sdmmc_dma_desc_s must be 64 bytes");

/* DMA Transfer State - tracks an in-progress DMA operation */

struct sdmmc_dma_transfer_s
{
  uint8_t           *ptr;               /* Current data pointer */
  size_t             size_remaining;    /* Bytes left to transfer */
  size_t             next_desc;         /* Index of next descriptor to fill */
  size_t             desc_remaining;    /* Descriptors remaining to fill */
};

/* SDMMC device structure implementing the NuttX sdio_dev_s interface */

struct esp32p4_sdmmc_dev_s
{
  struct sdio_dev_s         dev;           /* Must be first - NuttX SDIO interface */
  uint32_t                  clkdiv;        /* Current clock divider */
  uint32_t                  buswidth;      /* Current bus width (1 or 4) */
  bool                      card_init;     /* Card initialized flag */
  uint32_t                  rca;           /* Relative Card Address */
#ifdef CONFIG_ESP32P4_SDMMC_DMA
  struct sdmmc_dma_desc_s  *dma_desc;      /* DMA descriptor ring (64B aligned) */
  size_t                    dma_desc_num;  /* Number of descriptors in ring */
  struct sdmmc_dma_transfer_s dma_xfer;   /* Current DMA transfer state */
  bool                      dma_active;    /* DMA transfer in progress */
  sdio_eventset_t           dma_eventset;  /* Requested DMA events */
  sdio_callback_t           dma_callback;  /* DMA completion callback */
  void                     *dma_cb_arg;    /* Callback argument */
#endif
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/* SDMMC low-level operations */

static void sdmmc_reset(void);
static int  sdmmc_send_cmd(uint32_t cmd, uint32_t arg, uint32_t *resp);
static int  sdmmc_wait_cmd_done(void);
static int  sdmmc_set_clock(uint32_t freq_hz);
static void sdmmc_set_bus_width(uint32_t width);
static void sdmmc_configure_pins(void);

/* SD card initialization sequence */

static int  sdmmc_card_init_sequence(struct esp32p4_sdmmc_dev_s *priv);

#ifdef CONFIG_ESP32P4_SDMMC_DMA
/* DMA operations */

static void sdmmc_dma_init(struct esp32p4_sdmmc_dev_s *priv);
static void sdmmc_dma_prepare(struct esp32p4_sdmmc_dev_s *priv,
                              void *buffer, size_t buflen,
                              size_t block_size);
static void sdmmc_dma_fill_descriptors(struct esp32p4_sdmmc_dev_s *priv,
                                       size_t num_desc);
static void sdmmc_dma_start(struct esp32p4_sdmmc_dev_s *priv);
static void sdmmc_dma_stop(struct esp32p4_sdmmc_dev_s *priv);
static bool sdmmc_dma_poll_demand(struct esp32p4_sdmmc_dev_s *priv);
static void sdmmc_dma_deinit(struct esp32p4_sdmmc_dev_s *priv);
#endif

/* NuttX sdio_dev_s interface implementation */

static void sdmmc_lock(struct sdio_dev_s *dev, bool lock);
static int  sdmmc_reset_cmd(struct sdio_dev_s *dev);
static int  sdmmc_sendcmd(struct sdio_dev_s *dev, uint32_t cmd,
                           uint32_t arg);
static int  sdmmc_recvsetup(struct sdio_dev_s *dev, uint32_t *buffer,
                             size_t nbytes);
static int  sdmmc_sendsetup(struct sdio_dev_s *dev,
                             const uint32_t *buffer, size_t nbytes);
static int  sdmmc_waitresponse(struct sdio_dev_s *dev, uint32_t cmd);
static int  sdmmc_recvshortcrc(struct sdio_dev_s *dev, uint32_t *cmdresp);
static int  sdmmc_recvshort(struct sdio_dev_s *dev, uint32_t *cmdresp);
static int  sdmmc_recvlong(struct sdio_dev_s *dev, uint32_t *cmdresp);
static int  sdmmc_recvshortocr(struct sdio_dev_s *dev, uint32_t *cmdresp);
static int  sdmmc_recvr7(struct sdio_dev_s *dev, uint32_t *cmdresp);
static int  sdmmc_readwait(struct sdio_dev_s *dev);
static int  sdmmc_cancel(struct sdio_dev_s *dev);
static int  sdmmc_blockcount(struct sdio_dev_s *dev, size_t nblocks);
static int  sdmmc_dmapreflight(struct sdio_dev_s *dev,
                                const uint32_t *buffer, size_t buflen);
static int  sdmmc_dmasetup(struct sdio_dev_s *dev, uint32_t *buffer,
                            size_t buflen);
static void sdmmc_dmastart(struct sdio_dev_s *dev, sdio_eventset_t eventset,
                            sdio_callback_t callback, void *arg);
static void sdmmc_dmastop(struct sdio_dev_s *dev);
static sdio_eventset_t sdmmc_dmacapabilities(struct sdio_dev_s *dev);
static bool sdmmc_dmadone(struct sdio_dev_s *dev);
static void sdmmc_callback(struct sdio_dev_s *dev, sdio_status_t status);

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* SDMMC device instance */

static struct esp32p4_sdmmc_dev_s g_sdmmc_dev =
{
  .clkdiv     = 0,
  .buswidth   = SDMMC_BUS_WIDTH_1BIT,
  .card_init  = false,
  .rca        = 0,
#ifdef CONFIG_ESP32P4_SDMMC_DMA
  .dma_desc   = NULL,
  .dma_desc_num = SDMMC_DMA_DESC_COUNT,
  .dma_active = false,
  .dma_eventset = 0,
  .dma_callback = NULL,
  .dma_cb_arg   = NULL,
#endif
};

/* SDIO operations vtable */

static const struct sdio_dev_s g_sdmmc_ops =
{
  .lock              = sdmmc_lock,
  .reset             = sdmmc_reset_cmd,
  .sendcmd           = sdmmc_sendcmd,
  .recvsetup         = sdmmc_recvsetup,
  .sendsetup         = sdmmc_sendsetup,
  .waitresponse      = sdmmc_waitresponse,
  .recvshortcrc      = sdmmc_recvshortcrc,
  .recvshort         = sdmmc_recvshort,
  .recvlong          = sdmmc_recvlong,
  .recvshortocr      = sdmmc_recvshortocr,
  .recvr7            = sdmmc_recvr7,
  .readwait          = sdmmc_readwait,
  .cancel            = sdmmc_cancel,
  .blockcount        = sdmmc_blockcount,
  .dmapreflight      = sdmmc_dmapreflight,
  .dmasetup          = sdmmc_dmasetup,
  .dmastart          = sdmmc_dmastart,
  .dmastop           = sdmmc_dmastop,
  .dmacapabilities   = sdmmc_dmacapabilities,
  .dmadone           = sdmmc_dmadone,
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: sdmmc_configure_pins
 *
 * Description:
 *   Configure GPIO pins for SDMMC bus signals using IO MUX.
 *
 ****************************************************************************/

static void sdmmc_configure_pins(void)
{
  /* Configure SDMMC CLK pin (GPIO 13) */

  REG_WRITE(GPIO_FUNC_OUT_SEL_CFG_REG(SDMMC_PIN_CLK), 0x100);
  REG_WRITE(GPIO_FUNC_IN_SEL_CFG_REG(SDMMC_PIN_CLK),
            SDMMC_PIN_CLK & 0x3f);

  /* Configure SDMMC CMD pin (GPIO 14) - bidirectional */

  REG_WRITE(GPIO_FUNC_OUT_SEL_CFG_REG(SDMMC_PIN_CMD), 0x100);
  REG_WRITE(GPIO_FUNC_IN_SEL_CFG_REG(SDMMC_PIN_CMD),
            SDMMC_PIN_CMD & 0x3f);

  /* Configure SDMMC D0 pin (GPIO 15) - bidirectional */

  REG_WRITE(GPIO_FUNC_OUT_SEL_CFG_REG(SDMMC_PIN_D0), 0x100);
  REG_WRITE(GPIO_FUNC_IN_SEL_CFG_REG(SDMMC_PIN_D0),
            SDMMC_PIN_D0 & 0x3f);

  /* Configure SDMMC D1 pin (GPIO 16) - bidirectional */

  REG_WRITE(GPIO_FUNC_OUT_SEL_CFG_REG(SDMMC_PIN_D1), 0x100);
  REG_WRITE(GPIO_FUNC_IN_SEL_CFG_REG(SDMMC_PIN_D1),
            SDMMC_PIN_D1 & 0x3f);

  /* Configure SDMMC D2 pin (GPIO 17) - bidirectional */

  REG_WRITE(GPIO_FUNC_OUT_SEL_CFG_REG(SDMMC_PIN_D2), 0x100);
  REG_WRITE(GPIO_FUNC_IN_SEL_CFG_REG(SDMMC_PIN_D2),
            SDMMC_PIN_D2 & 0x3f);

  /* Configure SDMMC D3 pin (GPIO 18) - bidirectional */

  REG_WRITE(GPIO_FUNC_OUT_SEL_CFG_REG(SDMMC_PIN_D3), 0x100);
  REG_WRITE(GPIO_FUNC_IN_SEL_CFG_REG(SDMMC_PIN_D3),
            SDMMC_PIN_D3 & 0x3f);

  /* Set all SDMMC pins to GPIO function (function 1) in IO MUX.
   * The actual IO MUX function selection is SoC-specific and may
   * require additional register writes to the IO MUX pad registers.
   * For now, we use the GPIO matrix approach above.
   */
}

/****************************************************************************
 * Name: sdmmc_reset
 *
 * Description:
 *   Reset the SDMMC host controller.
 *
 ****************************************************************************/

static void sdmmc_reset(void)
{
  /* Assert reset to controller, FIFO, and DMA */

  REG_WRITE(SDMMC_CTRL_REG, SDMMC_CTRL_RESET |
                              SDMMC_CTRL_FIFO_RESET |
                              SDMMC_CTRL_DMA_RESET);

  /* Wait for reset to complete (self-clearing bits) */

  while (REG_READ(SDMMC_CTRL_REG) & (SDMMC_CTRL_RESET |
                                       SDMMC_CTRL_FIFO_RESET |
                                       SDMMC_CTRL_DMA_RESET))
    {
      up_udelay(10);
    }

  /* Clear all interrupt status bits */

  REG_WRITE(SDMMC_RINTSTS_REG, 0xffffffff);

  /* Set default timeout */

  REG_WRITE(SDMMC_TMOUT_REG, SDMMC_DEFAULT_CMD_TIMEOUT);

  /* Set default block size to 512 bytes */

  REG_WRITE(SDMMC_BLKSIZ_REG, 512);

  /* Set FIFO threshold.
   *
   * FIFOTH register layout (DesignWare MCI):
   *   bits[11:0]   TX_WMARK  - TX FIFO watermark
   *   bits[26:16]  RX_WMARK  - RX FIFO watermark
   *   bits[30:28]  MSIZE     - DMA burst size (multiple transaction size)
   *
   * The FIFO depth is 512 words (2048 bytes).
   * For DMA: TX_WMARK = FIFO_DEPTH - MSIZE, RX_WMARK = MSIZE.
   * MSIZE = 7 means 256-byte bursts (matching IDMAC INCR16).
   */

#ifdef CONFIG_ESP32P4_SDMMC_DMA
  REG_WRITE(SDMMC_FIFOTH_REG,
            (7 << 28) |       /* MSIZE = 7 (256 bytes) */
            (16 << 16) |      /* RX_WMARK = 16 */
            (240 << 0));      /* TX_WMARK = 240 */
#else
  REG_WRITE(SDMMC_FIFOTH_REG, SDMMC_FIFOTH_DEFAULT);
#endif

  /* Set debounce count */

  REG_WRITE(SDMMC_DEBNCE_REG, SDMMC_DEBNCE_DEFAULT);

  /* Enable interrupts.
   *
   * In DMA mode the FIFO data request interrupts (RXDR/TXDR) are not
   * needed because the IDMAC handles all data movement.  In PIO mode
   * those interrupts drive the CPU-driven FIFO read/write loop.
   */

  {
    uint32_t intmask = SDMMC_INT_CMD_DONE |
                       SDMMC_INT_DATA_OVER |
                       SDMMC_INT_ERROR_MASK;

#ifdef CONFIG_ESP32P4_SDMMC_DMA
    /* In DMA mode, the IDMAC handles FIFO data movement.
     * Suppress the FIFO data request interrupts.
     */

    intmask &= ~(SDMMC_INT_RXDR | SDMMC_INT_TXDR);
#endif

    REG_WRITE(SDMMC_INTMASK_REG, intmask);
  }
}

/****************************************************************************
 * Name: sdmmc_set_clock
 *
 * Description:
 *   Set the SDMMC bus clock frequency.
 *
 * Input Parameters:
 *   freq_hz - Desired frequency in Hz
 *
 * Returned Value:
 *   OK on success; negative errno on failure.
 *
 ****************************************************************************/

static int sdmmc_set_clock(uint32_t freq_hz)
{
  uint32_t div;

  if (freq_hz == 0)
    {
      return -EINVAL;
    }

  /* Disable clock before changing divider */

  REG_WRITE(SDMMC_CLKENA_REG, 0);

  /* Calculate clock divider.
   * The SDMMC controller divides the source clock by 2 * div.
   * Source clock is typically 160 MHz (PLL_160M).
   *
   * div = source_freq / (2 * target_freq)
   * Minimum divider is 1 (div=0 means divider bypass).
   */

  if (freq_hz >= 160000000)
    {
      div = 0;  /* Bypass divider - use source clock directly */
    }
  else
    {
      div = 160000000 / (2 * freq_hz);
      if (div < 1)
        {
          div = 1;
        }
    }

  /* Update clock divider register.
   * Use CMD with UPDATE_CLK_ONLY bit to apply the new divider.
   */

  REG_WRITE(SDMMC_CLKDIV_REG, div);

  /* Send update clock command */

  REG_WRITE(SDMMC_CMD_REG, SDMMC_CMD_START |
                             SDMMC_CMD_UPDATE_CLK_ONLY |
                             SDMMC_CMD_WAIT_PRVDATA);

  /* Wait for command completion */

  while (REG_READ(SDMMC_CMD_REG) & SDMMC_CMD_START)
    {
      up_udelay(10);
    }

  /* Enable clock */

  REG_WRITE(SDMMC_CLKENA_REG, SDMMC_CLKENA_ENABLE);

  /* Send another update clock command to apply CLKENA */

  REG_WRITE(SDMMC_CMD_REG, SDMMC_CMD_START |
                             SDMMC_CMD_UPDATE_CLK_ONLY |
                             SDMMC_CMD_WAIT_PRVDATA);

  while (REG_READ(SDMMC_CMD_REG) & SDMMC_CMD_START)
    {
      up_udelay(10);
    }

  return OK;
}

/****************************************************************************
 * Name: sdmmc_set_bus_width
 *
 * Description:
 *   Set the SDMMC bus width.
 *
 * Input Parameters:
 *   width - Bus width (1 or 4)
 *
 ****************************************************************************/

static void sdmmc_set_bus_width(uint32_t width)
{
  if (width == 4)
    {
      REG_WRITE(SDMMC_CTYPE_REG, SDMMC_CTYPE_4BIT);
    }
  else
    {
      REG_WRITE(SDMMC_CTYPE_REG, SDMMC_CTYPE_1BIT);
    }
}

/****************************************************************************
 * Name: sdmmc_wait_cmd_done
 *
 * Description:
 *   Wait for a command to complete.
 *
 * Returned Value:
 *   OK on success; negative errno on failure.
 *
 ****************************************************************************/

static int sdmmc_wait_cmd_done(void)
{
  uint32_t status;
  int timeout = 100000;

  while (timeout-- > 0)
    {
      status = REG_READ(SDMMC_RINTSTS_REG);

      if (status & SDMMC_INT_CMD_DONE)
        {
          /* Clear the command done interrupt */

          REG_WRITE(SDMMC_RINTSTS_REG, SDMMC_INT_CMD_DONE);
          return OK;
        }

      if (status & (SDMMC_INT_RTO | SDMMC_INT_RCRC |
                     SDMMC_INT_RE | SDMMC_INT_HLE))
        {
          /* Clear error interrupts */

          REG_WRITE(SDMMC_RINTSTS_REG, status);
          return -EIO;
        }

      up_udelay(1);
    }

  return -ETIMEDOUT;
}

/****************************************************************************
 * Name: sdmmc_send_cmd
 *
 * Description:
 *   Send an SD command and optionally wait for response.
 *
 * Input Parameters:
 *   cmd  - SD command index (without controller-specific bits)
 *   arg  - Command argument
 *   resp - Pointer to response buffer (up to 4 words for R2)
 *
 * Returned Value:
 *   OK on success; negative errno on failure.
 *
 ****************************************************************************/

static int sdmmc_send_cmd(uint32_t cmd, uint32_t arg, uint32_t *resp)
{
  uint32_t cmdval = SDMMC_CMD_START | SDMMC_CMD_WAIT_PRVDATA;
  int ret;

  /* Clear pending interrupts */

  REG_WRITE(SDMMC_RINTSTS_REG, 0xffffffff);

  /* Set command argument */

  REG_WRITE(SDMMC_CMDARG_REG, arg);

  /* Build command register value based on command type */

  switch (cmd)
    {
      case SDMMC_CMD0_GO_IDLE:
        /* CMD0: no response expected, send init sequence */

        cmdval |= SDMMC_CMD_SEND_INIT;
        break;

      case SDMMC_CMD2_ALL_SEND_CID:
        /* CMD2: R2 response (136 bits) */

        cmdval |= SDMMC_CMD_RESP_EXP | SDMMC_CMD_RESP_LONG |
                  SDMMC_CMD_CHECK_CRC;
        break;

      case SDMMC_CMD3_SEND_REL_ADDR:
      case SDMMC_CMD7_SELECT_CARD:
      case SDMMC_CMD55_APP_CMD:
        /* CMD3/CMD7/CMD55: R1 response (48 bits with CRC) */

        cmdval |= SDMMC_CMD_RESP_EXP | SDMMC_CMD_CHECK_CRC;
        break;

      case SDMMC_CMD8_SEND_IF_COND:
        /* CMD8: R7 response (48 bits) */

        cmdval |= SDMMC_CMD_RESP_EXP | SDMMC_CMD_CHECK_CRC;
        break;

      case SDMMC_CMD9_SEND_CSD:
        /* CMD9: R2 response (136 bits) */

        cmdval |= SDMMC_CMD_RESP_EXP | SDMMC_CMD_RESP_LONG |
                  SDMMC_CMD_CHECK_CRC;
        break;

      case SDMMC_CMD12_STOP_TRANS:
        /* CMD12: R1b response with stop/abort */

        cmdval |= SDMMC_CMD_RESP_EXP | SDMMC_CMD_CHECK_CRC |
                  SDMMC_CMD_STOP_ABORT;
        break;

      case SDMMC_CMD16_SET_BLOCKLEN:
        /* CMD16: R1 response */

        cmdval |= SDMMC_CMD_RESP_EXP | SDMMC_CMD_CHECK_CRC;
        break;

      case SDMMC_CMD17_READ_SINGLE:
        /* CMD17: R1 response, data expected */

        cmdval |= SDMMC_CMD_RESP_EXP | SDMMC_CMD_CHECK_CRC |
                  SDMMC_CMD_DATA_EXP;
        break;

      case SDMMC_CMD18_READ_MULTIPLE:
        /* CMD18: R1 response, data expected */

        cmdval |= SDMMC_CMD_RESP_EXP | SDMMC_CMD_CHECK_CRC |
                  SDMMC_CMD_DATA_EXP;
        break;

      case SDMMC_CMD24_WRITE_SINGLE:
        /* CMD24: R1 response, data write */

        cmdval |= SDMMC_CMD_RESP_EXP | SDMMC_CMD_CHECK_CRC |
                  SDMMC_CMD_DATA_EXP | SDMMC_CMD_DATA_WRITE;
        break;

      case SDMMC_CMD25_WRITE_MULTIPLE:
        /* CMD25: R1 response, data write */

        cmdval |= SDMMC_CMD_RESP_EXP | SDMMC_CMD_CHECK_CRC |
                  SDMMC_CMD_DATA_EXP | SDMMC_CMD_DATA_WRITE;
        break;

      case SDMMC_ACMD41_SD_SEND_OP:
        /* ACMD41: R3 response (no CRC check) */

        cmdval |= SDMMC_CMD_RESP_EXP;
        break;

      default:
        /* Default: R1 response */

        cmdval |= SDMMC_CMD_RESP_EXP | SDMMC_CMD_CHECK_CRC;
        break;
    }

  /* Set the command index in bits [7:0] of the CMD register */

  cmdval |= (cmd & 0xff);

  /* Issue the command */

  REG_WRITE(SDMMC_CMD_REG, cmdval);

  /* Wait for command completion */

  ret = sdmmc_wait_cmd_done();
  if (ret < 0)
    {
      return ret;
    }

  /* Read response if expected */

  if (resp != NULL)
    {
      if (cmd == SDMMC_CMD2_ALL_SEND_CID ||
          cmd == SDMMC_CMD9_SEND_CSD)
        {
          /* R2 response: 136 bits in RESP0-RESP3 */

          resp[0] = REG_READ(SDMMC_RESP0_REG);
          resp[1] = REG_READ(SDMMC_RESP1_REG);
          resp[2] = REG_READ(SDMMC_RESP2_REG);
          resp[3] = REG_READ(SDMMC_RESP3_REG);
        }
      else
        {
          /* R1/R3/R7 response: 48 bits in RESP0 */

          resp[0] = REG_READ(SDMMC_RESP0_REG);
        }
    }

  return OK;
}

/****************************************************************************
 * Name: sdmmc_card_init_sequence
 *
 * Description:
 *   Perform the SD card initialization sequence.
 *
 *   The initialization follows the SD Physical Layer Specification:
 *   1. CMD0  - GO_IDLE_STATE
 *   2. CMD8  - SEND_IF_COND (voltage check)
 *   3. ACMD41 - SD_SEND_OP_COND (wait for card ready)
 *   4. CMD2  - ALL_SEND_CID
 *   5. CMD3  - SEND_RELATIVE_ADDR
 *   6. CMD7  - SELECT_CARD
 *   7. CMD16 - SET_BLOCKLEN (512 bytes)
 *   8. Switch to 4-bit bus mode
 *
 ****************************************************************************/

static int sdmmc_card_init_sequence(struct esp32p4_sdmmc_dev_s *priv)
{
  uint32_t resp[4];
  uint32_t ocr;
  int ret;
  int retries;

  /* Check if card is present */

  if (REG_READ(SDMMC_CDETECT_REG) & SDMMC_CDETECT_CARD_ABSENT)
    {
      sderr("ERROR: No SD card detected\n");
      return -ENODEV;
    }

  sdinfo("SD card detected, starting initialization\n");

  /* Reset the controller */

  sdmmc_reset();

  /* Set initial clock to 400 kHz (identification mode) */

  ret = sdmmc_set_clock(400000);
  if (ret < 0)
    {
      sderr("ERROR: Failed to set initial clock: %d\n", ret);
      return ret;
    }

  /* Set 1-bit bus width initially */

  sdmmc_set_bus_width(SDMMC_BUS_WIDTH_1BIT);

  /* CMD0: GO_IDLE_STATE */

  ret = sdmmc_send_cmd(SDMMC_CMD0_GO_IDLE, 0, NULL);
  if (ret < 0)
    {
      sderr("ERROR: CMD0 failed: %d\n", ret);
      return ret;
    }

  up_udelay(2000);  /* Wait 2ms after GO_IDLE */

  /* CMD8: SEND_IF_COND
   * Argument: 0x000001AA (2.7-3.6V, check pattern 0xAA)
   */

  ret = sdmmc_send_cmd(SDMMC_CMD8_SEND_IF_COND, 0x000001aa, resp);
  if (ret < 0)
    {
      sderr("ERROR: CMD8 failed: %d\n", ret);
      return ret;
    }

  /* Verify check pattern in R7 response */

  if ((resp[0] & 0xff) != 0xaa)
    {
      sderr("ERROR: CMD8 check pattern mismatch: 0x%08" PRIx32 "\n",
            resp[0]);
      return -EIO;
    }

  sdinfo("CMD8 response OK: 0x%08" PRIx32 "\n", resp[0]);

  /* ACMD41: SD_SEND_OP_COND
   * Loop until card signals ready (busy bit cleared).
   * Request high capacity (HCS) and 3.3V (0x00ff8000).
   */

  retries = 1000;
  ocr = 0x00ff8000;  /* 3.3V voltage window */

  while (retries-- > 0)
    {
      /* CMD55 must precede each ACMD */

      ret = sdmmc_send_cmd(SDMMC_CMD55_APP_CMD, 0, resp);
      if (ret < 0)
        {
          sderr("ERROR: CMD55 failed: %d\n", ret);
          return ret;
        }

      /* ACMD41 with HCS bit set */

      ret = sdmmc_send_cmd(SDMMC_ACMD41_SD_SEND_OP,
                            ocr | 0x40000000, resp);
      if (ret < 0)
        {
          sderr("ERROR: ACMD41 failed: %d\n", ret);
          return ret;
        }

      /* Check if card is ready (busy bit = 0) */

      if (resp[0] & 0x80000000)
        {
          sdinfo("ACMD41 card ready, OCR: 0x%08" PRIx32 "\n", resp[0]);
          break;
        }

      up_udelay(1000);
    }

  if (retries <= 0)
    {
      sderr("ERROR: ACMD41 timeout - card not ready\n");
      return -ETIMEDOUT;
    }

  /* CMD2: ALL_SEND_CID */

  ret = sdmmc_send_cmd(SDMMC_CMD2_ALL_SEND_CID, 0, resp);
  if (ret < 0)
    {
      sderr("ERROR: CMD2 failed: %d\n", ret);
      return ret;
    }

  sdinfo("Card CID: %08" PRIx32 " %08" PRIx32 " %08" PRIx32 " %08"
         PRIx32 "\n", resp[0], resp[1], resp[2], resp[3]);

  /* CMD3: SEND_RELATIVE_ADDR */

  ret = sdmmc_send_cmd(SDMMC_CMD3_SEND_REL_ADDR, 0, resp);
  if (ret < 0)
    {
      sderr("ERROR: CMD3 failed: %d\n", ret);
      return ret;
    }

  priv->rca = resp[0] & 0xffff0000;
  sdinfo("Card RCA: 0x%08" PRIx32 "\n", priv->rca);

  /* CMD7: SELECT_CARD */

  ret = sdmmc_send_cmd(SDMMC_CMD7_SELECT_CARD, priv->rca, resp);
  if (ret < 0)
    {
      sderr("ERROR: CMD7 failed: %d\n", ret);
      return ret;
    }

  /* CMD16: SET_BLOCKLEN to 512 bytes */

  ret = sdmmc_send_cmd(SDMMC_CMD16_SET_BLOCKLEN, 512, resp);
  if (ret < 0)
    {
      sderr("ERROR: CMD16 failed: %d\n", ret);
      return ret;
    }

  /* Switch to higher clock (up to 25 MHz for default speed) */

  ret = sdmmc_set_clock(CONFIG_ESP32P4_SDMMC_FREQ > 0 ?
                         CONFIG_ESP32P4_SDMMC_FREQ : 25000000);
  if (ret < 0)
    {
      sderr("ERROR: Failed to set operating clock: %d\n", ret);
      return ret;
    }

  /* Switch to 4-bit bus width */

  sdmmc_set_bus_width(SDMMC_BUS_WIDTH_4BIT);
  priv->buswidth = SDMMC_BUS_WIDTH_4BIT;

  priv->card_init = true;

  sdinfo("SD card initialization complete (RCA=0x%08" PRIx32 ")\n",
         priv->rca);

  return OK;
}

#ifdef CONFIG_ESP32P4_SDMMC_DMA

/****************************************************************************
 * Name: sdmmc_dma_init
 *
 * Description:
 *   Allocate and initialize the IDMAC DMA descriptor ring.
 *
 *   The IDMAC (Internal DMA Controller) is part of the DesignWare MCI
 *   host controller.  It uses a linked list of descriptors (each 64
 *   bytes, cache-line aligned on ESP32-P4) to move data between the
 *   SDMMC FIFO and system memory without CPU intervention.
 *
 *   This function:
 *     1. Allocates a cache-line-aligned descriptor ring
 *     2. Clears all descriptors
 *     3. Configures the BMOD and IDINTEN registers
 *     4. Programs the DBADDR register with the ring base address
 *
 ****************************************************************************/

static void sdmmc_dma_init(struct esp32p4_sdmmc_dev_s *priv)
{
  /* Allocate descriptors with 64-byte alignment.
   * Each descriptor is exactly 64 bytes (one cache line on ESP32-P4).
   */

  priv->dma_desc = (struct sdmmc_dma_desc_s *)
    kmm_memalign(64, sizeof(struct sdmmc_dma_desc_s) * priv->dma_desc_num);

  DEBUGASSERT(priv->dma_desc != NULL);

  /* Zero the descriptor ring - clears the OWNED_BY_IDMAC bit in all
   * descriptors so the IDMAC will not process stale entries.
   */

  memset(priv->dma_desc, 0,
         sizeof(struct sdmmc_dma_desc_s) * priv->dma_desc_num);

  /* Reset the IDMAC internal state via BMOD.SWR */

  REG_WRITE(SDMMC_BMOD_REG, SDMMC_BMOD_SWR);

  /* Wait for the software reset bit to self-clear (one clock cycle) */

  while (REG_READ(SDMMC_BMOD_REG) & SDMMC_BMOD_SWR)
    {
      up_udelay(1);
    }

  /* Enable the IDMAC:
   *   BMOD.DE  = 1 (IDMAC enable)
   *   BMOD.FB  = 1 (Fixed burst - use INCR4/8/16 for better throughput)
   */

  REG_WRITE(SDMMC_BMOD_REG, SDMMC_BMOD_DE | SDMMC_BMOD_FB);

  /* Set the USE_INTERNAL_DMA bit in CTRL to route DMA through the IDMAC
   * (as opposed to an external DMA controller).
   */

  REG_SET_BIT(SDMMC_CTRL_REG, SDMMC_CTRL_USE_IDMAC);

  /* Set the descriptor base address.
   * The LSB two bits are ignored by the hardware.
   */

  REG_WRITE(SDMMC_DBADDR_REG, (uint32_t)priv->dma_desc);

  /* Enable IDMAC interrupts:
   *   TI  - Transmit Interrupt  (write complete)
   *   RI  - Receive Interrupt   (read complete)
   *   NI  - Normal Interrupt Summary
   *   FBE - Fatal Bus Error
   *   AI  - Abnormal Interrupt Summary
   */

  REG_WRITE(SDMMC_IDINTEN_REG,
            SDMMC_IDINTEN_TI | SDMMC_IDINTEN_RI | SDMMC_IDINTEN_NI |
            SDMMC_IDINTEN_FBE | SDMMC_IDINTEN_AI);

  /* Clear any pending IDMAC status bits */

  REG_WRITE(SDMMC_IDSTS_REG, 0xffffffff);

  sdinfo("DMA initialized: desc=%p count=%zu\n",
         priv->dma_desc, priv->dma_desc_num);
}

/****************************************************************************
 * Name: sdmmc_dma_fill_descriptors
 *
 * Description:
 *   Fill up to num_desc descriptors for the current transfer.
 *
 *   This walks the descriptor ring starting at dma_xfer.next_desc and
 *   sets up each descriptor with:
 *     - Buffer1 pointer = current data pointer
 *     - Buffer1 size    = min(remaining, 4096)
 *     - CHAIN flag      = 1 (next_desc_ptr points to following desc)
 *     - LAST_DESC flag  = 1 on the final descriptor
 *     - OWNED_BY_IDMAC  = 1 (hand ownership to the DMA engine)
 *
 ****************************************************************************/

static void sdmmc_dma_fill_descriptors(struct esp32p4_sdmmc_dev_s *priv,
                                       size_t num_desc)
{
  struct sdmmc_dma_transfer_s *xfer = &priv->dma_xfer;
  size_t i;

  for (i = 0; i < num_desc; i++)
    {
      if (xfer->size_remaining == 0)
        {
          break;
        }

      size_t next = xfer->next_desc;
      struct sdmmc_dma_desc_s *desc = &priv->dma_desc[next];

      /* This descriptor must have been released by the IDMAC */

      DEBUGASSERT((desc->des0 & SDMMC_DES0_OWN) == 0);

      /* Calculate buffer size for this descriptor */

      size_t sz = (xfer->size_remaining < SDMMC_DMA_MAX_BUF_LEN) ?
                  xfer->size_remaining : SDMMC_DMA_MAX_BUF_LEN;
      bool last = (sz == xfer->size_remaining);

      /* Fill the descriptor fields */

      desc->des0 = SDMMC_DES0_CHAIN | SDMMC_DES0_OWN;
      if (next == 0)
        {
          desc->des0 |= SDMMC_DES0_FIRST_DESC;
        }

      if (last)
        {
          desc->des0 |= SDMMC_DES0_LAST_DESC;
          desc->next_desc_ptr = 0;
        }
      else
        {
          desc->next_desc_ptr =
            (uint32_t)&priv->dma_desc[(next + 1) % priv->dma_desc_num];
        }

      /* Buffer size in DES1: bits[12:0] = buffer1, bits[25:13] = buffer2 */

      desc->des1 = (sz + 3) & ~3u;  /* Round up to 4-byte boundary */
      desc->buffer1_ptr = (uint32_t)xfer->ptr;

      xfer->size_remaining -= sz;
      xfer->ptr += sz;
      xfer->next_desc = (next + 1) % priv->dma_desc_num;
    }
}

/****************************************************************************
 * Name: sdmmc_dma_prepare
 *
 * Description:
 *   Prepare the DMA engine for a data transfer.
 *
 *   This function:
 *     1. Clears the descriptor ring
 *     2. Sets up the first descriptor
 *     3. Fills the ring with descriptors covering the entire buffer
 *     4. Programs BYTCNT and BLKSIZ
 *     5. Enables DMA in the controller (CTRL.DMA_ENABLE)
 *     6. Writes the descriptor base address (DBADDR)
 *     7. Issues a poll demand to start the IDMAC
 *
 ****************************************************************************/

static void sdmmc_dma_prepare(struct esp32p4_sdmmc_dev_s *priv,
                              void *buffer, size_t buflen,
                              size_t block_size)
{
  /* Clear the descriptor ring - all OWN bits cleared so IDMAC
   * will not process stale entries.
   */

  memset(priv->dma_desc, 0,
         sizeof(struct sdmmc_dma_desc_s) * priv->dma_desc_num);

  /* Save transfer info */

  priv->dma_xfer.ptr = (uint8_t *)buffer;
  priv->dma_xfer.size_remaining = buflen;
  priv->dma_xfer.next_desc = 0;
  priv->dma_xfer.desc_remaining =
    (buflen + SDMMC_DMA_MAX_BUF_LEN - 1) / SDMMC_DMA_MAX_BUF_LEN;

  /* Fill as many descriptors as the ring can hold */

  sdmmc_dma_fill_descriptors(priv, priv->dma_desc_num);

  /* Program the host controller data transfer registers */

  REG_WRITE(SDMMC_BYTCNT_REG, buflen);
  REG_WRITE(SDMMC_BLKSIZ_REG, block_size);

  /* Set the descriptor base address */

  REG_WRITE(SDMMC_DBADDR_REG, (uint32_t)priv->dma_desc);

  /* Enable DMA in the host controller:
   *   CTRL.dma_enable      = 1
   *   CTRL.use_internal_dma = 1 (IDMAC)
   */

  REG_SET_BIT(SDMMC_CTRL_REG, SDMMC_CTRL_DMA_ENABLE);
  REG_SET_BIT(SDMMC_CTRL_REG, SDMMC_CTRL_USE_IDMAC);

  /* Clear any pending IDMAC status */

  REG_WRITE(SDMMC_IDSTS_REG, 0xffffffff);

  priv->dma_active = true;

  sdinfo("DMA prepared: buf=%p len=%zu blk=%zu desc_addr=%p\n",
         buffer, buflen, block_size, priv->dma_desc);
}

/****************************************************************************
 * Name: sdmmc_dma_start
 *
 * Description:
 *   Start the DMA transfer by issuing a poll demand to the IDMAC.
 *
 *   The poll demand write causes the IDMAC to re-read the descriptor
 *   ring and begin fetching data.  Without this, the IDMAC FSM stays
 *   in the SUSPEND state waiting for software to kick it.
 *
 ****************************************************************************/

static void sdmmc_dma_start(struct esp32p4_sdmmc_dev_s *priv)
{
  /* Issue poll demand - any write to PLDMND resumes the IDMAC */

  REG_WRITE(SDMMC_PLDMND_REG, 1);
}

/****************************************************************************
 * Name: sdmmc_dma_stop
 *
 * Description:
 *   Stop any in-progress DMA transfer and reset the DMA engine.
 *
 ****************************************************************************/

static void sdmmc_dma_stop(struct esp32p4_sdmmc_dev_s *priv)
{
  /* Disable DMA in the host controller */

  REG_CLR_BIT(SDMMC_CTRL_REG, SDMMC_CTRL_DMA_ENABLE);
  REG_CLR_BIT(SDMMC_CTRL_REG, SDMMC_CTRL_USE_IDMAC);

  /* Reset the DMA interface */

  REG_SET_BIT(SDMMC_CTRL_REG, SDMMC_CTRL_DMA_RESET);

  while (REG_READ(SDMMC_CTRL_REG) & SDMMC_CTRL_DMA_RESET)
    {
      up_udelay(1);
    }

  /* Disable IDMAC in BMOD */

  REG_CLR_BIT(SDMMC_BMOD_REG, SDMMC_BMOD_DE);

  /* Clear IDMAC status */

  REG_WRITE(SDMMC_IDSTS_REG, 0xffffffff);

  priv->dma_active = false;
}

/****************************************************************************
 * Name: sdmmc_dma_poll_demand
 *
 * Description:
 *   Check if more descriptors need to be fed to the IDMAC and, if so,
 *   issue a poll demand.  This is called from the ISR when the
 *   IDMAC signals Descriptor Unavailable (DU).
 *
 * Returned Value:
 *   true if a poll demand was issued, false if no more work.
 *
 ****************************************************************************/

static bool sdmmc_dma_poll_demand(struct esp32p4_sdmmc_dev_s *priv)
{
  if (priv->dma_xfer.size_remaining > 0)
    {
      sdmmc_dma_fill_descriptors(priv, priv->dma_desc_num);
      REG_WRITE(SDMMC_PLDMND_REG, 1);
      return true;
    }

  return false;
}

/****************************************************************************
 * Name: sdmmc_dma_deinit
 *
 * Description:
 *   Release DMA resources.
 *
 ****************************************************************************/

static void sdmmc_dma_deinit(struct esp32p4_sdmmc_dev_s *priv)
{
  if (priv->dma_desc != NULL)
    {
      sdmmc_dma_stop(priv);
      kmm_free(priv->dma_desc);
      priv->dma_desc = NULL;
    }
}

#endif /* CONFIG_ESP32P4_SDMMC_DMA */

/****************************************************************************
 * NuttX sdio_dev_s Interface Implementation
 ****************************************************************************/

/****************************************************************************
 * Name: sdmmc_lock
 ****************************************************************************/

static void sdmmc_lock(struct sdio_dev_s *dev, bool lock)
{
  /* The SDMMC controller does not need locking for single-threaded
   * access.  For multi-threaded access, a mutex should be added here.
   */
}

/****************************************************************************
 * Name: sdmmc_reset_cmd
 ****************************************************************************/

static int sdmmc_reset_cmd(struct sdio_dev_s *dev)
{
  sdmmc_reset();
  return OK;
}

/****************************************************************************
 * Name: sdmmc_sendcmd
 ****************************************************************************/

static int sdmmc_sendcmd(struct sdio_dev_s *dev, uint32_t cmd,
                          uint32_t arg)
{
  return sdmmc_send_cmd(cmd, arg, NULL);
}

/****************************************************************************
 * Name: sdmmc_recvsetup
 ****************************************************************************/

static int sdmmc_recvsetup(struct sdio_dev_s *dev, uint32_t *buffer,
                            size_t nbytes)
{
  /* Set up for data reception.
   * Set byte count and block size for the transfer.
   */

  REG_WRITE(SDMMC_BYTCNT_REG, nbytes);
  REG_WRITE(SDMMC_BLKSIZ_REG, 512);

  return OK;
}

/****************************************************************************
 * Name: sdmmc_sendsetup
 ****************************************************************************/

static int sdmmc_sendsetup(struct sdio_dev_s *dev,
                            const uint32_t *buffer, size_t nbytes)
{
  /* Set up for data transmission */

  REG_WRITE(SDMMC_BYTCNT_REG, nbytes);
  REG_WRITE(SDMMC_BLKSIZ_REG, 512);

  return OK;
}

/****************************************************************************
 * Name: sdmmc_waitresponse
 ****************************************************************************/

static int sdmmc_waitresponse(struct sdio_dev_s *dev, uint32_t cmd)
{
  return sdmmc_wait_cmd_done();
}

/****************************************************************************
 * Name: sdmmc_recvshortcrc
 ****************************************************************************/

static int sdmmc_recvshortcrc(struct sdio_dev_s *dev, uint32_t *cmdresp)
{
  *cmdresp = REG_READ(SDMMC_RESP0_REG);
  return OK;
}

/****************************************************************************
 * Name: sdmmc_recvshort
 ****************************************************************************/

static int sdmmc_recvshort(struct sdio_dev_s *dev, uint32_t *cmdresp)
{
  *cmdresp = REG_READ(SDMMC_RESP0_REG);
  return OK;
}

/****************************************************************************
 * Name: sdmmc_recvlong
 ****************************************************************************/

static int sdmmc_recvlong(struct sdio_dev_s *dev, uint32_t *cmdresp)
{
  cmdresp[0] = REG_READ(SDMMC_RESP0_REG);
  cmdresp[1] = REG_READ(SDMMC_RESP1_REG);
  cmdresp[2] = REG_READ(SDMMC_RESP2_REG);
  cmdresp[3] = REG_READ(SDMMC_RESP3_REG);
  return OK;
}

/****************************************************************************
 * Name: sdmmc_recvshortocr
 ****************************************************************************/

static int sdmmc_recvshortocr(struct sdio_dev_s *dev, uint32_t *cmdresp)
{
  *cmdresp = REG_READ(SDMMC_RESP0_REG);
  return OK;
}

/****************************************************************************
 * Name: sdmmc_recvr7
 ****************************************************************************/

static int sdmmc_recvr7(struct sdio_dev_s *dev, uint32_t *cmdresp)
{
  *cmdresp = REG_READ(SDMMC_RESP0_REG);
  return OK;
}

/****************************************************************************
 * Name: sdmmc_readwait
 ****************************************************************************/

static int sdmmc_readwait(struct sdio_dev_s *dev)
{
  /* Not implemented - read-wait is not commonly used */

  return -ENOSYS;
}

/****************************************************************************
 * Name: sdmmc_cancel
 ****************************************************************************/

static int sdmmc_cancel(struct sdio_dev_s *dev)
{
#ifdef CONFIG_ESP32P4_SDMMC_DMA
  struct esp32p4_sdmmc_dev_s *priv = (struct esp32p4_sdmmc_dev_s *)dev;

  /* Stop any in-progress DMA first */

  if (priv->dma_active)
    {
      sdmmc_dma_stop(priv);
      priv->dma_callback = NULL;
    }
#endif

  /* Reset the controller to cancel any pending transfer */

  sdmmc_reset();
  return OK;
}

/****************************************************************************
 * Name: sdmmc_blockcount
 ****************************************************************************/

static int sdmmc_blockcount(struct sdio_dev_s *dev, size_t nblocks)
{
  /* The block count is set via BYTCNT register */

  return OK;
}

/****************************************************************************
 * Name: sdmmc_dmapreflight
 ****************************************************************************/

static int sdmmc_dmapreflight(struct sdio_dev_s *dev,
                               const uint32_t *buffer, size_t buflen)
{
  /* Check buffer alignment for DMA.
   * The SDMMC IDMAC requires word-aligned buffers.
   */

  if (((uintptr_t)buffer & 3) != 0)
    {
      return -EINVAL;
    }

  return OK;
}

/****************************************************************************
 * Name: sdmmc_dmasetup
 ****************************************************************************/

static int sdmmc_dmasetup(struct sdio_dev_s *dev, uint32_t *buffer,
                           size_t buflen)
{
  struct esp32p4_sdmmc_dev_s *priv = (struct esp32p4_sdmmc_dev_s *)dev;

#ifdef CONFIG_ESP32P4_SDMMC_DMA
  /* Use the IDMAC DMA engine for the transfer */

  sdmmc_dma_prepare(priv, buffer, buflen, 512);
#else
  /* PIO fallback: just program the byte/block count registers */

  REG_WRITE(SDMMC_BYTCNT_REG, buflen);
  REG_WRITE(SDMMC_BLKSIZ_REG, 512);
  REG_SET_BIT(SDMMC_CTRL_REG, SDMMC_CTRL_DMA_ENABLE);
#endif

  return OK;
}

/****************************************************************************
 * Name: sdmmc_dmastart
 ****************************************************************************/

static void sdmmc_dmastart(struct sdio_dev_s *dev,
                            sdio_eventset_t eventset,
                            sdio_callback_t callback, void *arg)
{
  struct esp32p4_sdmmc_dev_s *priv = (struct esp32p4_sdmmc_dev_s *)dev;

#ifdef CONFIG_ESP32P4_SDMMC_DMA
  /* Save the callback information for the ISR */

  priv->dma_eventset = eventset;
  priv->dma_callback = callback;
  priv->dma_cb_arg   = arg;

  /* Kick the IDMAC to begin fetching descriptors */

  sdmmc_dma_start(priv);
#endif
}

/****************************************************************************
 * Name: sdmmc_dmastop
 ****************************************************************************/

static void sdmmc_dmastop(struct sdio_dev_s *dev)
{
  struct esp32p4_sdmmc_dev_s *priv = (struct esp32p4_sdmmc_dev_s *)dev;

#ifdef CONFIG_ESP32P4_SDMMC_DMA
  sdmmc_dma_stop(priv);
  priv->dma_callback = NULL;
#else
  REG_CLR_BIT(SDMMC_CTRL_REG, SDMMC_CTRL_DMA_ENABLE);
#endif
}

/****************************************************************************
 * Name: sdmmc_dmacapabilities
 ****************************************************************************/

static sdio_eventset_t sdmmc_dmacapabilities(struct sdio_dev_s *dev)
{
  return SDIOWAIT_TRANSFERDONE;
}

/****************************************************************************
 * Name: sdmmc_dmadone
 ****************************************************************************/

static bool sdmmc_dmadone(struct sdio_dev_s *dev)
{
  struct esp32p4_sdmmc_dev_s *priv = (struct esp32p4_sdmmc_dev_s *)dev;
  uint32_t status = REG_READ(SDMMC_RINTSTS_REG);

  /* Check if data transfer is complete (host controller side) */

  if (status & SDMMC_INT_DATA_OVER)
    {
      REG_WRITE(SDMMC_RINTSTS_REG, SDMMC_INT_DATA_OVER);

#ifdef CONFIG_ESP32P4_SDMMC_DMA
      priv->dma_active = false;
#endif

      return true;
    }

#ifdef CONFIG_ESP32P4_SDMMC_DMA
  /* Also check the IDMAC completion status */

  if (priv->dma_active)
    {
      uint32_t idsts = REG_READ(SDMMC_IDSTS_REG);

      if (idsts & SDMMC_IDSTS_TI)
        {
          /* TX complete */

          REG_WRITE(SDMMC_IDSTS_REG, SDMMC_IDSTS_TI);
          priv->dma_active = false;
          return true;
        }

      if (idsts & SDMMC_IDSTS_RI)
        {
          /* RX complete */

          REG_WRITE(SDMMC_IDSTS_REG, SDMMC_IDSTS_RI);
          priv->dma_active = false;
          return true;
        }
    }
#endif

  return false;
}

/****************************************************************************
 * Name: sdmmc_callback
 ****************************************************************************/

static void sdmmc_callback(struct sdio_dev_s *dev, sdio_status_t status)
{
  /* Default callback - does nothing.
   * The board-level code can override this via dmasetup/dmastart.
   */
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: sdmmc_initialize
 *
 * Description:
 *   Initialize the SDMMC controller and return an SDIO device interface.
 *
 *   This function initializes the SDMMC host controller, configures the
 *   GPIO pins for the SDMMC bus, and performs the SD card initialization
 *   sequence.
 *
 * Input Parameters:
 *   slot - SDMMC slot number (0 for the primary slot)
 *
 * Returned Value:
 *   A pointer to the sdio_dev_s interface on success;
 *   NULL on failure.
 *
 ****************************************************************************/

struct sdio_dev_s *sdmmc_initialize(int slot)
{
  struct esp32p4_sdmmc_dev_s *priv = &g_sdmmc_dev;
  int ret;

  if (slot != 0)
    {
      sderr("ERROR: Only slot 0 is supported\n");
      return NULL;
    }

  sdinfo("Initializing SDMMC controller (slot %d)\n", slot);

  /* Set up the sdio_dev_s operations vtable */

  memcpy(&priv->dev, &g_sdmmc_ops, sizeof(g_sdmmc_ops));

  /* Configure SDMMC GPIO pins */

  sdmmc_configure_pins();

#ifdef CONFIG_ESP32P4_SDMMC_DMA
  /* Initialize the IDMAC DMA engine */

  sdmmc_dma_init(priv);
#endif

  /* Perform SD card initialization sequence */

  ret = sdmmc_card_init_sequence(priv);
  if (ret < 0)
    {
      sderr("ERROR: SD card initialization failed: %d\n", ret);
#ifdef CONFIG_ESP32P4_SDMMC_DMA
      sdmmc_dma_deinit(priv);
#endif
      return NULL;
    }

  sdinfo("SDMMC initialized with %s mode\n",
#ifdef CONFIG_ESP32P4_SDMMC_DMA
         "DMA"
#else
         "PIO"
#endif
         );

  return &priv->dev;
}
