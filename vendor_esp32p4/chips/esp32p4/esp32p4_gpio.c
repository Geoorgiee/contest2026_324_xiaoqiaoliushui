/****************************************************************************
 * vendor/espressif/chips/esp32p4/esp32p4_gpio.c
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
#include <stdbool.h>
#include <assert.h>
#include <errno.h>
#include <debug.h>

#include <nuttx/irq.h>
#include <nuttx/arch.h>

#ifdef CONFIG_DEV_GPIO
#  include <nuttx/ioexpander/gpio.h>
#endif

#include "hardware/esp32p4_gpio.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Number of GPIO pins on ESP32-P4 */

#define ESP32P4_GPIO_COUNT      GPIO_NUM_MAX

/* GPIO register helper macros.
 *
 * GPIO registers are split into two groups:
 *   - REG0: GPIO 0-31  (address offset 0x00)
 *   - REG1: GPIO 32-53 (address offset 0x04)
 */

#define GPIO_OUT_ADDR(gpio)     ((gpio) < 32 ? GPIO_OUT_REG : GPIO_OUT1_REG)
#define GPIO_IN_ADDR(gpio)      ((gpio) < 32 ? GPIO_IN_REG  : GPIO_IN1_REG)
#define GPIO_STATUS_ADDR(gpio)  ((gpio) < 32 ? GPIO_STATUS_REG : \
                                                  GPIO_STATUS1_REG)
#define GPIO_W1TS_ADDR(gpio)    ((gpio) < 32 ? GPIO_OUT_W1TS_REG : \
                                                  GPIO_OUT1_W1TS_REG)
#define GPIO_W1TC_ADDR(gpio)    ((gpio) < 32 ? GPIO_OUT_W1TC_REG : \
                                                  GPIO_OUT1_W1TC_REG)
#define GPIO_STATUS_W1TC_ADDR(gpio) \
                                ((gpio) < 32 ? GPIO_STATUS_W1TC_REG : \
                                               GPIO_STATUS1_W1TC_REG)

#define GPIO_ENABLE_W1TS_ADDR(gpio) \
                                ((gpio) < 32 ? GPIO_ENABLE_W1TS_REG : \
                                               GPIO_ENABLE1_W1TS_REG)
#define GPIO_ENABLE_W1TC_ADDR(gpio) \
                                ((gpio) < 32 ? GPIO_ENABLE_W1TC_REG : \
                                               GPIO_ENABLE1_W1TC_REG)

#define GPIO_BIT(gpio)          (1u << ((gpio) & 0x1f))

/* SIG_GPIO_OUT_IDX: value that routes simple GPIO output (not peripheral
 * signal) through the GPIO matrix.  On ESP32-P4 this is 128 per the TRM.
 */

#define SIG_GPIO_OUT_IDX        128

/****************************************************************************
 * Private Types
 ****************************************************************************/

/* GPIO ISR callback structure */

struct esp32p4_gpio_callback_s
{
  xcpt_t handler;         /* Interrupt handler */
  void  *arg;             /* Handler argument */
};

#ifdef CONFIG_DEV_GPIO
/* Per-pin GPIO device state for NuttX gpio_operations_s */

struct esp32p4_gpio_dev_s
{
  struct gpio_dev_s dev;          /* NuttX GPIO device (must be first) */
  int               gpio;        /* GPIO pin number */
  pin_interrupt_t   callback;    /* Upper-half interrupt callback */
};
#endif

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* GPIO interrupt handler table */

static struct esp32p4_gpio_callback_s
  g_gpio_callbacks[ESP32P4_GPIO_COUNT];

#ifdef CONFIG_DEV_GPIO
/* GPIO device instances (one per pin) */

static struct esp32p4_gpio_dev_s g_gpio_devs[ESP32P4_GPIO_COUNT];
#endif

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: esp32p4_iomux_config
 *
 * Description:
 *   Configure the IO MUX register for a GPIO pin.  This sets the pin
 *   function to GPIO, configures input enable, pull-up/pull-down, and
 *   drive strength.
 *
 *   Reference: ESP-IDF gpio_ll_pullup_en / gpio_ll_input_enable /
 *   gpio_ll_func_sel in hal/esp32p4/include/hal/gpio_ll.h
 *
 * Input Parameters:
 *   gpio      - GPIO number (0 to GPIO_NUM_MAX-1)
 *   input_en  - true to enable input, false to disable
 *   pullup    - true to enable pull-up, false to disable
 *   pulldown  - true to enable pull-down, false to disable
 *
 ****************************************************************************/

