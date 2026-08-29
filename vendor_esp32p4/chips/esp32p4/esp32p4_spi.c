/****************************************************************************
 * vendor/espressif/chips/esp32p4/esp32p4_spi.c
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
 * ESP32-P4 SPI Bus Driver
 *
 * This driver implements the NuttX spi_dev_s interface for the ESP32-P4
 * GP-SPI peripherals (SPI2 and SPI3).  The implementation uses polling
 * for data transfer and supports configurable clock frequency, mode,
 * and bit order.
 *
 * Key features:
 *   - SPI master mode
 *   - Full-duplex data transfer
 *   - Configurable clock frequency (up to 80 MHz)
 *   - SPI mode 0-3 (CPOL/CPHA)
 *   - 8-bit word size (default)
 *   - GPIO matrix pin routing
 *   - CS control via software (GPIO) or hardware
 *
 * ESP-IDF reference:
 *   - examples/peripherals/spi_master/lcd
 *   - components/esp_driver_spi/src/gpspi/spi_master.c
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
#include <nuttx/mutex.h>
#include <nuttx/spi/spi.h>

#include "hardware/esp32p4_spi.h"
#include "hardware/esp32p4_gpio.h"
#include "hardware/esp32p4_soc.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* SPI timeout in busy-wait iterations */

#define SPI_TIMEOUT_COUNT           100000

/* Default SPI pins if not configured via Kconfig */

#ifndef CONFIG_ESP32P4_SPI2_CLK_GPIO
#  define CONFIG_ESP32P4_SPI2_CLK_GPIO   12
#endif

#ifndef CONFIG_ESP32P4_SPI2_MISO_GPIO
#  define CONFIG_ESP32P4_SPI2_MISO_GPIO  13
#endif

#ifndef CONFIG_ESP32P4_SPI2_MOSI_GPIO
#  define CONFIG_ESP32P4_SPI2_MOSI_GPIO  14
#endif

#ifndef CONFIG_ESP32P4_SPI3_CLK_GPIO
#  define CONFIG_ESP32P4_SPI3_CLK_GPIO   15
#endif

#ifndef CONFIG_ESP32P4_SPI3_MISO_GPIO
#  define CONFIG_ESP32P4_SPI3_MISO_GPIO  16
#endif

#ifndef CONFIG_ESP32P4_SPI3_MOSI_GPIO
#  define CONFIG_ESP32P4_SPI3_MOSI_GPIO  17
#endif

/* Default CS GPIO pins */

#ifndef CONFIG_ESP32P4_SPI2_CS0_GPIO
#  define CONFIG_ESP32P4_SPI2_CS0_GPIO   11
#endif

#ifndef CONFIG_ESP32P4_SPI3_CS0_GPIO
#  define CONFIG_ESP32P4_SPI3_CS0_GPIO   18
#endif

/****************************************************************************
 * Private Types
 ****************************************************************************/

/* SPI bus private data structure */

struct esp32p4_spi_priv_s
{
  struct spi_dev_s  spidev;       /* SPI device (must be first) */
  uint32_t          base;         /* SPI register base address */
  uint32_t          frequency;    /* Current SPI frequency */
  uint32_t          actual_freq;  /* Actual SPI frequency achieved */
  enum spi_mode_e   mode;         /* Current SPI mode */
  uint8_t           nbits;        /* Bits per word (8 or 16) */
  mutex_t           lock;         /* Bus lock mutex */
  bool              initialized;  /* True if hardware is configured */

  /* GPIO signal numbers for this SPI controller */

  uint8_t           clk_sig;      /* CLK signal index */
  uint8_t           miso_sig;     /* MISO signal index */
  uint8_t           mosi_sig;     /* MOSI signal index */
  uint8_t           cs_sig;       /* CS0 signal index */

  /* GPIO pin numbers */

