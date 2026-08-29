/****************************************************************************
 * vendor/espressif/chips/esp32p4/esp32p4_i2c.c
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
 * ESP32-P4 I2C Bus Driver
 *
 * This driver implements the NuttX i2c_master_s interface for the ESP32-P4
 * HP I2C peripherals (I2C0 and I2C1).  The implementation uses the
 * hardware command-based interface with polling for simplicity and
 * reliability.
 *
 * Key features:
 *   - I2C master mode only
 *   - Standard mode (100 kHz) and Fast mode (400 kHz) support
 *   - GPIO matrix pin routing for SCL/SDA signals
 *   - Polling-based transfer (no interrupt overhead)
 *   - Timeout detection for bus hang recovery
 *   - Multi-message transfer with repeated start support
 *
 * ESP-IDF reference:
 *   - examples/peripherals/i2c/i2c_basic
 *   - components/esp_driver_i2c/i2c_master.c
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
#include <nuttx/i2c/i2c_master.h>

#include "hardware/esp32p4_i2c.h"
#include "hardware/esp32p4_gpio.h"
#include "hardware/esp32p4_soc.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* I2C timeout in busy-wait iterations.
 * At 80 MHz APB clock, 100000 iterations is ~12.5ms.
 */

#define I2C_TIMEOUT_COUNT           100000

/* Default I2C pins if not configured via Kconfig */

#ifndef CONFIG_ESP32P4_I2C0_SCL_GPIO
#  define CONFIG_ESP32P4_I2C0_SCL_GPIO  8
#endif

#ifndef CONFIG_ESP32P4_I2C0_SDA_GPIO
#  define CONFIG_ESP32P4_I2C0_SDA_GPIO  9
#endif

#ifndef CONFIG_ESP32P4_I2C1_SCL_GPIO
#  define CONFIG_ESP32P4_I2C1_SCL_GPIO  10
#endif

#ifndef CONFIG_ESP32P4_I2C1_SDA_GPIO
#  define CONFIG_ESP32P4_I2C1_SDA_GPIO  11
#endif

/****************************************************************************
 * Private Types
 ****************************************************************************/

/* I2C bus private data structure */