static void esp32p4_iomux_config(int gpio, bool input_en,
                                  bool pullup, bool pulldown)
{
  uint32_t regaddr;
  uint32_t regval;

  regaddr = IO_MUX_GPIO_REG(gpio);
  regval  = REG_READ(regaddr);

  /* Select GPIO function (MCU_SEL = IO_MUX_GPIO_FUNC = 1).
   * This routes the pin through the GPIO matrix rather than a
   * dedicated peripheral function.
   */

  regval &= ~IO_MUX_MCU_SEL_M;
  regval |= (IO_MUX_GPIO_FUNC << IO_MUX_MCU_SEL_S);

  /* Configure input enable (FUN_IE bit 9) */

  if (input_en)
    {
      regval |= IO_MUX_FUN_IE;
    }
  else
    {
      regval &= ~IO_MUX_FUN_IE;
    }

  /* Configure pull-up (FUN_WPU bit 8).
   * Reference: ESP-IDF gpio_ll_pullup_en in
   *   hal/esp32p4/include/hal/gpio_ll.h: IO_MUX.gpio[n].fun_wpu = 1
   */

  if (pullup)
    {
      regval |= IO_MUX_FUN_WPU;
    }
  else
    {
      regval &= ~IO_MUX_FUN_WPU;
    }

  /* Configure pull-down (FUN_WPD bit 7).
   * Reference: ESP-IDF gpio_ll_pulldown_en in
   *   hal/esp32p4/include/hal/gpio_ll.h: IO_MUX.gpio[n].fun_wpd = 1
   */

  if (pulldown)
    {
      regval |= IO_MUX_FUN_WPD;
    }
  else
    {
      regval &= ~IO_MUX_FUN_WPD;
    }

  REG_WRITE(regaddr, regval);
}

/****************************************************************************
 * Name: esp32p4_gpio_output_enable
 *
 * Description:
 *   Enable or disable the output driver for a GPIO pin.
 *
 *   Reference: ESP-IDF gpio_ll_output_enable / gpio_ll_output_disable in
 *   hal/esp32p4/include/hal/gpio_ll.h which use
 *   hw->enable_w1ts / hw->enable_w1tc (GPIO 0-31)
 *   hw->enable1_w1ts / hw->enable1_w1tc (GPIO 32-56)
 *
 * Input Parameters:
 *   gpio   - GPIO number (0 to GPIO_NUM_MAX-1)
 *   enable - true to enable output, false to disable
 *
 ****************************************************************************/

static void esp32p4_gpio_output_enable(int gpio, bool enable)
{
  if (enable)
    {
      /* Set output enable via W1TS register (atomic) */

      REG_WRITE(GPIO_ENABLE_W1TS_ADDR(gpio), GPIO_BIT(gpio));
    }
  else
    {
      /* Clear output enable via W1TC register (atomic) */

      REG_WRITE(GPIO_ENABLE_W1TC_ADDR(gpio), GPIO_BIT(gpio));
    }
}

/****************************************************************************
 * Name: esp32p4_gpio_matrix_out_default
 *
 * Description:
 *   Disconnect any peripheral output signal routed via GPIO matrix to
 *   the pin.  This sets the output function selection to SIG_GPIO_OUT_IDX
 *   (simple GPIO output, not a peripheral signal).
 *
 *   Reference: ESP-IDF gpio_ll_matrix_out_default in
 *   hal/esp32p4/include/hal/gpio_ll.h:
 *     hw->func_out_sel_cfg[n].out_sel = SIG_GPIO_OUT_IDX
 *
 * Input Parameters:
 *   gpio - GPIO number (0 to GPIO_NUM_MAX-1)
 *
 ****************************************************************************/

static void esp32p4_gpio_matrix_out_default(int gpio)
{
  REG_WRITE(GPIO_FUNC_OUT_SEL_CFG_REG(gpio), SIG_GPIO_OUT_IDX);
}

/****************************************************************************
 * Name: esp32p4_gpio_set_intr_type
 *
 * Description:
 *   Set the interrupt trigger type for a GPIO pin.
 *
 *   Reference: ESP-IDF gpio_ll_set_intr_type in
 *   hal/esp32p4/include/hal/gpio_ll.h:
 *     hw->pin[n].int_type = intr_type
 *
 * Input Parameters:
 *   gpio      - GPIO number (0 to GPIO_NUM_MAX-1)
 *   intr_type - Interrupt type (GPIO_INTR_DISABLE, GPIO_INTR_POSEDGE,
 *               GPIO_INTR_NEGEDGE, GPIO_INTR_ANYEDGE,
 *               GPIO_INTR_LOW_LEVEL, GPIO_INTR_HIGH_LEVEL)
 *
 ****************************************************************************/