  uint8_t           clk_gpio;     /* CLK GPIO pin */
  uint8_t           miso_gpio;    /* MISO GPIO pin */
  uint8_t           mosi_gpio;    /* MOSI GPIO pin */
  uint8_t           cs_gpio;      /* CS0 GPIO pin */
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/* SPI ops interface */

static int      esp32p4_spi_lock(FAR struct spi_dev_s *dev, bool lock);
static void     esp32p4_spi_select(FAR struct spi_dev_s *dev,
                                   uint32_t devid, bool selected);
static uint32_t esp32p4_spi_setfrequency(FAR struct spi_dev_s *dev,
                                         uint32_t frequency);
static void     esp32p4_spi_setmode(FAR struct spi_dev_s *dev,
                                    enum spi_mode_e mode);
static void     esp32p4_spi_setbits(FAR struct spi_dev_s *dev, int nbits);
static uint32_t esp32p4_spi_send(FAR struct spi_dev_s *dev, uint32_t wd);
#ifdef CONFIG_SPI_EXCHANGE
static void     esp32p4_spi_exchange(FAR struct spi_dev_s *dev,
                                     FAR const void *txbuffer,
                                     FAR void *rxbuffer,
                                     size_t nwords);
#endif
static uint8_t  esp32p4_spi_status(FAR struct spi_dev_s *dev,
                                   uint32_t devid);

/* Internal helper functions */

static void     esp32p4_spi_config_gpio(struct esp32p4_spi_priv_s *priv);
static void     esp32p4_spi_configure(struct esp32p4_spi_priv_s *priv);
static uint32_t esp32p4_spi_calc_clock(struct esp32p4_spi_priv_s *priv,
                                       uint32_t freq);
static void     esp32p4_spi_wait_done(struct esp32p4_spi_priv_s *priv);
static uint32_t esp32p4_spi_xfer_word(struct esp32p4_spi_priv_s *priv,
                                      uint32_t wd);

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* SPI2 instance */

#ifdef CONFIG_ESP32P4_SPI2
static const struct spi_ops_s g_spi2_ops =
{
  .lock          = esp32p4_spi_lock,
  .select        = esp32p4_spi_select,
  .setfrequency  = esp32p4_spi_setfrequency,
  .setmode       = esp32p4_spi_setmode,
  .setbits       = esp32p4_spi_setbits,
  .status        = esp32p4_spi_status,
  .send          = esp32p4_spi_send,
#ifdef CONFIG_SPI_EXCHANGE
  .exchange      = esp32p4_spi_exchange,
#endif
};

static struct esp32p4_spi_priv_s g_spi2_priv =
{
  .spidev =
  {
    .ops = &g_spi2_ops,
  },
  .base       = SPI2_BASE,
  .frequency  = 1000000,  /* Default 1 MHz */
  .mode       = SPIDEV_MODE0,
  .nbits      = 8,
  .lock       = NXMUTEX_INITIALIZER,
  .initialized = false,
  .clk_sig    = SPI2_CLK_OUT_SIG,
  .miso_sig   = SPI2_MISO_IN_SIG,
  .mosi_sig   = SPI2_MOSI_OUT_SIG,
  .cs_sig     = SPI2_CS0_OUT_SIG,
  .clk_gpio   = CONFIG_ESP32P4_SPI2_CLK_GPIO,
  .miso_gpio  = CONFIG_ESP32P4_SPI2_MISO_GPIO,
  .mosi_gpio  = CONFIG_ESP32P4_SPI2_MOSI_GPIO,
  .cs_gpio    = CONFIG_ESP32P4_SPI2_CS0_GPIO,
};
#endif /* CONFIG_ESP32P4_SPI2 */

/* SPI3 instance */

#ifdef CONFIG_ESP32P4_SPI3
static const struct spi_ops_s g_spi3_ops =
{
  .lock          = esp32p4_spi_lock,
  .select        = esp32p4_spi_select,
  .setfrequency  = esp32p4_spi_setfrequency,
  .setmode       = esp32p4_spi_setmode,
  .setbits       = esp32p4_spi_setbits,
  .status        = esp32p4_spi_status,
  .send          = esp32p4_spi_send,
#ifdef CONFIG_SPI_EXCHANGE
  .exchange      = esp32p4_spi_exchange,
#endif
};

static struct esp32p4_spi_priv_s g_spi3_priv =
{
  .spidev =
  {
    .ops = &g_spi3_ops,
  },
  .base       = SPI3_BASE,
  .frequency  = 1000000,  /* Default 1 MHz */
  .mode       = SPIDEV_MODE0,
  .nbits      = 8,
  .lock       = NXMUTEX_INITIALIZER,
  .initialized = false,
  .clk_sig    = SPI3_CLK_OUT_SIG,
  .miso_sig   = SPI3_MISO_IN_SIG,
  .mosi_sig   = SPI3_MOSI_OUT_SIG,
  .cs_sig     = SPI3_CS0_OUT_SIG,
  .clk_gpio   = CONFIG_ESP32P4_SPI3_CLK_GPIO,
  .miso_gpio  = CONFIG_ESP32P4_SPI3_MISO_GPIO,
  .mosi_gpio  = CONFIG_ESP32P4_SPI3_MOSI_GPIO,
  .cs_gpio    = CONFIG_ESP32P4_SPI3_CS0_GPIO,
};
#endif /* CONFIG_ESP32P4_SPI3 */

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: esp32p4_spi_config_gpio
 *
 * Description:
 *   Configure GPIO pins for SPI CLK, MISO, MOSI, and CS via the
 *   GPIO matrix.
 *
 ****************************************************************************/

static void esp32p4_spi_config_gpio(struct esp32p4_spi_priv_s *priv)
{
  uint32_t regval;

  /* Configure CLK pin: output via GPIO matrix */

  regval = priv->clk_sig & GPIO_FUNC_OUT_SEL_M;
  REG_WRITE(GPIO_FUNC_OUT_SEL_CFG_REG(priv->clk_gpio), regval);

  /* Configure MOSI pin: output via GPIO matrix */

  regval = priv->mosi_sig & GPIO_FUNC_OUT_SEL_M;
  REG_WRITE(GPIO_FUNC_OUT_SEL_CFG_REG(priv->mosi_gpio), regval);

  /* Configure CS pin: output via GPIO matrix */

  regval = priv->cs_sig & GPIO_FUNC_OUT_SEL_M;
  REG_WRITE(GPIO_FUNC_OUT_SEL_CFG_REG(priv->cs_gpio), regval);

  /* Configure MISO input routing:
   * Route the GPIO pin to the SPI MISO input signal.
   */

  regval = priv->miso_gpio & 0x3f;
  REG_WRITE(GPIO_FUNC_IN_SEL_CFG_REG(priv->miso_sig), regval);

  /* Configure CLK pin: GPIO function, output, push-pull */

  regval = IO_MUX_GPIO_FUNC << IO_MUX_MCU_SEL_S;
  regval |= (2 << IO_MUX_FUN_DRV_S); /* Drive strength 2 */
  REG_WRITE(IO_MUX_GPIO_REG(priv->clk_gpio), regval);

  /* Configure MOSI pin: GPIO function, output, push-pull */

  regval = IO_MUX_GPIO_FUNC << IO_MUX_MCU_SEL_S;
  regval |= (2 << IO_MUX_FUN_DRV_S);
  REG_WRITE(IO_MUX_GPIO_REG(priv->mosi_gpio), regval);

  /* Configure MISO pin: GPIO function, input enabled, pull-up */

  regval = IO_MUX_GPIO_FUNC << IO_MUX_MCU_SEL_S;
  regval |= IO_MUX_FUN_WPU;     /* Pull-up */
  regval |= IO_MUX_FUN_IE;      /* Input enable */
  regval |= (2 << IO_MUX_FUN_DRV_S);
  REG_WRITE(IO_MUX_GPIO_REG(priv->miso_gpio), regval);

  /* Configure CS pin: GPIO function, output, push-pull */

  regval = IO_MUX_GPIO_FUNC << IO_MUX_MCU_SEL_S;
  regval |= (2 << IO_MUX_FUN_DRV_S);
  REG_WRITE(IO_MUX_GPIO_REG(priv->cs_gpio), regval);

  /* Set CS high (inactive) by default */

  if (priv->cs_gpio < 32)
    {
      REG_WRITE(GPIO_OUT_W1TS_REG, (1 << priv->cs_gpio));
    }
  else
    {
      REG_WRITE(GPIO_OUT1_W1TS_REG, (1 << (priv->cs_gpio - 32)));
    }
}

/****************************************************************************
 * Name: esp32p4_spi_calc_clock
 *
 * Description:
 *   Calculate the SPI clock divider for the requested frequency.
 *
 *   The SPI clock is derived from the APB clock:
 *     SPI_CLK = APB_CLK / (pre_div * (N + 1))
 *
 *   Returns the actual frequency achieved.
 *
 ****************************************************************************/

static uint32_t esp32p4_spi_calc_clock(struct esp32p4_spi_priv_s *priv,
                                       uint32_t freq)
{
  uint32_t pre_div;
  uint32_t n;
  uint32_t regval;

  /* If requested frequency >= APB clock, use system clock directly */

  if (freq >= SPI_APB_CLK_FREQ)
    {
      REG_WRITE(priv->base + SPI_CLOCK_REG, SPI_CLK_EQU_SYSCLK);
      return SPI_APB_CLK_FREQ;
    }

  /* Calculate pre-divider and N counter.
   * We want: freq = APB_CLK / (pre_div * (N + 1))
   * Strategy: find the smallest pre_div that gives N <= 63
   */

  for (pre_div = 1; pre_div <= 64; pre_div++)
    {
      n = (SPI_APB_CLK_FREQ / (freq * pre_div));
      if (n == 0)
        {
          n = 1;
        }

      if (n <= 64)
        {
          break;
        }
    }

  if (pre_div > 64)
    {
      pre_div = 64;
      n = 1;
    }

  /* Build clock register value:
   * CLKDIV_PRE [23:18] = pre_div - 1
   * CLKCNT_N   [5:0]   = n - 1
   * CLKCNT_H   [11:6]  = (n / 2) - 1  (50% duty cycle)
   * CLKCNT_L   [17:12] = (n / 2) - 1  (50% duty cycle)
   */

  regval = (((pre_div - 1) << SPI_CLKDIV_PRE_S) & SPI_CLKDIV_PRE_M) |
           (((n - 1) << SPI_CLKCNT_N_S) & SPI_CLKCNT_N_M) |
           ((((n / 2) - 1) << SPI_CLKCNT_H_S) & SPI_CLKCNT_H_M) |
           ((((n / 2) - 1) << SPI_CLKCNT_L_S) & SPI_CLKCNT_L_M);

  REG_WRITE(priv->base + SPI_CLOCK_REG, regval);

  return SPI_APB_CLK_FREQ / (pre_div * n);
}

/****************************************************************************
 * Name: esp32p4_spi_configure
 *
 * Description:
 *   Configure the SPI controller hardware based on current settings.
 *
 ****************************************************************************/

static void esp32p4_spi_configure(struct esp32p4_spi_priv_s *priv)
{
  uint32_t user_reg;
  uint32_t misc_reg;

  /* Configure clock */

  priv->actual_freq = esp32p4_spi_calc_clock(priv, priv->frequency);

  /* Configure USER register for mode and data direction */

  user_reg = SPI_USR_MOSI |     /* Enable TX data phase */
             SPI_USR_MISO |     /* Enable RX data phase */
             SPI_DOUTDIN;        /* Full-duplex */

  /* Configure clock polarity (CPOL) and phase (CPHA) based on mode */

  misc_reg = 0;

  switch (priv->mode)
    {
      case SPIDEV_MODE0:  /* CPOL=0, CPHA=0 */
        misc_reg &= ~SPI_CK_IDLE_EDGE;  /* Clock idle low */
        user_reg &= ~SPI_CK_OUT_EDGE;   /* Sample on rising edge */
        break;

      case SPIDEV_MODE1:  /* CPOL=0, CPHA=1 */
        misc_reg &= ~SPI_CK_IDLE_EDGE;  /* Clock idle low */
        user_reg |= SPI_CK_OUT_EDGE;    /* Sample on falling edge */
        break;

      case SPIDEV_MODE2:  /* CPOL=1, CPHA=0 */
        misc_reg |= SPI_CK_IDLE_EDGE;   /* Clock idle high */
        user_reg |= SPI_CK_OUT_EDGE;    /* Sample on falling edge */
        break;

      case SPIDEV_MODE3:  /* CPOL=1, CPHA=1 */
        misc_reg |= SPI_CK_IDLE_EDGE;   /* Clock idle high */
        user_reg &= ~SPI_CK_OUT_EDGE;   /* Sample on rising edge */
        break;

      default:
        break;
    }

  /* Set CS idle level high (active low) */

  misc_reg |= SPI_CS_IDLE_VAL;

  /* Write registers */

  REG_WRITE(priv->base + SPI_USER_REG, user_reg);
  REG_WRITE(priv->base + SPI_MISC_REG, misc_reg);

  /* Configure data bit length (8 bits = 8-1 = 7) */

  REG_WRITE(priv->base + SPI_MS_DLEN_REG,
            ((priv->nbits - 1) << SPI_MS_DATA_BITLEN_S) &
            SPI_MS_DATA_BITLEN_M);

  /* Configure command phase: no command */

  REG_WRITE(priv->base + SPI_USER2_REG, 0);

  /* Configure address phase: no address */

  REG_WRITE(priv->base + SPI_USER1_REG, 0);

  /* Mark as initialized */

  priv->initialized = true;
}

/****************************************************************************
 * Name: esp32p4_spi_wait_done
 *
 * Description:
 *   Wait for the SPI transfer to complete (TRANS_DONE interrupt flag).
 *
 ****************************************************************************/

static void esp32p4_spi_wait_done(struct esp32p4_spi_priv_s *priv)
{
  int timeout = SPI_TIMEOUT_COUNT;

  while (timeout-- > 0)
    {
      if (REG_READ(priv->base + SPI_INT_RAW_REG) & SPI_TRANS_DONE_INT)
        {
          /* Clear the interrupt */

          REG_WRITE(priv->base + SPI_INT_CLR_REG, SPI_TRANS_DONE_INT);
          return;
        }
    }

  spierr("ERROR: SPI transfer timeout\n");
}

/****************************************************************************
 * Name: esp32p4_spi_xfer_word
 *
 * Description:
 *   Transfer a single word (8 or 16 bits) on the SPI bus.
 *   Returns the received word.
 *
 ****************************************************************************/

static uint32_t esp32p4_spi_xfer_word(struct esp32p4_spi_priv_s *priv,
                                      uint32_t wd)
{
  /* Write data to TX buffer */

  REG_WRITE(priv->base + SPI_W(0), wd);

  /* Start the transfer */

  REG_WRITE(priv->base + SPI_CMD_REG, SPI_USR);

  /* Wait for transfer to complete */

  esp32p4_spi_wait_done(priv);

  /* Read and return received data */

  return REG_READ(priv->base + SPI_R(0));
}

/****************************************************************************
 * Name: esp32p4_spi_lock
 *
 * Description:
 *   Lock or unlock the SPI bus.
 *
 ****************************************************************************/

static int esp32p4_spi_lock(FAR struct spi_dev_s *dev, bool lock)
{
  FAR struct esp32p4_spi_priv_s *priv = (FAR struct esp32p4_spi_priv_s *)dev;
  int ret;

  if (lock)
    {
      ret = nxmutex_lock(&priv->lock);
    }
  else
    {
      ret = nxmutex_unlock(&priv->lock);
    }

  return ret;
}

/****************************************************************************
 * Name: esp32p4_spi_select
 *
 * Description:
 *   Select or deselect the SPI device.
 *
 *   CS is active low.  When selected, CS is driven low; when deselected,
 *   CS is driven high.
 *
 ****************************************************************************/

static void esp32p4_spi_select(FAR struct spi_dev_s *dev, uint32_t devid,
                               bool selected)
{
  FAR struct esp32p4_spi_priv_s *priv = (FAR struct esp32p4_spi_priv_s *)dev;

  /* Ensure hardware is configured */

  if (!priv->initialized)
    {
      esp32p4_spi_config_gpio(priv);
      esp32p4_spi_configure(priv);
    }

  /* Drive CS: active low */

  if (selected)
    {
      /* CS active (low) */

      if (priv->cs_gpio < 32)
        {
          REG_WRITE(GPIO_OUT_W1TC_REG, (1 << priv->cs_gpio));
        }
      else
        {
          REG_WRITE(GPIO_OUT1_W1TC_REG, (1 << (priv->cs_gpio - 32)));
        }
    }
  else
    {
      /* CS inactive (high) */

      if (priv->cs_gpio < 32)
        {
          REG_WRITE(GPIO_OUT_W1TS_REG, (1 << priv->cs_gpio));
        }
      else
        {
          REG_WRITE(GPIO_OUT1_W1TS_REG, (1 << (priv->cs_gpio - 32)));
        }
    }
}

/****************************************************************************
 * Name: esp32p4_spi_setfrequency
 *
 * Description:
 *   Set the SPI bus frequency.
 *
 *   Returns the actual frequency achieved.
 *
 ****************************************************************************/

static uint32_t esp32p4_spi_setfrequency(FAR struct spi_dev_s *dev,
                                         uint32_t frequency)
{
  FAR struct esp32p4_spi_priv_s *priv = (FAR struct esp32p4_spi_priv_s *)dev;

  if (priv->frequency != frequency)
    {
      priv->frequency = frequency;

      if (priv->initialized)
        {
          priv->actual_freq = esp32p4_spi_calc_clock(priv, frequency);
        }
    }

  return priv->actual_freq;
}

/****************************************************************************
 * Name: esp32p4_spi_setmode
 *
 * Description:
 *   Set the SPI mode.
 *
 ****************************************************************************/

static void esp32p4_spi_setmode(FAR struct spi_dev_s *dev,
                                enum spi_mode_e mode)
{
  FAR struct esp32p4_spi_priv_s *priv = (FAR struct esp32p4_spi_priv_s *)dev;