struct esp32p4_i2c_priv_s
{
  const struct i2c_ops_s *ops;    /* Standard I2C operations (must be first) */
  uint32_t                base;   /* I2C register base address */
  int                     irq;    /* Interrupt number */
  uint32_t                freq;   /* Current I2C frequency */
  uint8_t                 scl_sig; /* GPIO matrix SCL signal */
  uint8_t                 sda_sig; /* GPIO matrix SDA signal */
  uint8_t                 scl_gpio; /* SCL GPIO pin */
  uint8_t                 sda_gpio; /* SDA GPIO pin */
  mutex_t                 lock;   /* Mutual exclusion mutex */
  int                     refs;   /* Reference count */
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int  esp32p4_i2c_transfer(FAR struct i2c_master_s *dev,
                                 FAR struct i2c_msg_s *msgs, int count);
static int  esp32p4_i2c_reset(FAR struct i2c_master_s *dev);
static int  esp32p4_i2c_setup(FAR struct i2c_master_s *dev);
static int  esp32p4_i2c_shutdown(FAR struct i2c_master_s *dev);

/* Internal helper functions */

static void esp32p4_i2c_config_gpio(struct esp32p4_i2c_priv_s *priv);
static void esp32p4_i2c_set_clock(struct esp32p4_i2c_priv_s *priv,
                                  uint32_t freq);
static int  esp32p4_i2c_write_cmd(struct esp32p4_i2c_priv_s *priv,
                                  uint8_t cmd_idx, uint8_t op_code,
                                  uint8_t byte_num, bool ack_check,
                                  bool ack_exp, bool ack_val);
static int  esp32p4_i2c_poll_cmd_done(struct esp32p4_i2c_priv_s *priv,
                                      uint8_t cmd_idx);
static int  esp32p4_i2c_wait_bus_idle(struct esp32p4_i2c_priv_s *priv);
static int  esp32p4_i2c_poll_trans_done(struct esp32p4_i2c_priv_s *priv);
static void esp32p4_i2c_reset_fifo(struct esp32p4_i2c_priv_s *priv);

/* Transfer helpers */

static int  esp32p4_i2c_write_bytes(struct esp32p4_i2c_priv_s *priv,
                                    FAR const uint8_t *data, uint8_t len,
                                    bool ack_check, bool last);
static int  esp32p4_i2c_read_bytes(struct esp32p4_i2c_priv_s *priv,
                                   FAR uint8_t *data, uint8_t len,
                                   bool send_nack);
static int  esp32p4_i2c_do_write(struct esp32p4_i2c_priv_s *priv,
                                 FAR struct i2c_msg_s *msg);
static int  esp32p4_i2c_do_read(struct esp32p4_i2c_priv_s *priv,
                                FAR struct i2c_msg_s *msg);
static int  esp32p4_i2c_do_writeread(struct esp32p4_i2c_priv_s *priv,
                                     FAR struct i2c_msg_s *wmsg,
                                     FAR struct i2c_msg_s *rmsg);

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* I2C0 instance */

#ifdef CONFIG_ESP32P4_I2C0
static const struct i2c_ops_s g_i2c0_ops =
{
  .transfer = esp32p4_i2c_transfer,
#ifdef CONFIG_I2C_RESET
  .reset    = esp32p4_i2c_reset,
#endif
  .setup    = esp32p4_i2c_setup,
  .shutdown = esp32p4_i2c_shutdown,
};

static struct esp32p4_i2c_priv_s g_i2c0_priv =
{
  .ops      = &g_i2c0_ops,
  .base     = I2C0_BASE,
  .irq      = ESP32P4_IRQ_I2C0,
  .freq     = I2C_SPEED_STANDARD,
  .scl_sig  = I2C0_SCL_OUT_SIG,
  .sda_sig  = I2C0_SDA_OUT_SIG,
  .scl_gpio = CONFIG_ESP32P4_I2C0_SCL_GPIO,
  .sda_gpio = CONFIG_ESP32P4_I2C0_SDA_GPIO,
  .lock     = NXMUTEX_INITIALIZER,
  .refs     = 0,
};
#endif /* CONFIG_ESP32P4_I2C0 */

/* I2C1 instance */

#ifdef CONFIG_ESP32P4_I2C1
static const struct i2c_ops_s g_i2c1_ops =
{
  .transfer = esp32p4_i2c_transfer,
#ifdef CONFIG_I2C_RESET
  .reset    = esp32p4_i2c_reset,
#endif
  .setup    = esp32p4_i2c_setup,
  .shutdown = esp32p4_i2c_shutdown,
};

static struct esp32p4_i2c_priv_s g_i2c1_priv =
{
  .ops      = &g_i2c1_ops,
  .base     = I2C1_BASE,
  .irq      = ESP32P4_IRQ_I2C1,
  .freq     = I2C_SPEED_STANDARD,
  .scl_sig  = I2C1_SCL_OUT_SIG,
  .sda_sig  = I2C1_SDA_OUT_SIG,
  .scl_gpio = CONFIG_ESP32P4_I2C1_SCL_GPIO,
  .sda_gpio = CONFIG_ESP32P4_I2C1_SDA_GPIO,
  .lock     = NXMUTEX_INITIALIZER,
  .refs     = 0,
};
#endif /* CONFIG_ESP32P4_I2C1 */

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: esp32p4_i2c_config_gpio
 *
 * Description:
 *   Configure GPIO pins for I2C SCL and SDA via the GPIO matrix.
 *
 *   The ESP32-P4 uses a GPIO matrix that allows any peripheral signal
 *   to be routed to any GPIO pin.  This function sets up the pin mux
 *   for the I2C SCL and SDA signals.
 *
 *   Both SCL and SDA are configured as open-drain outputs with pull-up
 *   enabled, which is the standard I2C bus configuration.
 *
 ****************************************************************************/

static void esp32p4_i2c_config_gpio(struct esp32p4_i2c_priv_s *priv)
{
  uint32_t regval;

  /* Configure SCL pin:
   * - Route I2C SCL output signal to GPIO via GPIO matrix
   * - Configure as open-drain output with pull-up
   */

  regval = priv->scl_sig & GPIO_FUNC_OUT_SEL_M;
  REG_WRITE(GPIO_FUNC_OUT_SEL_CFG_REG(priv->scl_gpio), regval);

  /* Configure SDA pin:
   * - Route I2C SDA output signal to GPIO via GPIO matrix
   * - Configure as open-drain output with pull-up
   */

  regval = priv->sda_sig & GPIO_FUNC_OUT_SEL_M;
  REG_WRITE(GPIO_FUNC_OUT_SEL_CFG_REG(priv->sda_gpio), regval);

  /* Configure SCL input routing:
   * Route the GPIO pin to the I2C SCL input signal.
   */

  regval = priv->scl_gpio & 0x3f;
  REG_WRITE(GPIO_FUNC_IN_SEL_CFG_REG(priv->scl_sig), regval);

  /* Configure SDA input routing:
   * Route the GPIO pin to the I2C SDA input signal.
   */

  regval = priv->sda_gpio & 0x3f;
  REG_WRITE(GPIO_FUNC_IN_SEL_CFG_REG(priv->sda_sig), regval);

  /* Configure SCL pin: open-drain with pull-up, input enabled */

  regval = IO_MUX_GPIO_FUNC << IO_MUX_MCU_SEL_S;
  regval |= IO_MUX_FUN_WPU;     /* Pull-up */
  regval |= IO_MUX_FUN_IE;      /* Input enable */
  regval |= (2 << IO_MUX_FUN_DRV_S); /* Drive strength 2 */
  REG_WRITE(IO_MUX_GPIO_REG(priv->scl_gpio), regval);

  /* Configure SDA pin: open-drain with pull-up, input enabled */

  regval = IO_MUX_GPIO_FUNC << IO_MUX_MCU_SEL_S;
  regval |= IO_MUX_FUN_WPU;     /* Pull-up */
  regval |= IO_MUX_FUN_IE;      /* Input enable */
  regval |= (2 << IO_MUX_FUN_DRV_S); /* Drive strength 2 */
  REG_WRITE(IO_MUX_GPIO_REG(priv->sda_gpio), regval);

  /* Enable open-drain mode for both pins */

  REG_SET_BIT(GPIO_PIN_REG(priv->scl_gpio), GPIO_PIN_PAD_DRIVER_BIT);
  REG_SET_BIT(GPIO_PIN_REG(priv->sda_gpio), GPIO_PIN_PAD_DRIVER_BIT);
}

/****************************************************************************
 * Name: esp32p4_i2c_set_clock
 *
 * Description:
 *   Configure the I2C clock frequency.
 *
 *   The I2C SCL frequency is determined by:
 *     SCL_FREQ = APB_CLK / (scl_high_period + scl_low_period)
 *
 *   For 100 kHz (standard mode): 80 MHz / (400 + 400) = 100 kHz
 *   For 400 kHz (fast mode):     80 MHz / (100 + 100) = 400 kHz
 *
 ****************************************************************************/

static void esp32p4_i2c_set_clock(struct esp32p4_i2c_priv_s *priv,
                                  uint32_t freq)
{
  uint32_t high_period;
  uint32_t low_period;

  /* Select timing based on requested frequency */

  if (freq >= I2C_SPEED_FAST)
    {
      high_period = I2C_SCL_HIGH_PERIOD_400K;
      low_period  = I2C_SCL_LOW_PERIOD_400K;
    }
  else
    {
      high_period = I2C_SCL_HIGH_PERIOD_100K;
      low_period  = I2C_SCL_LOW_PERIOD_100K;
    }

  /* Set SCL high period */

  REG_WRITE(priv->base + I2C_SCL_HIGH_PERIOD_REG,
            (high_period & 0x1ff) |
            ((high_period & 0x1ff) << 8));  /* wait_high_period */

  /* Set SCL low period */

  REG_WRITE(priv->base + I2C_SCL_LOW_PERIOD_REG,
            low_period & 0x1ff);

  /* Set SDA hold time */

  REG_WRITE(priv->base + I2C_SDA_HOLD_REG,
            I2C_SDA_HOLD_TIME & 0x1ff);

  /* Set SDA sample time */

  REG_WRITE(priv->base + I2C_SDA_SAMPLE_REG,
            I2C_SDA_SAMPLE_TIME & 0x1ff);

  /* Set start/stop timing */

  REG_WRITE(priv->base + I2C_SCL_START_HOLD_REG,
            I2C_SCL_START_HOLD_TIME & 0x1ff);
  REG_WRITE(priv->base + I2C_SCL_RSTART_SETUP_REG,
            I2C_SCL_RSTART_SETUP_TIME & 0x1ff);
  REG_WRITE(priv->base + I2C_SCL_STOP_HOLD_REG,
            I2C_SCL_STOP_HOLD_TIME & 0x1ff);
  REG_WRITE(priv->base + I2C_SCL_STOP_SETUP_REG,
            I2C_SCL_STOP_SETUP_TIME & 0x1ff);

  /* Configure glitch filter */

  REG_WRITE(priv->base + I2C_FILTER_CFG_REG,
            (I2C_FILTER_CYCLES << I2C_SCL_FILTER_THRES_S) |
            (I2C_FILTER_CYCLES << I2C_SDA_FILTER_THRES_S) |
            I2C_SCL_FILTER_EN | I2C_SDA_FILTER_EN);

  /* Configure timeout */

  REG_WRITE(priv->base + I2C_TO_REG,
            (I2C_TIMEOUT_VALUE & I2C_TIME_OUT_VALUE_M) |
            I2C_TIME_OUT_EN);

  /* Update configuration */

  REG_SET_BIT(priv->base + I2C_CTR_REG, I2C_CONF_UPGATE);

  priv->freq = freq;
}

/****************************************************************************
 * Name: esp32p4_i2c_write_cmd
 *
 * Description:
 *   Write a command to one of the 8 I2C command registers.
 *
 *   Command encoding:
 *     [2:0]   op_code (RSTART=0, WRITE=1, READ=2, STOP=3, END=4)
 *     [10:3]  byte_num (number of bytes)
 *     [11]    ack_check_en
 *     [12]    ack_exp
 *     [13]    ack_val
 *
 ****************************************************************************/

static int esp32p4_i2c_write_cmd(struct esp32p4_i2c_priv_s *priv,
                                 uint8_t cmd_idx, uint8_t op_code,
                                 uint8_t byte_num, bool ack_check,
                                 bool ack_exp, bool ack_val)
{
  uint32_t cmd;

  if (cmd_idx >= I2C_CMD_REG_NUM)
    {
      return -EINVAL;
    }

  cmd = (op_code & I2C_CMD_OP_CODE_M) |
        ((byte_num << I2C_CMD_BYTE_NUM_S) & I2C_CMD_BYTE_NUM_M);

  if (ack_check)
    {
      cmd |= I2C_CMD_ACK_CHECK_EN;
    }

  if (ack_exp)
    {
      cmd |= I2C_CMD_ACK_EXP;
    }

  if (ack_val)
    {
      cmd |= I2C_CMD_ACK_VAL;
    }

  REG_WRITE(priv->base + I2C_COMD_REG(cmd_idx), cmd);

  return OK;
}

/****************************************************************************
 * Name: esp32p4_i2c_poll_cmd_done
 *
 * Description:
 *   Wait for a command register to complete (cmd_done bit set).
 *
 ****************************************************************************/

static int esp32p4_i2c_poll_cmd_done(struct esp32p4_i2c_priv_s *priv,
                                     uint8_t cmd_idx)
{
  int timeout = I2C_TIMEOUT_COUNT;

  while (timeout-- > 0)
    {
      if (REG_READ(priv->base + I2C_COMD_REG(cmd_idx)) & I2C_CMD_DONE)
        {
          return OK;
        }
    }

  i2cerr("ERROR: I2C cmd%d timeout\n", cmd_idx);
  return -ETIMEDOUT;
}

/****************************************************************************
 * Name: esp32p4_i2c_wait_bus_idle
 *
 * Description:
 *   Wait until the I2C bus is idle (not busy).
 *
 ****************************************************************************/

static int esp32p4_i2c_wait_bus_idle(struct esp32p4_i2c_priv_s *priv)
{
  int timeout = I2C_TIMEOUT_COUNT;

  while (timeout-- > 0)
    {
      if (!(REG_READ(priv->base + I2C_SR_REG) & I2C_BUS_BUSY))
        {
          return OK;
        }
    }

  i2cerr("ERROR: I2C bus busy timeout\n");
  return -ETIMEDOUT;
}

/****************************************************************************
 * Name: esp32p4_i2c_poll_trans_done
 *
 * Description:
 *   Wait for the TRANS_COMPLETE interrupt flag, which indicates that
 *   the entire I2C transaction (including STOP) has completed.
 *
 ****************************************************************************/

static int esp32p4_i2c_poll_trans_done(struct esp32p4_i2c_priv_s *priv)
{
  int timeout = I2C_TIMEOUT_COUNT;

  while (timeout-- > 0)
    {
      if (REG_READ(priv->base + I2C_INT_RAW_REG) & I2C_TRANS_COMPLETE_INT)
        {
          /* Clear the interrupt */

          REG_WRITE(priv->base + I2C_INT_CLR_REG, I2C_TRANS_COMPLETE_INT);
          return OK;
        }
    }

  i2cerr("ERROR: I2C trans_done timeout\n");
  return -ETIMEDOUT;
}

/****************************************************************************
 * Name: esp32p4_i2c_reset_fifo
 *
 * Description:
 *   Reset both TX and RX FIFOs.
 *
 ****************************************************************************/

static void esp32p4_i2c_reset_fifo(struct esp32p4_i2c_priv_s *priv)
{
  REG_SET_BIT(priv->base + I2C_FIFO_CONF_REG,
              I2C_RX_FIFO_RST | I2C_TX_FIFO_RST);
}

/****************************************************************************
 * Name: esp32p4_i2c_write_bytes
 *
 * Description:
 *   Write bytes to the I2C TX FIFO and execute the WRITE command.
 *
 ****************************************************************************/

static int esp32p4_i2c_write_bytes(struct esp32p4_i2c_priv_s *priv,
                                   FAR const uint8_t *data, uint8_t len,
                                   bool ack_check, bool last)
{
  int ret;
  int i;

  if (len == 0)
    {
      return OK;
    }

  /* Reset FIFO before writing */

  esp32p4_i2c_reset_fifo(priv);

  /* Write data to TX FIFO */

  for (i = 0; i < len; i++)
    {
      REG_WRITE(priv->base + I2C_TXFIFO_MEM, data[i]);
    }

  /* Set up command 0: WRITE with ack_check */

  ret = esp32p4_i2c_write_cmd(priv, 0, I2C_CMD_OP_WRITE, len,
                              ack_check, false, false);
  if (ret < 0)
    {
      return ret;
    }

  /* Set up command 1: END */

  ret = esp32p4_i2c_write_cmd(priv, 1, I2C_CMD_OP_END, 0,
                              false, false, false);
  if (ret < 0)
    {
      return ret;
    }

  /* Start the transaction */

  REG_SET_BIT(priv->base + I2C_CTR_REG, I2C_TRANS_START);

  /* Wait for command 0 to complete */

  ret = esp32p4_i2c_poll_cmd_done(priv, 0);
  if (ret < 0)
    {
      return ret;
    }

  /* Check for NACK if ack_check was enabled */

  if (ack_check &&
      (REG_READ(priv->base + I2C_SR_REG) & I2C_RESP_REC))
    {
      i2cerr("ERROR: I2C NACK received during write\n");
      return -EIO;
    }

  /* Wait for transaction complete if this is the last write */

  if (last)
    {
      ret = esp32p4_i2c_poll_trans_done(priv);
      if (ret < 0)
        {
          return ret;
        }
    }

  return OK;
}

/****************************************************************************
 * Name: esp32p4_i2c_read_bytes
 *
 * Description:
 *   Execute the READ command and read bytes from the I2C RX FIFO.
 *
 ****************************************************************************/

static int esp32p4_i2c_read_bytes(struct esp32p4_i2c_priv_s *priv,
                                  FAR uint8_t *data, uint8_t len,
                                  bool send_nack)
{
  int ret;
  int i;
  uint32_t regval;

  if (len == 0)
    {
      return OK;
    }

  /* Reset FIFO before reading */

  esp32p4_i2c_reset_fifo(priv);

  /* Set up command 0: READ with NACK on last byte */

  ret = esp32p4_i2c_write_cmd(priv, 0, I2C_CMD_OP_READ, len,
                              false, false, send_nack);
  if (ret < 0)
    {
      return ret;
    }

  /* Set up command 1: END */

  ret = esp32p4_i2c_write_cmd(priv, 1, I2C_CMD_OP_END, 0,
                              false, false, false);
  if (ret < 0)
    {
      return ret;
    }

  /* Start the transaction */

  REG_SET_BIT(priv->base + I2C_CTR_REG, I2C_TRANS_START);

  /* Wait for command 0 to complete */

  ret = esp32p4_i2c_poll_cmd_done(priv, 0);
  if (ret < 0)
    {
      return ret;
    }

  /* Wait for transaction complete */

  ret = esp32p4_i2c_poll_trans_done(priv);
  if (ret < 0)
    {
      return ret;
    }

  /* Read data from RX FIFO */

  for (i = 0; i < len; i++)
    {
      regval = REG_READ(priv->base + I2C_DATA_REG);
      data[i] = (uint8_t)(regval & 0xff);
    }

  return OK;
}

/****************************************************************************
 * Name: esp32p4_i2c_do_write
 *
 * Description:
 *   Perform a complete I2C write transaction:
 *     START -> WRITE(addr+W) -> WRITE(data) -> STOP
 *
 ****************************************************************************/

static int esp32p4_i2c_do_write(struct esp32p4_i2c_priv_s *priv,
                                FAR struct i2c_msg_s *msg)
{
  int ret;
  uint8_t addr_byte;
  int remaining;
  int chunk;
  int offset = 0;
  bool nostop = (msg->flags & I2C_M_NOSTOP) != 0;

  /* Wait for bus to be idle */

  ret = esp32p4_i2c_wait_bus_idle(priv);
  if (ret < 0)
    {
      return ret;
    }

  /* Clear all pending interrupts */

  REG_WRITE(priv->base + I2C_INT_CLR_REG, 0x1ffff);

  /* Build address byte: 7-bit address + Write bit (0) */

  addr_byte = (uint8_t)(msg->addr << 1);

  /* Reset FIFO */

  esp32p4_i2c_reset_fifo(priv);

  /* Set up command sequence:
   *   CMD0: START
   *   CMD1: WRITE(addr byte, ack_check)
   *   CMD2: WRITE(data bytes, ack_check)
   *   CMD3: STOP (if not NOSTOP)
   *   CMD4: END
   */

  /* CMD0: START */

  ret = esp32p4_i2c_write_cmd(priv, 0, I2C_CMD_OP_RSTART, 0,
                              false, false, false);
  if (ret < 0)
    {
      return ret;
    }

  /* CMD1: WRITE address byte with ACK check */

  ret = esp32p4_i2c_write_cmd(priv, 1, I2C_CMD_OP_WRITE, 1,
                              true, false, false);
  if (ret < 0)
    {
      return ret;
    }

  /* Write address byte to FIFO */

  REG_WRITE(priv->base + I2C_TXFIFO_MEM, addr_byte);

  /* Write data bytes in chunks of up to I2C_FIFO_LEN */

  remaining = msg->length;
  while (remaining > 0)
    {
      chunk = (remaining > I2C_FIFO_LEN) ? I2C_FIFO_LEN : remaining;

      /* Reset FIFO for this chunk */

      if (offset > 0)
        {
          esp32p4_i2c_reset_fifo(priv);
        }

      /* Write data bytes to FIFO */

      {
        int i;
        for (i = 0; i < chunk; i++)
          {
            REG_WRITE(priv->base + I2C_TXFIFO_MEM,
                      msg->buffer[offset + i]);
          }
      }

      /* CMD2: WRITE data with ACK check */

      ret = esp32p4_i2c_write_cmd(priv, 2, I2C_CMD_OP_WRITE, chunk,
                                  true, false, false);
      if (ret < 0)
        {
          return ret;
        }

      /* CMD3: STOP (only on last chunk and if not NOSTOP) */

      if (remaining <= chunk && !nostop)
        {
          ret = esp32p4_i2c_write_cmd(priv, 3, I2C_CMD_OP_STOP, 0,
                                      false, false, false);
          if (ret < 0)
            {
              return ret;
            }
        }

      /* CMD4: END */

      ret = esp32p4_i2c_write_cmd(priv, 4, I2C_CMD_OP_END, 0,
                                  false, false, false);
      if (ret < 0)
        {
          return ret;
        }

      /* Start the transaction */

      REG_SET_BIT(priv->base + I2C_CTR_REG, I2C_TRANS_START);

      /* Wait for CMD1 (address) to complete */

      ret = esp32p4_i2c_poll_cmd_done(priv, 1);
      if (ret < 0)
        {
          return ret;
        }

      /* Check for NACK on address */

      if (REG_READ(priv->base + I2C_SR_REG) & I2C_RESP_REC)
        {
          i2cerr("ERROR: I2C NACK on address 0x%02x\n", msg->addr);
          return -EIO;
        }

      /* Wait for CMD2 (data) to complete */

      ret = esp32p4_i2c_poll_cmd_done(priv, 2);
      if (ret < 0)
        {
          return ret;
        }

      /* Check for NACK on data */

      if (REG_READ(priv->base + I2C_SR_REG) & I2C_RESP_REC)
        {
          i2cerr("ERROR: I2C NACK on data\n");
          return -EIO;
        }

      /* Wait for transaction complete */

      ret = esp32p4_i2c_poll_trans_done(priv);
      if (ret < 0)
        {
          return ret;
        }

      offset += chunk;
      remaining -= chunk;
    }

  return OK;
}

/****************************************************************************
 * Name: esp32p4_i2c_do_read
 *
 * Description:
 *   Perform a complete I2C read transaction:
 *     START -> WRITE(addr+R) -> READ(data) -> STOP
 *
 ****************************************************************************/

static int esp32p4_i2c_do_read(struct esp32p4_i2c_priv_s *priv,
                               FAR struct i2c_msg_s *msg)
{
  int ret;
  uint8_t addr_byte;
  int remaining;
  int chunk;
  int offset = 0;
  bool nostop = (msg->flags & I2C_M_NOSTOP) != 0;

  /* Wait for bus to be idle */

  ret = esp32p4_i2c_wait_bus_idle(priv);
  if (ret < 0)
    {
      return ret;
    }

  /* Clear all pending interrupts */

  REG_WRITE(priv->base + I2C_INT_CLR_REG, 0x1ffff);

  /* Build address byte: 7-bit address + Read bit (1) */

  addr_byte = (uint8_t)((msg->addr << 1) | 1);

  /* Reset FIFO */

  esp32p4_i2c_reset_fifo(priv);

  /* Set up command sequence:
   *   CMD0: START
   *   CMD1: WRITE(addr byte, ack_check)
   *   CMD2: READ(data bytes) with NACK on last byte
   *   CMD3: STOP (if not NOSTOP)
   *   CMD4: END
   */

  /* CMD0: START */

  ret = esp32p4_i2c_write_cmd(priv, 0, I2C_CMD_OP_RSTART, 0,
                              false, false, false);
  if (ret < 0)
    {
      return ret;
    }

  /* CMD1: WRITE address byte with ACK check */

  ret = esp32p4_i2c_write_cmd(priv, 1, I2C_CMD_OP_WRITE, 1,
                              true, false, false);
  if (ret < 0)
    {
      return ret;
    }

  /* Write address byte to FIFO */

  REG_WRITE(priv->base + I2C_TXFIFO_MEM, addr_byte);

  /* Read data in chunks of up to I2C_FIFO_LEN */

  remaining = msg->length;
  while (remaining > 0)
    {
      chunk = (remaining > I2C_FIFO_LEN) ? I2C_FIFO_LEN : remaining;

      /* Reset FIFO for this chunk */

      esp32p4_i2c_reset_fifo(priv);

      /* CMD2: READ data, NACK on last byte of last chunk */

      ret = esp32p4_i2c_write_cmd(priv, 2, I2C_CMD_OP_READ, chunk,
                                  false, false,
                                  (remaining <= chunk) ? true : false);
      if (ret < 0)
        {
          return ret;
        }

      /* CMD3: STOP (only on last chunk and if not NOSTOP) */

      if (remaining <= chunk && !nostop)
        {
          ret = esp32p4_i2c_write_cmd(priv, 3, I2C_CMD_OP_STOP, 0,
                                      false, false, false);
          if (ret < 0)
            {
              return ret;
            }
        }

      /* CMD4: END */

      ret = esp32p4_i2c_write_cmd(priv, 4, I2C_CMD_OP_END, 0,
                                  false, false, false);
      if (ret < 0)
        {
          return ret;
        }

      /* Start the transaction */

      REG_SET_BIT(priv->base + I2C_CTR_REG, I2C_TRANS_START);

      /* Wait for CMD1 (address) to complete */

      ret = esp32p4_i2c_poll_cmd_done(priv, 1);
      if (ret < 0)
        {
          return ret;
        }

      /* Check for NACK on address */

      if (REG_READ(priv->base + I2C_SR_REG) & I2C_RESP_REC)
        {
          i2cerr("ERROR: I2C NACK on address 0x%02x\n", msg->addr);
          return -EIO;
        }

      /* Wait for CMD2 (data) to complete */

      ret = esp32p4_i2c_poll_cmd_done(priv, 2);
      if (ret < 0)
        {
          return ret;
        }

      /* Wait for transaction complete */

      ret = esp32p4_i2c_poll_trans_done(priv);
      if (ret < 0)
        {
          return ret;
        }

      /* Read data from RX FIFO */

      {
        int i;
        for (i = 0; i < chunk; i++)
          {
            msg->buffer[offset + i] =
              (uint8_t)(REG_READ(priv->base + I2C_DATA_REG) & 0xff);
          }
      }

      offset += chunk;
      remaining -= chunk;
    }

  return OK;
}

/****************************************************************************
 * Name: esp32p4_i2c_do_writeread
 *
 * Description:
 *   Perform a combined write-then-read I2C transaction:
 *     START -> WRITE(addr+W) -> WRITE(reg_addr) ->
 *     RESTART -> WRITE(addr+R) -> READ(data) -> STOP
 *
 *   This is commonly used for reading registers from I2C devices.
 *
 ****************************************************************************/

static int esp32p4_i2c_do_writeread(struct esp32p4_i2c_priv_s *priv,
                                    FAR struct i2c_msg_s *wmsg,
                                    FAR struct i2c_msg_s *rmsg)
{
  int ret;
  uint8_t addr_byte;
  int i;
  int remaining;
  int chunk;
  int offset = 0;

  DEBUGASSERT(wmsg->length <= I2C_FIFO_LEN);

  /* Wait for bus to be idle */

  ret = esp32p4_i2c_wait_bus_idle(priv);
  if (ret < 0)
    {
      return ret;
    }

  /* Clear all pending interrupts */

  REG_WRITE(priv->base + I2C_INT_CLR_REG, 0x1ffff);

  /* Reset FIFO */

  esp32p4_i2c_reset_fifo(priv);

  /* Build write address byte: 7-bit address + Write bit (0) */

  addr_byte = (uint8_t)(wmsg->addr << 1);

  /* Set up command sequence:
   *   CMD0: START
   *   CMD1: WRITE(addr+W, ack_check)
   *   CMD2: WRITE(data, ack_check)
   *   CMD3: RESTART
   *   CMD4: WRITE(addr+R, ack_check)
   *   CMD5: READ(data) with NACK on last byte
   *   CMD6: STOP
   *   CMD7: END
   */

  /* CMD0: START */

  ret = esp32p4_i2c_write_cmd(priv, 0, I2C_CMD_OP_RSTART, 0,
                              false, false, false);
  if (ret < 0)
    {
      return ret;
    }

  /* CMD1: WRITE address byte with ACK check */

  ret = esp32p4_i2c_write_cmd(priv, 1, I2C_CMD_OP_WRITE, 1,
                              true, false, false);
  if (ret < 0)
    {
      return ret;
    }

  /* Write address byte to FIFO */

  REG_WRITE(priv->base + I2C_TXFIFO_MEM, addr_byte);

  /* Write register address/data bytes to FIFO */

  for (i = 0; i < wmsg->length; i++)
    {
      REG_WRITE(priv->base + I2C_TXFIFO_MEM, wmsg->buffer[i]);
    }

  /* CMD2: WRITE data bytes with ACK check */

  ret = esp32p4_i2c_write_cmd(priv, 2, I2C_CMD_OP_WRITE, wmsg->length,
                              true, false, false);
  if (ret < 0)
    {
      return ret;
    }

  /* Start transaction for write phase */

  REG_SET_BIT(priv->base + I2C_CTR_REG, I2C_TRANS_START);

  /* Wait for CMD1 (address) to complete */

  ret = esp32p4_i2c_poll_cmd_done(priv, 1);
  if (ret < 0)
    {
      return ret;
    }

  /* Check for NACK on address */

  if (REG_READ(priv->base + I2C_SR_REG) & I2C_RESP_REC)
    {
      i2cerr("ERROR: I2C NACK on address 0x%02x\n", wmsg->addr);
      return -EIO;
    }

  /* Wait for CMD2 (data) to complete */

  ret = esp32p4_i2c_poll_cmd_done(priv, 2);
  if (ret < 0)
    {
      return ret;
    }

  /* Wait for write phase to complete */

  ret = esp32p4_i2c_poll_trans_done(priv);
  if (ret < 0)
    {
      return ret;
    }

  /* Now set up the read phase */

  /* Build read address byte: 7-bit address + Read bit (1) */

  addr_byte = (uint8_t)((rmsg->addr << 1) | 1);

  /* Read data in chunks of up to I2C_FIFO_LEN */

  remaining = rmsg->length;
  while (remaining > 0)
    {
      chunk = (remaining > I2C_FIFO_LEN) ? I2C_FIFO_LEN : remaining;

      /* Reset FIFO */

      esp32p4_i2c_reset_fifo(priv);

      /* CMD0: RESTART */

      ret = esp32p4_i2c_write_cmd(priv, 0, I2C_CMD_OP_RSTART, 0,
                                  false, false, false);
      if (ret < 0)
        {
          return ret;
        }

      /* CMD1: WRITE address byte with ACK check */

      ret = esp32p4_i2c_write_cmd(priv, 1, I2C_CMD_OP_WRITE, 1,
                                  true, false, false);
      if (ret < 0)
        {
          return ret;
        }

      /* Write address byte to FIFO */

      REG_WRITE(priv->base + I2C_TXFIFO_MEM, addr_byte);

      /* CMD2: READ data, NACK on last byte */

      ret = esp32p4_i2c_write_cmd(priv, 2, I2C_CMD_OP_READ, chunk,
                                  false, false,
                                  (remaining <= chunk) ? true : false);
      if (ret < 0)
        {
          return ret;
        }

      /* CMD3: STOP on last chunk */

      if (remaining <= chunk)
        {
          ret = esp32p4_i2c_write_cmd(priv, 3, I2C_CMD_OP_STOP, 0,
                                      false, false, false);
          if (ret < 0)
            {
              return ret;
            }
        }

      /* CMD4: END */

      ret = esp32p4_i2c_write_cmd(priv, 4, I2C_CMD_OP_END, 0,
                                  false, false, false);
      if (ret < 0)
        {
          return ret;
        }

      /* Start the read transaction */

      REG_SET_BIT(priv->base + I2C_CTR_REG, I2C_TRANS_START);

      /* Wait for CMD1 (address) to complete */

      ret = esp32p4_i2c_poll_cmd_done(priv, 1);
      if (ret < 0)
        {
          return ret;
        }

      /* Check for NACK on address */

      if (REG_READ(priv->base + I2C_SR_REG) & I2C_RESP_REC)
        {
          i2cerr("ERROR: I2C NACK on read address 0x%02x\n", rmsg->addr);
          return -EIO;
        }

      /* Wait for CMD2 (data) to complete */

      ret = esp32p4_i2c_poll_cmd_done(priv, 2);
      if (ret < 0)
        {
          return ret;
        }

      /* Wait for transaction complete */

      ret = esp32p4_i2c_poll_trans_done(priv);
      if (ret < 0)
        {
          return ret;
        }

      /* Read data from RX FIFO */

      for (i = 0; i < chunk; i++)
        {
          rmsg->buffer[offset + i] =
            (uint8_t)(REG_READ(priv->base + I2C_DATA_REG) & 0xff);
        }

      offset += chunk;
      remaining -= chunk;
    }

  return OK;
}

/****************************************************************************
 * Name: esp32p4_i2c_transfer
 *
 * Description:
 *   Perform a sequence of I2C transfers.
 *
 *   This is the main entry point for I2C transfers.  It handles:
 *   - Single write messages
 *   - Single read messages
 *   - Combined write-then-read messages (register read pattern)
 *
 ****************************************************************************/

static int esp32p4_i2c_transfer(FAR struct i2c_master_s *dev,
                                FAR struct i2c_msg_s *msgs, int count)
{
  FAR struct esp32p4_i2c_priv_s *priv = (FAR struct esp32p4_i2c_priv_s *)dev;
  int ret;
  int i;

  DEBUGASSERT(dev != NULL && msgs != NULL && count > 0);

  /* Acquire the bus mutex */

  ret = nxmutex_lock(&priv->lock);
  if (ret < 0)
    {
      return ret;
    }

  /* Process messages */

  for (i = 0; i < count; i++)
    {
      FAR struct i2c_msg_s *msg = &msgs[i];

      /* Check for combined write-then-read pattern:
       * - Current message is a write (not I2C_M_READ)
       * - Next message exists and is a read
       * - Same address
       * - Current message has I2C_M_NOSTOP set
       */

      if (i < count - 1 &&
          !(msg->flags & I2C_M_READ) &&
          (msgs[i + 1].flags & I2C_M_READ) &&
          msg->addr == msgs[i + 1].addr)
        {
          /* Combined write-then-read */

          ret = esp32p4_i2c_do_writeread(priv, msg, &msgs[i + 1]);
          i++;  /* Skip the read message */
        }
      else if (msg->flags & I2C_M_READ)
        {
          /* Read transaction */

          ret = esp32p4_i2c_do_read(priv, msg);
        }
      else
        {
          /* Write transaction */

          ret = esp32p4_i2c_do_write(priv, msg);
        }

      if (ret < 0)
        {
          goto errout;
        }
    }

errout:
  nxmutex_unlock(&priv->lock);
  return ret;
}

/****************************************************************************
 * Name: esp32p4_i2c_reset
 *
 * Description:
 *   Perform an I2C bus reset to recover from stuck devices.
 *
 *   This resets the I2C controller state machine and reconfigures
 *   the clock.
 *
 ****************************************************************************/

static int esp32p4_i2c_reset(FAR struct i2c_master_s *dev)
{
  FAR struct esp32p4_i2c_priv_s *priv = (FAR struct esp32p4_i2c_priv_s *)dev;
  int ret;

  ret = nxmutex_lock(&priv->lock);
  if (ret < 0)
    {
      return ret;
    }

  /* Reset the I2C controller */

  REG_SET_BIT(priv->base + I2C_CTR_REG, I2C_FSM_RST);

  /* Reconfigure clock */

  esp32p4_i2c_set_clock(priv, priv->freq);

  /* Reconfigure GPIO */

  esp32p4_i2c_config_gpio(priv);

  nxmutex_unlock(&priv->lock);
  return OK;
}

/****************************************************************************
 * Name: esp32p4_i2c_setup
 *
 * Description:
 *   Initialize the I2C controller.
 *
 *   This is called when the I2C device is first opened.  It configures
 *   the GPIO pins, sets the clock frequency, and enables master mode.
 *
 ****************************************************************************/

static int esp32p4_i2c_setup(FAR struct i2c_master_s *dev)
{
  FAR struct esp32p4_i2c_priv_s *priv = (FAR struct esp32p4_i2c_priv_s *)dev;
  int ret;

  ret = nxmutex_lock(&priv->lock);
  if (ret < 0)
    {
      return ret;
    }

  /* Increment reference count */

  priv->refs++;
  if (priv->refs > 1)
    {
      nxmutex_unlock(&priv->lock);
      return OK;
    }

  /* Configure GPIO pins for I2C */

  esp32p4_i2c_config_gpio(priv);

  /* Enable master mode, clock enable, and arbitration */

  REG_WRITE(priv->base + I2C_CTR_REG,
            I2C_MS_MODE |          /* Master mode */
            I2C_CLK_EN |           /* Clock enable */
            I2C_ARBITRATION_EN);   /* Arbitration enable */

  /* Configure FIFO */

  REG_WRITE(priv->base + I2C_FIFO_CONF_REG,
            I2C_NONFIFO_EN |       /* Non-FIFO mode */
            I2C_FIFO_PRT_EN);      /* FIFO protection */

  /* Set clock frequency */

  esp32p4_i2c_set_clock(priv, priv->freq);

  /* Clear all pending interrupts */

  REG_WRITE(priv->base + I2C_INT_CLR_REG, 0x1ffff);

  /* Disable all interrupts (polling mode) */

  REG_WRITE(priv->base + I2C_INT_ENA_REG, 0);

  /* Update configuration */

  REG_SET_BIT(priv->base + I2C_CTR_REG, I2C_CONF_UPGATE);

  nxmutex_unlock(&priv->lock);
  return OK;
}

/****************************************************************************
 * Name: esp32p4_i2c_shutdown
 *
 * Description:
 *   Shut down the I2C controller.
 *
 *   This is called when the I2C device is closed.  It disables the
 *   I2C controller and releases resources.
 *
 ****************************************************************************/

static int esp32p4_i2c_shutdown(FAR struct i2c_master_s *dev)
{
  FAR struct esp32p4_i2c_priv_s *priv = (FAR struct esp32p4_i2c_priv_s *)dev;
  int ret;

  ret = nxmutex_lock(&priv->lock);
  if (ret < 0)
    {
      return ret;
    }

  /* Decrement reference count */

  priv->refs--;
  if (priv->refs > 0)
    {
      nxmutex_unlock(&priv->lock);
      return OK;
    }

  /* Disable all interrupts */

  REG_WRITE(priv->base + I2C_INT_ENA_REG, 0);

  /* Clear all pending interrupts */

  REG_WRITE(priv->base + I2C_INT_CLR_REG, 0x1ffff);

  /* Reset the I2C controller */

  REG_SET_BIT(priv->base + I2C_CTR_REG, I2C_FSM_RST);

  nxmutex_unlock(&priv->lock);
  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: esp32p4_i2cbus_register
 *
 * Description:
 *   Register an I2C bus driver for the specified I2C port.
 *
 *   This function initializes the I2C controller hardware and registers
 *   the I2C character device at /dev/i2cN.
 *
 * Input Parameters:
 *   port - I2C port number (0 for I2C0, 1 for I2C1)
 *
 * Returned Value:
 *   OK on success; a negated errno value on failure.
 *
 ****************************************************************************/

int esp32p4_i2cbus_register(int port)
{
  FAR struct esp32p4_i2c_priv_s *priv;
  int ret;

  switch (port)
    {
#ifdef CONFIG_ESP32P4_I2C0
      case 0:
        priv = &g_i2c0_priv;
        break;
#endif

#ifdef CONFIG_ESP32P4_I2C1
      case 1:
        priv = &g_i2c1_priv;
        break;
#endif

      default:
        i2cerr("ERROR: Invalid I2C port: %d\n", port);
        return -EINVAL;
    }

  /* Register the I2C character device */

  ret = i2c_register((FAR struct i2c_master_s *)priv, port);
  if (ret < 0)
    {
      i2cerr("ERROR: Failed to register I2C%d: %d\n", port, ret);
      return ret;
    }

  i2cinfo("I2C%d registered successfully\n", port);
  return OK;
}