static void esp32p4_gpio_set_intr_type(int gpio, int intr_type)
{
  uint32_t regaddr;
  uint32_t regval;

  regaddr = GPIO_PIN_REG(gpio);
  regval  = REG_READ(regaddr);

  /* Clear the interrupt type field and set the new value */

  regval &= ~GPIO_PIN_INT_TYPE_M;
  regval |= ((intr_type << GPIO_PIN_INT_TYPE_S) & GPIO_PIN_INT_TYPE_M);

  REG_WRITE(regaddr, regval);
}

/****************************************************************************
 * Name: esp32p4_gpio_intr_enable
 *
 * Description:
 *   Enable the GPIO interrupt for a specific pin by setting the
 *   interrupt enable bits in the GPIO_PIN_REG.
 *
 *   Reference: ESP-IDF gpio_ll_intr_enable_on_core in
 *   hal/esp32p4/include/hal/gpio_ll.h:
 *     hw->pin[n].int_ena = GPIO_LL_INTR0_ENA
 *
 * Input Parameters:
 *   gpio - GPIO number (0 to GPIO_NUM_MAX-1)
 *
 ****************************************************************************/

static void esp32p4_gpio_intr_enable(int gpio)
{
  uint32_t regaddr;
  uint32_t regval;

  regaddr = GPIO_PIN_REG(gpio);
  regval  = REG_READ(regaddr);

  regval &= ~GPIO_PIN_INT_ENA_M;
  regval |= GPIO_PIN_INT_ENA_INTR0;

  REG_WRITE(regaddr, regval);
}

/****************************************************************************
 * Name: esp32p4_gpio_intr_disable
 *
 * Description:
 *   Disable the GPIO interrupt for a specific pin.
 *
 *   Reference: ESP-IDF gpio_ll_intr_disable in
 *   hal/esp32p4/include/hal/gpio_ll.h:
 *     hw->pin[n].int_ena = 0
 *
 * Input Parameters:
 *   gpio - GPIO number (0 to GPIO_NUM_MAX-1)
 *
 ****************************************************************************/

static void esp32p4_gpio_intr_disable(int gpio)
{
  uint32_t regaddr;
  uint32_t regval;

  regaddr = GPIO_PIN_REG(gpio);
  regval  = REG_READ(regaddr);

  regval &= ~GPIO_PIN_INT_ENA_M;

  REG_WRITE(regaddr, regval);
}

/****************************************************************************
 * Name: esp32p4_gpio_interrupt
 *
 * Description:
 *   GPIO interrupt handler.  This is called from the PLIC interrupt
 *   dispatch when a GPIO interrupt is detected.  It reads the GPIO
 *   status register to determine which GPIOs have pending interrupts,
 *   and dispatches to the registered handlers.
 *
 *   Reference: ESP-IDF gpio_intr_service in
 *   components/esp_driver_gpio/src/gpio.c which reads
 *   hw->intr_0.int_0 (GPIO 0-31) and hw->intr1_0.int1_0 (GPIO 32+)
 *
 ****************************************************************************/