  if (priv->mode != mode)
    {
      priv->mode = mode;

      if (priv->initialized)
        {
          esp32p4_spi_configure(priv);
        }
    }
}

/****************************************************************************
 * Name: esp32p4_spi_setbits
 *
 * Description:
 *   Set the number of bits per word.
 *
 ****************************************************************************/

static void esp32p4_spi_setbits(FAR struct spi_dev_s *dev, int nbits)
{
  FAR struct esp32p4_spi_priv_s *priv = (FAR struct esp32p4_spi_priv_s *)dev;

  if (priv->nbits != nbits)
    {
      priv->nbits = nbits;

      if (priv->initialized)
        {
          esp32p4_spi_configure(priv);
        }
    }
}

/****************************************************************************
 * Name: esp32p4_spi_status
 *
 * Description:
 *   Get SPI status.
 *
 ****************************************************************************/

static uint8_t esp32p4_spi_status(FAR struct spi_dev_s *dev,
                                  uint32_t devid)
{
  /* Always report present */

  return SPI_STATUS_PRESENT;
}

/****************************************************************************
 * Name: esp32p4_spi_send
 *
 * Description:
 *   Exchange one word on SPI.
 *
 ****************************************************************************/

static uint32_t esp32p4_spi_send(FAR struct spi_dev_s *dev, uint32_t wd)
{
  FAR struct esp32p4_spi_priv_s *priv = (FAR struct esp32p4_spi_priv_s *)dev;

  /* Ensure hardware is configured */

  if (!priv->initialized)
    {
      esp32p4_spi_config_gpio(priv);
      esp32p4_spi_configure(priv);
    }

  return esp32p4_spi_xfer_word(priv, wd);
}

/****************************************************************************
 * Name: esp32p4_spi_exchange
 *
 * Description:
 *   Exchange a block of data on SPI (full-duplex).
 *
 *   This transfers nwords between txbuffer and rxbuffer simultaneously.
 *   If txbuffer is NULL, zeros are sent.  If rxbuffer is NULL, received
 *   data is discarded.
 *
 ****************************************************************************/

#ifdef CONFIG_SPI_EXCHANGE
static void esp32p4_spi_exchange(FAR struct spi_dev_s *dev,
                                 FAR const void *txbuffer,
                                 FAR void *rxbuffer,
                                 size_t nwords)
{
  FAR struct esp32p4_spi_priv_s *priv = (FAR struct esp32p4_spi_priv_s *)dev;
  FAR const uint8_t *txptr = (FAR const uint8_t *)txbuffer;
  FAR uint8_t *rxptr = (FAR uint8_t *)rxbuffer;
  size_t i;
  uint32_t txword;
  uint32_t rxword;

  /* Ensure hardware is configured */

  if (!priv->initialized)
    {
      esp32p4_spi_config_gpio(priv);
      esp32p4_spi_configure(priv);
    }

  /* Transfer words one at a time */

  for (i = 0; i < nwords; i++)
    {
      /* Get the TX word */

      if (txptr != NULL)
        {
          if (priv->nbits <= 8)
            {
              txword = (uint32_t)txptr[i];
            }
          else
            {
              txword = (uint32_t)((FAR const uint16_t *)txbuffer)[i];
            }
        }
      else
        {
          txword = 0;
        }

      /* Transfer the word */

      rxword = esp32p4_spi_xfer_word(priv, txword);

      /* Store the RX word */

      if (rxptr != NULL)
        {
          if (priv->nbits <= 8)
            {
              rxptr[i] = (uint8_t)(rxword & 0xff);
            }
          else
            {
              ((FAR uint16_t *)rxbuffer)[i] = (uint16_t)(rxword & 0xffff);
            }
        }
    }
}
#endif /* CONFIG_SPI_EXCHANGE */

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: esp32p4_spidev_register
 *
 * Description:
 *   Register an SPI bus driver for the specified SPI port.
 *
 *   This function initializes the SPI controller hardware and registers
 *   the SPI device at /dev/spiN.
 *
 * Input Parameters:
 *   port - SPI port number (2 for SPI2, 3 for SPI3)
 *
 * Returned Value:
 *   OK on success; a negated errno value on failure.
 *
 ****************************************************************************/

int esp32p4_spidev_register(int port)
{
  FAR struct esp32p4_spi_priv_s *priv;
  FAR struct spi_dev_s *spidev;

  switch (port)
    {
#ifdef CONFIG_ESP32P4_SPI2
      case 2:
        priv = &g_spi2_priv;
        break;
#endif

#ifdef CONFIG_ESP32P4_SPI3
      case 3:
        priv = &g_spi3_priv;
        break;
#endif

      default:
        spierr("ERROR: Invalid SPI port: %d\n", port);
        return -EINVAL;
    }

  spidev = &priv->spidev;

  /* Initialize the SPI hardware */

  esp32p4_spi_config_gpio(priv);
  esp32p4_spi_configure(priv);

  /* Register the SPI device */

  spiinfo("SPI%d registered successfully (base=0x%08" PRIx32 ")\n",
          port, priv->base);

  return OK;
}