static int esp32p4_gpio_interrupt(int irq, void *context, void *arg)
{
  uint32_t status;
  int gpio;

  /* Read the interrupt status for GPIO 0-31 */

  status = REG_READ(GPIO_STATUS_REG);

  /* Clear the handled interrupt bits */

  REG_WRITE(GPIO_STATUS_W1TC_REG, status);

  /* Dispatch to registered handlers for GPIO 0-31 */

  for (gpio = 0; gpio < 32 && status != 0; gpio++)
    {
      if ((status & GPIO_BIT(gpio)) != 0)
        {
          if (g_gpio_callbacks[gpio].handler != NULL)
            {
              g_gpio_callbacks[gpio].handler(irq, context,
                                             g_gpio_callbacks[gpio].arg);
            }

#ifdef CONFIG_DEV_GPIO
          /* Notify upper-half via callback if registered */

          if (g_gpio_devs[gpio].callback != NULL)
            {
              g_gpio_devs[gpio].callback(&g_gpio_devs[gpio].dev, gpio);
            }
#endif

          status &= ~GPIO_BIT(gpio);
        }
    }

  /* Read the interrupt status for GPIO 32-53 */

  status = REG_READ(GPIO_STATUS1_REG);

  /* Clear the handled interrupt bits */

  REG_WRITE(GPIO_STATUS1_W1TC_REG, status);

  /* Dispatch to registered handlers for GPIO 32-53 */

  for (gpio = 32; gpio < ESP32P4_GPIO_COUNT && status != 0; gpio++)
    {
      if ((status & GPIO_BIT(gpio)) != 0)
        {
          if (g_gpio_callbacks[gpio].handler != NULL)
            {
              g_gpio_callbacks[gpio].handler(irq, context,
                                             g_gpio_callbacks[gpio].arg);
            }

#ifdef CONFIG_DEV_GPIO
          /* Notify upper-half via callback if registered */

          if (g_gpio_devs[gpio].callback != NULL)
            {
              g_gpio_devs[gpio].callback(&g_gpio_devs[gpio].dev, gpio);
            }
#endif

          status &= ~GPIO_BIT(gpio);
        }
    }

  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: esp32p4_config_gpio
 *
 * Description:
 *   Configure a GPIO pin for input or output mode, with optional
 *   pull-up/pull-down resistors and interrupt trigger type.
 *
 *   This function follows the ESP-IDF gpio_config() pattern:
 *   1. Configure IO MUX (function select, input enable, pull-up/down)
 *   2. Configure GPIO matrix output routing
 *   3. Enable/disable output driver
 *   4. Configure interrupt type and enable/disable interrupt
 *
 *   Reference: ESP-IDF gpio_config() in
 *   components/esp_driver_gpio/src/gpio.c
 *
 * Input Parameters:
 *   gpio - GPIO number (0 to GPIO_NUM_MAX-1)
 *   mode - GPIO_OUTPUT, GPIO_INPUT, GPIO_INPUT_PULLUP,
 *          GPIO_INPUT_PULLDOWN, or one of the GPIO_INTR_* values
 *          (GPIO_INTR_POSEDGE, GPIO_INTR_NEGEDGE, GPIO_INTR_ANYEDGE,
 *          GPIO_INTR_LOW_LEVEL, GPIO_INTR_HIGH_LEVEL)
 *
 * Returned Value:
 *   OK on success; A negated errno value on failure.
 *
 ****************************************************************************/

int esp32p4_config_gpio(int gpio, int mode)
{
  bool input_en  = false;
  bool output_en = false;
  bool pullup    = false;
  bool pulldown  = false;
  int  intr_type = GPIO_INTR_DISABLE;

  DEBUGASSERT(gpio >= 0 && gpio < ESP32P4_GPIO_COUNT);

  /* The 'mode' parameter encodes both direction and interrupt type.
   * Bits 0-3: direction (GPIO_INPUT, GPIO_OUTPUT, etc.)
   * Bits 4-7: interrupt type (GPIO_INTR_*)
   * This avoids the overlapping enum values issue.
   */

  int direction = mode & 0x0f;
  int intr = (mode >> 4) & 0x0f;

  switch (direction)
    {
      case GPIO_OUTPUT:
        output_en = true;
        break;

      case GPIO_INPUT:
        input_en = true;
        break;

      case GPIO_INPUT_PULLUP:
        input_en = true;
        pullup   = true;
        break;

      case GPIO_INPUT_PULLDOWN:
        input_en   = true;
        pulldown   = true;
        break;

      default:
        gpioerr("ERROR: Invalid GPIO direction %d for GPIO %d\n",
                direction, gpio);
        return -EINVAL;
    }

  if (intr != 0)
    {
      intr_type = intr;
    }

  /* Step 1: Configure IO MUX
   *
   * Set the pin function to GPIO (MCU_SEL = 1), enable/disable input,
   * and configure pull-up/pull-down resistors.
   *
   * Reference: ESP-IDF gpio_config() calls gpio_hal_func_sel() which
   * sets IO_MUX.gpio[n].mcu_sel = PIN_FUNC_GPIO, and
   * gpio_pullup_en/dis + gpio_pulldown_en/dis which set
   * IO_MUX.gpio[n].fun_wpu / fun_wpd.
   */

  esp32p4_iomux_config(gpio, input_en, pullup, pulldown);

  /* Step 2: Configure GPIO matrix output routing
   *
   * Route the GPIO output signal through the GPIO matrix.  This
   * disconnects any peripheral signal and connects the simple GPIO
   * output (SIG_GPIO_OUT_IDX).
   *
   * Reference: ESP-IDF gpio_config() calls
   * gpio_hal_matrix_out_default() which sets
   * hw->func_out_sel_cfg[n].out_sel = SIG_GPIO_OUT_IDX
   */

  esp32p4_gpio_matrix_out_default(gpio);

  /* Step 3: Enable/disable output driver
   *
   * Reference: ESP-IDF gpio_config() calls gpio_output_enable() which
   * uses hw->enable_w1ts / hw->enable1_w1ts for output enable.
   */

  esp32p4_gpio_output_enable(gpio, output_en);

  /* Step 4: Configure interrupt type
   *
   * Reference: ESP-IDF gpio_config() calls gpio_set_intr_type() which
   * sets hw->pin[n].int_type = intr_type, and gpio_intr_enable/dis
   * which sets hw->pin[n].int_ena.
   */

  esp32p4_gpio_set_intr_type(gpio, intr_type);

  if (intr_type != GPIO_INTR_DISABLE)
    {
      esp32p4_gpio_intr_enable(gpio);
    }
  else
    {
      esp32p4_gpio_intr_disable(gpio);
    }

  gpioinfo("GPIO%d configured: mode=%d pullup=%d pulldown=%d intr=%d\n",
           gpio, mode, pullup, pulldown, intr_type);

  return OK;
}

/****************************************************************************
 * Name: esp32p4_gpio_write
 *
 * Description:
 *   Write a value to a GPIO pin.
 *
 *   Uses the atomic W1TS/W1TC registers to set or clear the output
 *   level without requiring a read-modify-write operation.
 *
 *   Reference: ESP-IDF gpio_ll_set_level in
 *   hal/esp32p4/include/hal/gpio_ll.h:
 *     if (level) hw->out_w1ts.val = 1 << gpio_num
 *     else       hw->out_w1tc.val = 1 << gpio_num
 *
 * Input Parameters:
 *   gpio  - GPIO number (0 to GPIO_NUM_MAX-1)
 *   value - true for high, false for low
 *
 ****************************************************************************/

void esp32p4_gpio_write(int gpio, bool value)
{
  DEBUGASSERT(gpio >= 0 && gpio < ESP32P4_GPIO_COUNT);

  if (value)
    {
      REG_WRITE(GPIO_W1TS_ADDR(gpio), GPIO_BIT(gpio));
    }
  else
    {
      REG_WRITE(GPIO_W1TC_ADDR(gpio), GPIO_BIT(gpio));
    }
}

/****************************************************************************
 * Name: esp32p4_gpio_read
 *
 * Description:
 *   Read the current value of a GPIO pin.
 *
 *   Reference: ESP-IDF gpio_ll_get_level in
 *   hal/esp32p4/include/hal/gpio_ll.h:
 *     return (hw->in.in_data_next >> gpio_num) & 0x1
 *
 * Input Parameters:
 *   gpio - GPIO number (0 to GPIO_NUM_MAX-1)
 *
 * Returned Value:
 *   true if the pin is high, false if low.
 *
 ****************************************************************************/

bool esp32p4_gpio_read(int gpio)
{
  DEBUGASSERT(gpio >= 0 && gpio < ESP32P4_GPIO_COUNT);

  return (REG_READ(GPIO_IN_ADDR(gpio)) & GPIO_BIT(gpio)) != 0;
}

/****************************************************************************
 * Name: esp32p4_gpio_set_irq
 *
 * Description:
 *   Set up an interrupt handler for a GPIO pin.  The handler will be
 *   called from the GPIO interrupt dispatcher when the pin's interrupt
 *   fires.
 *
 * Input Parameters:
 *   gpio    - GPIO number
 *   handler - Interrupt handler (xcpt_t signature)
 *   arg     - Handler argument
 *
 * Returned Value:
 *   OK on success; A negated errno value on failure.
 *
 ****************************************************************************/

int esp32p4_gpio_set_irq(int gpio, xcpt_t handler, void *arg)
{
  irqstate_t flags;

  DEBUGASSERT(gpio >= 0 && gpio < ESP32P4_GPIO_COUNT);

  flags = enter_critical_section();

  /* Save the handler and argument */

  g_gpio_callbacks[gpio].handler = handler;
  g_gpio_callbacks[gpio].arg     = arg;

  leave_critical_section(flags);

  return OK;
}

/****************************************************************************
 * Name: esp32p4_gpio_init
 *
 * Description:
 *   Initialize the GPIO driver.  This attaches the GPIO interrupt
 *   handler to the PLIC and should be called during board bring-up.
 *
 * Returned Value:
 *   OK on success; A negated errno value on failure.
 *
 ****************************************************************************/

int esp32p4_gpio_init(void)
{
  int ret;

  /* Attach the GPIO interrupt handler */

  ret = irq_attach(ESP32P4_IRQ_GPIO0, esp32p4_gpio_interrupt, NULL);
  if (ret < 0)
    {
      gpioerr("ERROR: Failed to attach GPIO interrupt: %d\n", ret);
      return ret;
    }

  /* Enable the GPIO interrupt */

  up_enable_irq(ESP32P4_IRQ_GPIO0);

  return OK;
}

/****************************************************************************
 * NuttX GPIO Lower-Half Operations (CONFIG_DEV_GPIO)
 *
 * The following functions implement the NuttX gpio_operations_s interface
 * defined in nuttx/ioexpander/gpio.h.  This allows user-space access to
 * GPIO pins via /dev/gpioN character devices.
 *
 * Interface methods:
 *   go_read       - Read pin value (required for all pin types)
 *   go_write      - Write pin value (required for output pin types)
 *   go_attach     - Attach interrupt callback (required for interrupt types)
 *   go_enable     - Enable/disable interrupt (required for interrupt types)
 *   go_setpintype - Change pin type (required for all pin types)
 *   go_setdebounce - Set debounce duration (optional, not supported)
 *   go_setmask    - Set interrupt mask (optional, not used)
 ****************************************************************************/

#ifdef CONFIG_DEV_GPIO

/****************************************************************************
 * Name: esp32p4_gpio_ops_read
 *
 * Description:
 *   Read the value of a GPIO pin.  Implements the NuttX go_read interface.
 *
 ****************************************************************************/

static int esp32p4_gpio_ops_read(FAR struct gpio_dev_s *dev,
                                  FAR bool *value)
{
  FAR struct esp32p4_gpio_dev_s *priv =
    (FAR struct esp32p4_gpio_dev_s *)dev;

  DEBUGASSERT(priv != NULL && value != NULL);
  DEBUGASSERT(priv->gpio >= 0 && priv->gpio < ESP32P4_GPIO_COUNT);

  *value = (REG_READ(GPIO_IN_ADDR(priv->gpio)) &
            GPIO_BIT(priv->gpio)) != 0;
  return OK;
}

/****************************************************************************
 * Name: esp32p4_gpio_ops_write
 *
 * Description:
 *   Write a value to a GPIO pin.  Implements the NuttX go_write interface.
 *
 ****************************************************************************/

static int esp32p4_gpio_ops_write(FAR struct gpio_dev_s *dev,
                                   bool value)
{
  FAR struct esp32p4_gpio_dev_s *priv =
    (FAR struct esp32p4_gpio_dev_s *)dev;

  DEBUGASSERT(priv != NULL);
  DEBUGASSERT(priv->gpio >= 0 && priv->gpio < ESP32P4_GPIO_COUNT);

  if (value)
    {
      REG_WRITE(GPIO_W1TS_ADDR(priv->gpio), GPIO_BIT(priv->gpio));
    }
  else
    {
      REG_WRITE(GPIO_W1TC_ADDR(priv->gpio), GPIO_BIT(priv->gpio));
    }

  return OK;
}

/****************************************************************************
 * Name: esp32p4_gpio_ops_attach
 *
 * Description:
 *   Attach an interrupt callback for a GPIO pin.  Implements the NuttX
 *   go_attach interface.
 *
 ****************************************************************************/

static int esp32p4_gpio_ops_attach(FAR struct gpio_dev_s *dev,
                                    pin_interrupt_t callback)
{
  FAR struct esp32p4_gpio_dev_s *priv =
    (FAR struct esp32p4_gpio_dev_s *)dev;
  irqstate_t flags;

  DEBUGASSERT(priv != NULL);
  DEBUGASSERT(priv->gpio >= 0 && priv->gpio < ESP32P4_GPIO_COUNT);

  flags = enter_critical_section();
  priv->callback = callback;
  leave_critical_section(flags);

  return OK;
}

/****************************************************************************
 * Name: esp32p4_gpio_ops_enable
 *
 * Description:
 *   Enable or disable the interrupt for a GPIO pin.  Implements the
 *   NuttX go_enable interface.
 *
 ****************************************************************************/

static int esp32p4_gpio_ops_enable(FAR struct gpio_dev_s *dev,
                                    bool enable)
{
  FAR struct esp32p4_gpio_dev_s *priv =
    (FAR struct esp32p4_gpio_dev_s *)dev;

  DEBUGASSERT(priv != NULL);
  DEBUGASSERT(priv->gpio >= 0 && priv->gpio < ESP32P4_GPIO_COUNT);

  if (enable)
    {
      esp32p4_gpio_intr_enable(priv->gpio);
    }
  else
    {
      esp32p4_gpio_intr_disable(priv->gpio);
    }

  return OK;
}

/****************************************************************************
 * Name: esp32p4_gpio_ops_setpintype
 *
 * Description:
 *   Set the pin type (input, output, interrupt, etc.) for a GPIO pin.
 *   Implements the NuttX go_setpintype interface.  This reconfigures the
 *   hardware to match the requested pin type.
 *
 ****************************************************************************/

static int esp32p4_gpio_ops_setpintype(FAR struct gpio_dev_s *dev,
                                        enum gpio_pintype_e pintype)
{
  FAR struct esp32p4_gpio_dev_s *priv =
    (FAR struct esp32p4_gpio_dev_s *)dev;

  DEBUGASSERT(priv != NULL);
  DEBUGASSERT(priv->gpio >= 0 && priv->gpio < ESP32P4_GPIO_COUNT);

  switch (pintype)
    {
      case GPIO_INPUT_PIN:
        esp32p4_iomux_config(priv->gpio, true, false, false);
        esp32p4_gpio_matrix_out_default(priv->gpio);
        esp32p4_gpio_output_enable(priv->gpio, false);
        esp32p4_gpio_set_intr_type(priv->gpio, GPIO_INTR_DISABLE);
        esp32p4_gpio_intr_disable(priv->gpio);
        break;

      case GPIO_INPUT_PIN_PULLUP:
        esp32p4_iomux_config(priv->gpio, true, true, false);
        esp32p4_gpio_matrix_out_default(priv->gpio);
        esp32p4_gpio_output_enable(priv->gpio, false);
        esp32p4_gpio_set_intr_type(priv->gpio, GPIO_INTR_DISABLE);
        esp32p4_gpio_intr_disable(priv->gpio);
        break;

      case GPIO_INPUT_PIN_PULLDOWN:
        esp32p4_iomux_config(priv->gpio, true, false, true);
        esp32p4_gpio_matrix_out_default(priv->gpio);
        esp32p4_gpio_output_enable(priv->gpio, false);
        esp32p4_gpio_set_intr_type(priv->gpio, GPIO_INTR_DISABLE);
        esp32p4_gpio_intr_disable(priv->gpio);
        break;

      case GPIO_OUTPUT_PIN:
        esp32p4_iomux_config(priv->gpio, false, false, false);
        esp32p4_gpio_matrix_out_default(priv->gpio);
        esp32p4_gpio_output_enable(priv->gpio, true);
        esp32p4_gpio_set_intr_type(priv->gpio, GPIO_INTR_DISABLE);
        esp32p4_gpio_intr_disable(priv->gpio);
        break;

      case GPIO_OUTPUT_PIN_OPENDRAIN:
        esp32p4_iomux_config(priv->gpio, true, false, false);
        esp32p4_gpio_matrix_out_default(priv->gpio);
        esp32p4_gpio_output_enable(priv->gpio, true);
        /* Enable open-drain mode (PAD_DRIVER bit in GPIO_PIN_REG) */

        modifyreg32(GPIO_PIN_REG(priv->gpio), 0,
                     GPIO_PIN_PAD_DRIVER_BIT);
        esp32p4_gpio_set_intr_type(priv->gpio, GPIO_INTR_DISABLE);
        esp32p4_gpio_intr_disable(priv->gpio);
        break;

      case GPIO_INTERRUPT_PIN:
      case GPIO_INTERRUPT_HIGH_PIN:
        esp32p4_iomux_config(priv->gpio, true, false, false);
        esp32p4_gpio_matrix_out_default(priv->gpio);
        esp32p4_gpio_output_enable(priv->gpio, false);
        esp32p4_gpio_set_intr_type(priv->gpio, GPIO_INTR_HIGH_LEVEL);
        esp32p4_gpio_intr_enable(priv->gpio);
        break;

      case GPIO_INTERRUPT_LOW_PIN:
        esp32p4_iomux_config(priv->gpio, true, false, false);
        esp32p4_gpio_matrix_out_default(priv->gpio);
        esp32p4_gpio_output_enable(priv->gpio, false);
        esp32p4_gpio_set_intr_type(priv->gpio, GPIO_INTR_LOW_LEVEL);
        esp32p4_gpio_intr_enable(priv->gpio);
        break;

      case GPIO_INTERRUPT_RISING_PIN:
        esp32p4_iomux_config(priv->gpio, true, false, false);
        esp32p4_gpio_matrix_out_default(priv->gpio);
        esp32p4_gpio_output_enable(priv->gpio, false);
        esp32p4_gpio_set_intr_type(priv->gpio, GPIO_INTR_POSEDGE);
        esp32p4_gpio_intr_enable(priv->gpio);
        break;

      case GPIO_INTERRUPT_FALLING_PIN:
        esp32p4_iomux_config(priv->gpio, true, false, false);
        esp32p4_gpio_matrix_out_default(priv->gpio);
        esp32p4_gpio_output_enable(priv->gpio, false);
        esp32p4_gpio_set_intr_type(priv->gpio, GPIO_INTR_NEGEDGE);
        esp32p4_gpio_intr_enable(priv->gpio);
        break;

      case GPIO_INTERRUPT_BOTH_PIN:
        esp32p4_iomux_config(priv->gpio, true, false, false);
        esp32p4_gpio_matrix_out_default(priv->gpio);
        esp32p4_gpio_output_enable(priv->gpio, false);
        esp32p4_gpio_set_intr_type(priv->gpio, GPIO_INTR_ANYEDGE);
        esp32p4_gpio_intr_enable(priv->gpio);
        break;

      default:
        gpioerr("ERROR: Unsupported pin type %d for GPIO %d\n",
                pintype, priv->gpio);
        return -ENOTSUP;
    }

  dev->gp_pintype = pintype;
  return OK;
}

/****************************************************************************
 * Name: esp32p4_gpio_ops_setdebounce
 *
 * Description:
 *   Set debounce duration for a GPIO pin.  ESP32-P4 GPIO does not have
 *   hardware debounce support; this is a no-op.
 *
 ****************************************************************************/

static int esp32p4_gpio_ops_setdebounce(FAR struct gpio_dev_s *gpio,
                                         unsigned long duration)
{
  return -ENOTSUP;
}

/****************************************************************************
 * Name: esp32p4_gpio_ops_setmask
 *
 * Description:
 *   Set interrupt mask for a GPIO pin.  Not used on ESP32-P4.
 *
 ****************************************************************************/

static int esp32p4_gpio_ops_setmask(FAR struct gpio_dev_s *dev,
                                     bool enable)
{
  return OK;
}

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* GPIO operations vtable */

static const struct gpio_operations_s g_esp32p4_gpio_ops =
{
  esp32p4_gpio_ops_read,        /* go_read */
  esp32p4_gpio_ops_write,       /* go_write */
  esp32p4_gpio_ops_attach,      /* go_attach */
  esp32p4_gpio_ops_enable,      /* go_enable */
  esp32p4_gpio_ops_setpintype,  /* go_setpintype */
  esp32p4_gpio_ops_setdebounce, /* go_setdebounce */
  esp32p4_gpio_ops_setmask,     /* go_setmask */
};

/****************************************************************************
 * Name: esp32p4_gpio_register
 *
 * Description:
 *   Register a GPIO pin as a NuttX GPIO character device at
 *   /dev/gpioN.  This allows user-space access via the standard
 *   GPIO ioctl interface (GPIOC_READ, GPIOC_WRITE, GPIOC_REGISTER, etc.).
 *
 *   This function should be called from board initialization code
 *   (e.g. esp32p4_bringup) for each GPIO pin that needs user-space
 *   access.
 *
 * Input Parameters:
 *   gpio    - GPIO number (0 to GPIO_NUM_MAX-1)
 *   pintype - Initial pin type (see enum gpio_pintype_e)
 *   minor   - Device minor number (e.g. 0 -> /dev/gpio0)
 *
 * Returned Value:
 *   OK on success; A negated errno value on failure.
 *
 ****************************************************************************/

int esp32p4_gpio_register(int gpio, enum gpio_pintype_e pintype,
                           int minor)
{
  FAR struct esp32p4_gpio_dev_s *priv;
  int ret;

  DEBUGASSERT(gpio >= 0 && gpio < ESP32P4_GPIO_COUNT);

  priv = &g_gpio_devs[gpio];

  /* Initialize the GPIO device structure */

  priv->dev.gp_ops = &g_esp32p4_gpio_ops;
  priv->gpio       = gpio;
  priv->callback   = NULL;

  /* Set the initial pin type (this also configures the hardware) */

  ret = esp32p4_gpio_ops_setpintype(&priv->dev, pintype);
  if (ret < 0)
    {
      gpioerr("ERROR: Failed to set pin type %d for GPIO %d: %d\n",
              pintype, gpio, ret);
      return ret;
    }

  /* Register the GPIO device */

  ret = gpio_pin_register(&priv->dev, minor);
  if (ret < 0)
    {
      gpioerr("ERROR: Failed to register GPIO %d as /dev/gpio%d: %d\n",
              gpio, minor, ret);
      return ret;
    }

  gpioinfo("GPIO%d registered as /dev/gpio%d (pintype=%d)\n",
           gpio, minor, pintype);

  return OK;
}

#endif /* CONFIG_DEV_GPIO */
