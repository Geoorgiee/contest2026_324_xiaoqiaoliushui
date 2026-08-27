/****************************************************************************
 * vendor/espressif/chips/esp32p4/esp32p4_serial.c
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
 * ESP32-P4 UART Serial Driver
 *
 * This driver implements the NuttX uart_ops_s interface for the ESP32-P4
 * HP UART peripherals (UART0 and UART1).  The implementation is modeled
 * after the ESP-IDF uart_driver_install() and uart_param_config() APIs.
 *
 * Key features:
 *   - Interrupt-driven TX and RX with FIFO support
 *   - Configurable baud rate, data bits, stop bits, and parity
 *   - RX FIFO timeout detection for efficient byte-by-byte reception
 *   - GPIO matrix pin routing for UART TX/RX signals
 *   - Error detection (parity, framing, overflow)
 *   - Runtime reconfiguration via TCSETS ioctl (including baud rate)
 *
 * ESP-IDF reference:
 *   - examples/peripherals/uart/uart_async_rxtxtasks
 *   - examples/peripherals/uart/uart_echo
 *   - examples/peripherals/uart/uart_events
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/types.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <debug.h>

#include <nuttx/irq.h>
#include <nuttx/arch.h>
#include <nuttx/serial/serial.h>
#include <nuttx/clock.h>
#include <termios.h>

#include "riscv_internal.h"
#include "hardware/esp32p4_uart.h"
#include "hardware/esp32p4_clock.h"
#include "hardware/esp32p4_gpio.h"
#include "hardware/esp32p4_soc.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* UART register access macros */

#define UART_REG(uart, reg) \
  (*(volatile uint32_t *)((uart)->base + (reg)))

/* UART FIFO depth - ESP32-P4 HP UART has 128-byte FIFOs */

#define UART_FIFO_MAX           128

/* RX FIFO full threshold - trigger interrupt when this many bytes received.
 * Matching ESP-IDF UART_RXFIFO_FULL_THRHD_DEFAULT (112).
 */

#define UART_RXFIFO_FULL_THRHD  112

/* TX FIFO empty threshold - trigger interrupt when FIFO drops below this.
 * Matching ESP-IDF UART_TXFIFO_EMPTY_THRHD_DEFAULT (8).
 */

#define UART_TXFIFO_EMPTY_THRHD 8

/* RX FIFO timeout threshold (in UART symbol periods).
 * After this many idle bit periods with no new data, an RX timeout
 * interrupt fires.  This allows byte-by-byte reception without waiting
 * for the FIFO to fill.  Value in bits [16:7] of CONF1 register.
 * ESP-IDF default is 2.
 */

#define UART_RX_TOUT_THRHD      2

/* ESP32-P4 UART signal numbers for GPIO matrix routing.
 * These map UART TX/RX signals to GPIO matrix function numbers.
 * UART0 TX = signal 6, UART0 RX = signal 7 (input index)
 * UART1 TX = signal 8, UART1 RX = signal 9 (input index)
 *
 * Reference: ESP32-P4 Technical Reference Manual, GPIO Matrix chapter.
 */

#define UART0_TX_SIG            6
#define UART0_RX_SIG            7
#define UART1_TX_SIG            8
#define UART1_RX_SIG            9

/* Default GPIO pins for UART (ESP32-P4 EV Board default wiring) */

#ifndef CONFIG_ESP32P4_UART0_TX_GPIO
#  define CONFIG_ESP32P4_UART0_TX_GPIO  16
#endif

#ifndef CONFIG_ESP32P4_UART0_RX_GPIO
#  define CONFIG_ESP32P4_UART0_RX_GPIO  17
#endif

#ifndef CONFIG_ESP32P4_UART1_TX_GPIO
#  define CONFIG_ESP32P4_UART1_TX_GPIO  18
#endif

#ifndef CONFIG_ESP32P4_UART1_RX_GPIO
#  define CONFIG_ESP32P4_UART1_RX_GPIO  19
#endif

/****************************************************************************
 * Private Types
 ****************************************************************************/

/* UART port descriptor - holds per-port hardware configuration.
 * This is referenced via dev->priv in all uart_ops callbacks.
 */

struct esp32p4_uart_port_s
{
  uint32_t base;          /* UART register base address */
  int      irq;           /* Interrupt number */
  uint32_t baud;          /* Baud rate */
  uint32_t clk_freq;      /* Source clock frequency (APB clock) */
  uint8_t  parity;        /* Parity: 0=none, 1=odd, 2=even */
  uint8_t  bits;          /* Data bits: 5, 6, 7, or 8 */
  bool     stopbits2;     /* true = 2 stop bits, false = 1 stop bit */
  uint8_t  tx_sig;        /* GPIO matrix TX signal number */
  uint8_t  rx_sig;        /* GPIO matrix RX signal number */
  uint8_t  tx_gpio;       /* GPIO pin for TX */
  uint8_t  rx_gpio;       /* GPIO pin for RX */
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/* Serial driver lower-half operations (uart_ops_s interface) */

static int  esp32p4_uart_setup(struct uart_dev_s *dev);
static void esp32p4_uart_shutdown(struct uart_dev_s *dev);
static int  esp32p4_uart_attach(struct uart_dev_s *dev);
static void esp32p4_uart_detach(struct uart_dev_s *dev);
static int  esp32p4_uart_ioctl(struct uart_dev_s *dev, int cmd,
                                unsigned long arg);
static int  esp32p4_uart_receive(struct uart_dev_s *dev, unsigned int *status);
static void esp32p4_uart_rxint(struct uart_dev_s *dev, bool enable);
static bool esp32p4_uart_rxavailable(struct uart_dev_s *dev);
static void esp32p4_uart_send(struct uart_dev_s *dev, int ch);
static void esp32p4_uart_txint(struct uart_dev_s *dev, bool enable);
static bool esp32p4_uart_txready(struct uart_dev_s *dev);
static bool esp32p4_uart_txempty(struct uart_dev_s *dev);

/* Internal helper functions */

static void esp32p4_uart_configure(struct esp32p4_uart_port_s *port);
static void esp32p4_uart_sw_reset(struct esp32p4_uart_port_s *port);
static void esp32p4_uart_config_gpio(struct esp32p4_uart_port_s *port);
static void esp32p4_uart_set_baudrate(struct esp32p4_uart_port_s *port);

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* UART0 port descriptor */

#ifdef CONFIG_ESP32P4_UART0
static struct esp32p4_uart_port_s g_uart0_port =
{
  .base      = DR_REG_UART0_BASE,
  .irq       = ESP32P4_IRQ_UART0,
  .baud      = CONFIG_UART0_BAUD,
  .clk_freq  = APB_CLK_FREQ_DEFAULT,
  .parity    = CONFIG_ESP32P4_UART0_PARITY,
  .bits      = CONFIG_ESP32P4_UART0_BITS,
  .stopbits2 = CONFIG_ESP32P4_UART0_2STOP,
  .tx_sig    = UART0_TX_SIG,
  .rx_sig    = UART0_RX_SIG,
  .tx_gpio   = CONFIG_ESP32P4_UART0_TX_GPIO,
  .rx_gpio   = CONFIG_ESP32P4_UART0_RX_GPIO,
};

static const struct uart_ops_s g_uart0_ops =
{
  .setup       = esp32p4_uart_setup,
  .shutdown    = esp32p4_uart_shutdown,
  .attach      = esp32p4_uart_attach,
  .detach      = esp32p4_uart_detach,
  .ioctl       = esp32p4_uart_ioctl,
  .receive     = esp32p4_uart_receive,
  .rxint       = esp32p4_uart_rxint,
  .rxavailable = esp32p4_uart_rxavailable,
#ifdef CONFIG_SERIAL_IFLOWCONTROL
  .rxflowcontrol = NULL,
#endif
  .send        = esp32p4_uart_send,
  .txint       = esp32p4_uart_txint,
  .txready     = esp32p4_uart_txready,
  .txempty     = esp32p4_uart_txempty,
};

static char g_uart0_rxbuffer[CONFIG_UART0_RXBUFSIZE];
static char g_uart0_txbuffer[CONFIG_UART0_TXBUFSIZE];

static struct uart_dev_s g_uart0_priv =
{
  .isconsole = (bool)CONFIG_UART0_SERIAL_CONSOLE,
  .ops       = &g_uart0_ops,
  .xmit =
  {
    .size    = CONFIG_UART0_TXBUFSIZE,
    .buffer  = g_uart0_txbuffer,
  },
  .recv =
  {
    .size    = CONFIG_UART0_RXBUFSIZE,
    .buffer  = g_uart0_rxbuffer,
  },
  .priv      = &g_uart0_port,
};
#endif /* CONFIG_ESP32P4_UART0 */

/* UART1 port descriptor */

#ifdef CONFIG_ESP32P4_UART1
static struct esp32p4_uart_port_s g_uart1_port =
{
  .base      = DR_REG_UART1_BASE,
  .irq       = ESP32P4_IRQ_UART1,
  .baud      = CONFIG_UART1_BAUD,
  .clk_freq  = APB_CLK_FREQ_DEFAULT,
  .parity    = CONFIG_ESP32P4_UART1_PARITY,
  .bits      = CONFIG_ESP32P4_UART1_BITS,
  .stopbits2 = CONFIG_ESP32P4_UART1_2STOP,
  .tx_sig    = UART1_TX_SIG,
  .rx_sig    = UART1_RX_SIG,
  .tx_gpio   = CONFIG_ESP32P4_UART1_TX_GPIO,
  .rx_gpio   = CONFIG_ESP32P4_UART1_RX_GPIO,
};

static const struct uart_ops_s g_uart1_ops =
{
  .setup       = esp32p4_uart_setup,
  .shutdown    = esp32p4_uart_shutdown,
  .attach      = esp32p4_uart_attach,
  .detach      = esp32p4_uart_detach,
  .ioctl       = esp32p4_uart_ioctl,
  .receive     = esp32p4_uart_receive,
  .rxint       = esp32p4_uart_rxint,
  .rxavailable = esp32p4_uart_rxavailable,
#ifdef CONFIG_SERIAL_IFLOWCONTROL
  .rxflowcontrol = NULL,
#endif
  .send        = esp32p4_uart_send,
  .txint       = esp32p4_uart_txint,
  .txready     = esp32p4_uart_txready,
  .txempty     = esp32p4_uart_txempty,
};

static char g_uart1_rxbuffer[CONFIG_UART1_RXBUFSIZE];
static char g_uart1_txbuffer[CONFIG_UART1_TXBUFSIZE];

static struct uart_dev_s g_uart1_priv =
{
  .isconsole = false,
  .ops       = &g_uart1_ops,
  .xmit =
  {
    .size    = CONFIG_UART1_TXBUFSIZE,
    .buffer  = g_uart1_txbuffer,
  },
  .recv =
  {
    .size    = CONFIG_UART1_RXBUFSIZE,
    .buffer  = g_uart1_rxbuffer,
  },
  .priv      = &g_uart1_port,
};
#endif /* CONFIG_ESP32P4_UART1 */

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: esp32p4_uart_sw_reset
 *
 * Description:
 *   Perform a software reset of the UART peripheral.
 *
 *   This resets the UART state machine, FIFOs, and configuration
 *   registers to their default values.  Modeled after the ESP-IDF
 *   uart_driver_install() sequence which resets the UART before
 *   configuration.
 *
 *   The ESP32-P4 UART does not have a dedicated software reset bit
 *   in the UART registers.  Instead, we reset through the clock and
 *   reset controller by toggling the peripheral reset bit.
 *
 ****************************************************************************/

static void esp32p4_uart_sw_reset(struct esp32p4_uart_port_s *port)
{
  /* Reset the TX and RX FIFOs by setting the reset bits in CONF0.
   * These bits are self-clearing on ESP32-P4.
   */

  REG_WRITE(port->base + UART_CONF0_REG,
            UART_TXFIFO_RST | UART_RXFIFO_RST);

  /* Clear all pending interrupts */

  REG_WRITE(port->base + UART_INT_CLR_REG, 0xffffffff);

  /* Disable all interrupts */

  REG_WRITE(port->base + UART_INT_ENA_REG, 0);
}

/****************************************************************************
 * Name: esp32p4_uart_set_baudrate
 *
 * Description:
 *   Configure the UART baud rate divisor.
 *
 *   The ESP32-P4 HP UART uses the APB clock as its source clock.
 *   The baud rate is determined by:
 *
 *     baud_rate = clk_freq / (CLKDIV_integer + CLKDIV_frag / 16)
 *
 *   The clock divider register has an integer part (20 bits) and a
 *   4-bit fractional part, giving fine-grained baud rate control.
 *   This matches the ESP-IDF uart_ll_set_baudrate() implementation.
 *
 *   Example: 115200 baud with 80 MHz APB clock:
 *     CLKDIV_full = 80000000 * 16 / 115200 = 11111.11
 *     Integer part = 11111 / 16 = 694 (0x2B6)
 *     Fractional part = 11111 % 16 = 7 (0x7)
 *
 ****************************************************************************/

static void esp32p4_uart_set_baudrate(struct esp32p4_uart_port_s *port)
{
  uint32_t clkdiv;

  /* Calculate full clock divider with 4-bit fractional precision.
   * clkdiv = (clk_freq << 4) / baud = clk_freq * 16 / baud
   * Integer part = clkdiv >> 4, fractional part = clkdiv & 0xf
   */

  clkdiv = (port->clk_freq << 4) / port->baud;

  /* Write the integer clock divider (20 bits) */

  REG_WRITE(port->base + UART_CLKDIV_REG, (clkdiv >> 4) & 0xfffff);

  /* Write the fractional clock divider (4 bits) */

  REG_WRITE(port->base + UART_CLKDIV_FRAG_REG, clkdiv & 0xf);
}

/****************************************************************************
 * Name: esp32p4_uart_config_gpio
 *
 * Description:
 *   Configure GPIO pins for UART TX and RX via the GPIO matrix.
 *
 *   The ESP32-P4 uses a GPIO matrix that allows any peripheral signal
 *   to be routed to any GPIO pin.  This function sets up the pin mux
 *   for the UART TX (output) and RX (input) signals.
 *
 *   This is modeled after ESP-IDF's uart_set_pin() and the GPIO
 *   configuration done in uart_driver_install().
 *
 *   TX pin configuration:
 *     - Set GPIO_FUNC_OUT_SEL_CFG_REG(pin) to route the UART TX
 *       signal to the pin
 *     - Configure pin as output
 *
 *   RX pin configuration:
 *     - Set GPIO_FUNC_IN_SEL_CFG_REG(signal) to route the pin to
 *       the UART RX input
 *     - Configure pin as input with pull-up enabled
 *
 ****************************************************************************/

static void esp32p4_uart_config_gpio(struct esp32p4_uart_port_s *port)
{
  uint32_t regval;

  /* Configure TX pin: route UART TX signal to GPIO output */

  regval = port->tx_sig & GPIO_FUNC_OUT_SEL_M;
  REG_WRITE(GPIO_FUNC_OUT_SEL_CFG_REG(port->tx_gpio), regval);

  /* Configure RX pin: route GPIO input to UART RX signal.
   * For input routing, we write the GPIO pin number to the
   * UART RX signal's input selection register.
   * Bit 7 (GPIO_FUNC_IN_SEL_HIGH) = 0 means the source is a
   * GPIO pin, not a constant.
   */

  regval = port->rx_gpio & 0x3f;
  REG_WRITE(GPIO_FUNC_IN_SEL_CFG_REG(port->rx_sig), regval);

  /* Configure TX pin as output (function 1 = GPIO output mode) */

  regval = REG_READ(GPIO_PIN_REG(port->tx_gpio));
  regval &= ~(0x3);   /* Clear bits [1:0] (direction) */
  regval |= GPIO_OUTPUT;
  REG_WRITE(GPIO_PIN_REG(port->tx_gpio), regval);

  /* Configure RX pin as input with pull-up to prevent floating */

  regval = REG_READ(GPIO_PIN_REG(port->rx_gpio));
  regval &= ~(0x3);   /* Clear bits [1:0] (direction) */
  regval |= GPIO_INPUT_PULLUP;
  REG_WRITE(GPIO_PIN_REG(port->rx_gpio), regval);
}

/****************************************************************************
 * Name: esp32p4_uart_configure
 *
 * Description:
 *   Configure the UART hardware with the specified parameters.
 *   This sets the baud rate, data bits, stop bits, parity, and
 *   FIFO thresholds.
 *
 *   The configuration sequence follows the ESP-IDF uart_param_config()
 *   pattern:
 *     1. Set clock divider for baud rate
 *     2. Configure data format in CONF0 (data bits, stop bits, parity)
 *     3. Configure FIFO thresholds and timeouts in CONF1
 *
 ****************************************************************************/

static void esp32p4_uart_configure(struct esp32p4_uart_port_s *port)
{
  uint32_t conf0;
  uint32_t conf1;

  /* Step 1: Set baud rate clock divider */

  esp32p4_uart_set_baudrate(port);

  /* Step 2: Reset FIFOs before configuration */

  REG_WRITE(port->base + UART_CONF0_REG,
            UART_TXFIFO_RST | UART_RXFIFO_RST);

  /* Step 3: Build CONF0 value with data format settings.
   * We build the entire register value and write it once to avoid
   * glitches from intermediate states.
   */

  conf0 = 0;

  /* Configure data bits (bits [3:2]) */

  switch (port->bits)
    {
      case 5:
        conf0 |= UART_BIT_NUM_5;
        break;
      case 6:
        conf0 |= UART_BIT_NUM_6;
        break;
      case 7:
        conf0 |= UART_BIT_NUM_7;
        break;
      case 8:
      default:
        conf0 |= UART_BIT_NUM_8;
        break;
    }

  /* Configure stop bits (bits [5:4]) */

  if (port->stopbits2)
    {
      conf0 |= UART_STOP_BIT_NUM_2;
    }
  else
    {
      conf0 |= UART_STOP_BIT_NUM_1;
    }

  /* Configure parity (bits [1:0])
   * PARITY bit selects even (0) or odd (1) parity.
   * PARITY_EN bit enables parity check.
   */

  if (port->parity != 0)
    {
      conf0 |= UART_PARITY_EN;
      if (port->parity == 1)  /* Odd parity */
        {
          conf0 |= UART_PARITY;
        }
    }

  REG_WRITE(port->base + UART_CONF0_REG, conf0);

  /* Step 4: Configure CONF1 register with FIFO thresholds and timeouts.
   *
   * RXFIFO_FULL_THRHD [bits 22:16] (7 bits): RX FIFO threshold for
   *   generating the RXFIFO_FULL interrupt.  We use 112 bytes
   *   (matching ESP-IDF default) to reduce interrupt overhead while
   *   still responding quickly.
   *
   * TXFIFO_EMPTY_THRHD [bits 29:23] (7 bits): TX FIFO threshold for
   *   generating the TXFIFO_EMPTY interrupt.  When the TX FIFO level
   *   drops below this threshold, the interrupt fires.  We use 8
   *   bytes to trigger the interrupt early enough to keep the TX
   *   FIFO fed.
   *
   * RX_TOUT_EN [6]: Enable RX FIFO timeout.  When enabled, an
   *   interrupt fires after RX_TOUT_THRHD idle bit periods with no
   *   new data.  This is essential for receiving data that doesn't
   *   fill the FIFO (e.g., interactive terminal input).
   *
   * RX_TOUT_THRHD [17:7] (11 bits): RX timeout threshold in UART
   *   symbol periods.  We use 2 (minimum useful value).
   */

  conf1 = (UART_RXFIFO_FULL_THRHD << UART_RXFIFO_FULL_THRHD_S) |
          (UART_TXFIFO_EMPTY_THRHD << UART_TXFIFO_EMPTY_THRHD_S) |
          UART_RX_TOUT_EN |
          (UART_RX_TOUT_THRHD << UART_RX_TOUT_THRHD_S);

  REG_WRITE(port->base + UART_CONF1_REG, conf1);

  /* Step 5: Configure idle character detection for robustness.
   *
   * The IDLE_CONF_REG controls the number of idle bit periods
   * before the UART considers the line idle.  This interacts with
   * the RX timeout mechanism.
   *   Pre-idle: 1 bit period before data
   *   Post-idle: 1 bit period after data
   *   Gap: 1 bit period between characters
   * These are reasonable defaults for most baud rates.
   */

  REG_WRITE(port->base + UART_IDLE_CONF_REG, (1 << 10) | (1 << 4) | 1);
}

/****************************************************************************
 * Name: esp32p4_uart_setup
 *
 * Description:
 *   Configure the UART.  This method is called the first time that the
 *   serial port is opened.
 *
 *   The setup sequence follows the ESP-IDF uart_driver_install() pattern:
 *     1. Software reset the UART module
 *     2. Configure GPIO pins for TX/RX
 *     3. Configure UART parameters (baud, format, FIFOs)
 *     4. Enable the UART receiver and transmitter
 *
 ****************************************************************************/

static int esp32p4_uart_setup(struct uart_dev_s *dev)
{
  struct esp32p4_uart_port_s *port = dev->priv;

  /* Step 1: Software reset to ensure clean state */

  esp32p4_uart_sw_reset(port);

  /* Step 2: Configure GPIO pins for UART TX/RX */

  esp32p4_uart_config_gpio(port);

  /* Step 3: Configure UART hardware (baud, format, FIFOs) */

  esp32p4_uart_configure(port);

  /* Step 4: Enable UART transmitter and receiver.
   * These bits in CONF0 enable the TX and RX state machines.
   * We read-modify-write to preserve the format settings we just wrote.
   */

  modifyreg32(port->base + UART_CONF0_REG, 0,
               (1 << 27) |  /* UART_TXFIFO_RST_CLR - ensure TX enabled */
               (1 << 28));  /* UART_RXFIFO_RST_CLR - ensure RX enabled */

  return OK;
}

/****************************************************************************
 * Name: esp32p4_uart_shutdown
 *
 * Description:
 *   Disable the UART.  This method is called when the serial port is
 *   closed.
 *
 *   Following the ESP-IDF uart_driver_delete() pattern:
 *     1. Disable all UART interrupts
 *     2. Reset the UART module to clean state
 *
 ****************************************************************************/

static void esp32p4_uart_shutdown(struct uart_dev_s *dev)
{
  struct esp32p4_uart_port_s *port = dev->priv;

  /* Disable all UART interrupts */

  REG_WRITE(port->base + UART_INT_ENA_REG, 0);

  /* Clear any pending interrupts */

  REG_WRITE(port->base + UART_INT_CLR_REG, 0xffffffff);

  /* Reset the UART FIFOs */

  REG_WRITE(port->base + UART_CONF0_REG,
            UART_TXFIFO_RST | UART_RXFIFO_RST);
}

/****************************************************************************
 * Name: esp32p4_uart_interrupt
 *
 * Description:
 *   UART interrupt handler.  This is called from the PLIC interrupt
 *   dispatch when a UART interrupt is detected.
 *
 *   The handler processes the following interrupt sources:
 *     - RXFIFO_FULL: RX FIFO has reached the full threshold
 *     - RXFIFO_TOUT: RX FIFO timeout (idle line detected)
 *     - TXFIFO_EMPTY: TX FIFO has dropped below empty threshold
 *     - Parity error, frame error, RX FIFO overflow (cleared to
 *       prevent interrupt storms)
 *
 *   This matches the ESP-IDF uart_rx_intr_handler_default() pattern
 *   where the handler reads the masked interrupt status, processes
 *   data events, clears all interrupts, and returns.
 *
 ****************************************************************************/

static int esp32p4_uart_interrupt(int irq, void *context, void *arg)
{
  struct uart_dev_s *dev = (struct uart_dev_s *)arg;
  struct esp32p4_uart_port_s *port;
  uint32_t int_status;

  DEBUGASSERT(dev != NULL && dev->priv != NULL);
  port = dev->priv;

  /* Read the masked interrupt status register.
   * This only shows interrupts that are both pending AND enabled.
   */

  int_status = REG_READ(port->base + UART_INT_ST_REG);

  /* Handle RX data available: both FIFO-full and timeout events.
   *
   * The RX timeout interrupt is particularly important for
   * interactive terminal use: it fires when data stops arriving
   * (e.g., after a line of input), even if the FIFO isn't full.
   * This matches ESP-IDF's handling in uart_rx_intr_handler_default()
   * where both UART_RXFIFO_FULL_INT_ST and UART_RXFIFO_TOUT_INT_ST
   * trigger uart_recvchars().
   */

  if (int_status & (UART_RXFIFO_FULL_INT_RAW | UART_RXFIFO_TOUT_INT_RAW))
    {
      uart_recvchars(dev);
    }

  /* Handle TX FIFO empty.
   *
   * When the TX FIFO level drops below the empty threshold, we
   * refill it from the software transmit buffer via uart_xmitchars().
   */

  if (int_status & UART_TXFIFO_EMPTY_INT_RAW)
    {
      uart_xmitchars(dev);
    }

  /* Handle error conditions.
   *
   * We clear error interrupt flags to prevent interrupt storms.
   * The actual error reporting happens in esp32p4_uart_receive()
   * which checks the STATUS register for error flags.
   */

  if (int_status & (UART_PARITY_ERR_INT_RAW |
                     UART_FRM_ERR_INT_RAW |
                     UART_RXFIFO_OVF_INT_RAW))
    {
      /* Log the error for debugging */

      _err("UART%d error: status=0x%08" PRIx32 "\n",
           (port->base == DR_REG_UART0_BASE) ? 0 : 1,
           int_status & (UART_PARITY_ERR_INT_RAW |
                         UART_FRM_ERR_INT_RAW |
                         UART_RXFIFO_OVF_INT_RAW));
    }

  /* Clear all pending interrupts by writing to the clear register.
   * This acknowledges all interrupt sources at once, which is the
   * recommended pattern for the ESP32-P4 UART.
   */

  REG_WRITE(port->base + UART_INT_CLR_REG, int_status);

  return OK;
}

/****************************************************************************
 * Name: esp32p4_uart_attach
 *
 * Description:
 *   Configure the UART to interrupt on RX data available.
 *   This registers the interrupt handler with the PLIC and is
 *   called when the serial port is opened.
 *
 ****************************************************************************/

static int esp32p4_uart_attach(struct uart_dev_s *dev)
{
  struct esp32p4_uart_port_s *port = dev->priv;

  /* Attach the UART interrupt handler to the PLIC */

  return irq_attach(port->irq, esp32p4_uart_interrupt, dev);
}

/****************************************************************************
 * Name: esp32p4_uart_detach
 *
 * Description:
 *   Detach the UART interrupt handler.
 *   This is called when the serial port is closed.
 *
 ****************************************************************************/

static void esp32p4_uart_detach(struct uart_dev_s *dev)
{
  struct esp32p4_uart_port_s *port = dev->priv;

  /* Disable all UART interrupts */

  REG_WRITE(port->base + UART_INT_ENA_REG, 0);

  /* Detach the interrupt handler from the PLIC */

  irq_attach(port->irq, NULL, NULL);
}

/****************************************************************************
 * Name: esp32p4_uart_ioctl
 *
 * Description:
 *   All ioctl calls will be routed through this method.
 *
 *   Supported commands:
 *     TCGETS - Get terminal attributes (data bits, stop bits, parity)
 *     TCSETS - Set terminal attributes (data bits, stop bits, parity,
 *              baud rate via CBAUD/CBAUDEX)
 *
 ****************************************************************************/

static int esp32p4_uart_ioctl(struct uart_dev_s *dev, int cmd,
                               unsigned long arg)
{
  struct esp32p4_uart_port_s *port = dev->priv;

  switch (cmd)
    {
      case TCGETS:
        {
          struct termios *termiosp = (struct termios *)(uintptr_t)arg;

          if (termiosp == NULL)
            {
              return -EINVAL;
            }

          memset(termiosp, 0, sizeof(struct termios));

          /* Data bits */

          switch (port->bits)
            {
              case 5:
                termiosp->c_cflag |= CS5;
                break;
              case 6:
                termiosp->c_cflag |= CS6;
                break;
              case 7:
                termiosp->c_cflag |= CS7;
                break;
              case 8:
              default:
                termiosp->c_cflag |= CS8;
                break;
            }

          /* Stop bits */

          if (port->stopbits2)
            {
              termiosp->c_cflag |= CSTOPB;
            }

          /* Parity */

          if (port->parity != 0)
            {
              termiosp->c_cflag |= PARENB;
              if (port->parity == 1)  /* Odd parity */
                {
                  termiosp->c_cflag |= PARODD;
                }
            }

          /* Baud rate - store in c_ispeed/c_ospeed */

          termiosp->c_ispeed = port->baud;
          termiosp->c_ospeed = port->baud;

          /* Input flags - enable basic processing */

          termiosp->c_iflag = 0;

          /* Output flags */

          termiosp->c_oflag = 0;

          /* Local flags */

          termiosp->c_lflag = 0;

          return OK;
        }

      case TCSETS:
        {
          struct termios *termiosp = (struct termios *)(uintptr_t)arg;
          bool need_reconfig = false;

          if (termiosp == NULL)
            {
              return -EINVAL;
            }

          /* Data bits */

          switch (termiosp->c_cflag & CSIZE)
            {
              case CS5:
                if (port->bits != 5)
                  {
                    port->bits = 5;
                    need_reconfig = true;
                  }
                break;
              case CS6:
                if (port->bits != 6)
                  {
                    port->bits = 6;
                    need_reconfig = true;
                  }
                break;
              case CS7:
                if (port->bits != 7)
                  {
                    port->bits = 7;
                    need_reconfig = true;
                  }
                break;
              case CS8:
              default:
                if (port->bits != 8)
                  {
                    port->bits = 8;
                    need_reconfig = true;
                  }
                break;
            }

          /* Stop bits */

          {
            bool stopbits2 = (termiosp->c_cflag & CSTOPB) ? true : false;
            if (port->stopbits2 != stopbits2)
              {
                port->stopbits2 = stopbits2;
                need_reconfig = true;
              }
          }

          /* Parity */

          {
            uint8_t newparity;
            if (termiosp->c_cflag & PARENB)
              {
                newparity = (termiosp->c_cflag & PARODD) ? 1 : 2;
              }
            else
              {
                newparity = 0;
              }

            if (port->parity != newparity)
              {
                port->parity = newparity;
                need_reconfig = true;
              }
          }

          /* Baud rate - check if the caller is requesting a change.
           * We accept baud rate through c_ispeed/c_ospeed fields.
           * If either is non-zero and differs from the current baud
           * rate, we update the port and reconfigure.
           */

          if (termiosp->c_ospeed != 0 && termiosp->c_ospeed != port->baud)
            {
              port->baud = termiosp->c_ospeed;
              need_reconfig = true;
            }

          /* Reconfigure hardware if settings changed.
           * We reconfigure the entire UART to ensure all settings
           * are applied consistently, matching the ESP-IDF pattern
           * of calling uart_param_config() which reconfigures
           * everything at once.
           */

          if (need_reconfig)
            {
              esp32p4_uart_configure(port);
            }

          return OK;
        }

      default:
        return -ENOTTY;
    }
}

/****************************************************************************
 * Name: esp32p4_uart_receive
 *
 * Description:
 *   Called (usually) from the interrupt handler to receive one byte from
 *   the UART data register.
 *
 *   The status parameter returns error flags from the UART status
 *   register, which the upper-half serial driver uses to report
 *   errors to the application:
 *     - Parity error
 *     - Framing error
 *     - RX FIFO overflow
 *
 *   This matches the ESP-IDF uart_read_bytes() pattern where error
 *   flags are checked alongside the received data.
 *
 ****************************************************************************/

static int esp32p4_uart_receive(struct uart_dev_s *dev, unsigned int *status)
{
  struct esp32p4_uart_port_s *port = dev->priv;
  uint32_t regval;

  /* Read the UART status register for error detection.
   * The status register contains:
   *   Bits [15:0]  : RX FIFO count
   *   Bits [23:16] : TX FIFO count
   *   Bit  [30]    : RX FIFO overflow flag (sticky until cleared)
   *
   * We also check the raw interrupt register for parity and framing
   * errors, as these are latched there.
   */

  *status = 0;

  regval = REG_READ(port->base + UART_INT_RAW_REG);

  if (regval & UART_PARITY_ERR_INT_RAW)
    {
      *status |= UART_PARITY_ERR_INT_RAW;
    }

  if (regval & UART_FRM_ERR_INT_RAW)
    {
      *status |= UART_FRM_ERR_INT_RAW;
    }

  if (regval & UART_RXFIFO_OVF_INT_RAW)
    {
      *status |= UART_RXFIFO_OVF_INT_RAW;
    }

  /* Read the byte from the FIFO register.
   * The FIFO register is 8 bits wide for data.  Reading from this
   * register pops one byte from the RX FIFO.
   */

  regval = REG_READ(port->base + UART_FIFO_REG);

  return (int)(regval & 0xff);
}

/****************************************************************************
 * Name: esp32p4_uart_rxint
 *
 * Description:
 *   Call to enable or disable RX interrupts.
 *
 *   When enabled, the following interrupt sources are activated:
 *     - RXFIFO_FULL: fires when RX FIFO reaches the full threshold
 *     - RXFIFO_TOUT: fires after idle timeout with data in FIFO
 *
 *   The RX timeout is critical for interactive use: it ensures that
 *   data is delivered even when the line has fewer bytes than the
 *   FIFO threshold.  This matches the ESP-IDF pattern of enabling
 *   both interrupts in uart_driver_install().
 *
 ****************************************************************************/

static void esp32p4_uart_rxint(struct uart_dev_s *dev, bool enable)
{
  struct esp32p4_uart_port_s *port = dev->priv;

  if (enable)
    {
      /* Enable RX FIFO full and RX timeout interrupts.
       * REG_SET_BIT uses a critical section internally for
       * atomic read-modify-write.
       */

      REG_SET_BIT(port->base + UART_INT_ENA_REG,
                  UART_RXFIFO_FULL_INT_RAW | UART_RXFIFO_TOUT_INT_RAW);
    }
  else
    {
      /* Disable RX FIFO full and RX timeout interrupts */

      REG_CLR_BIT(port->base + UART_INT_ENA_REG,
                  UART_RXFIFO_FULL_INT_RAW | UART_RXFIFO_TOUT_INT_RAW);
    }
}

/****************************************************************************
 * Name: esp32p4_uart_rxavailable
 *
 * Description:
 *   Return true if the receive FIFO is not empty.
 *
 *   The RX FIFO count is in bits [15:0] of the STATUS register.
 *   This matches the ESP-IDF uart_ll_get_rxfifo_len() which reads
 *   the same field.
 *
 ****************************************************************************/

static bool esp32p4_uart_rxavailable(struct uart_dev_s *dev)
{
  struct esp32p4_uart_port_s *port = dev->priv;
  uint32_t status;

  /* Check the RX FIFO count from the status register */

  status = REG_READ(port->base + UART_STATUS_REG);

  /* RX FIFO count is in bits [15:0] of the status register.
   * If non-zero, data is available.
   */

  return ((status & 0xffff) != 0);
}

/****************************************************************************
 * Name: esp32p4_uart_send
 *
 * Description:
 *   This method will send one byte on the UART.
 *
 *   The caller (uart_xmitchars) has already verified that the TX FIFO
 *   is not full via txready() before calling this function.
 *
 ****************************************************************************/

static void esp32p4_uart_send(struct uart_dev_s *dev, int ch)
{
  struct esp32p4_uart_port_s *port = dev->priv;

  /* Write the character to the TX FIFO.
   * The FIFO register is 8 bits wide; writing pushes one byte
   * into the TX FIFO.
   */

  REG_WRITE(port->base + UART_FIFO_REG, (uint32_t)(ch & 0xff));
}

/****************************************************************************
 * Name: esp32p4_uart_txint
 *
 * Description:
 *   Call to enable or disable TX interrupts.
 *
 *   When enabled, the TXFIFO_EMPTY interrupt fires when the TX FIFO
 *   level drops below the empty threshold (configured in CONF1).
 *   This triggers uart_xmitchars() to refill the FIFO from the
 *   software transmit buffer.
 *
 ****************************************************************************/

static void esp32p4_uart_txint(struct uart_dev_s *dev, bool enable)
{
  struct esp32p4_uart_port_s *port = dev->priv;

  if (enable)
    {
      REG_SET_BIT(port->base + UART_INT_ENA_REG,
                  UART_TXFIFO_EMPTY_INT_RAW);
    }
  else
    {
      REG_CLR_BIT(port->base + UART_INT_ENA_REG,
                  UART_TXFIFO_EMPTY_INT_RAW);
    }
}

/****************************************************************************
 * Name: esp32p4_uart_txready
 *
 * Description:
 *   Return true if the transmit FIFO is not full.
 *
 *   The TX FIFO count is in bits [23:16] of the STATUS register.
 *   The FIFO is 128 bytes deep.  If the count is less than the
 *   maximum, we can accept more data.
 *
 ****************************************************************************/

static bool esp32p4_uart_txready(struct uart_dev_s *dev)
{
  struct esp32p4_uart_port_s *port = dev->priv;
  uint32_t status;

  /* Check the TX FIFO count from the status register */

  status = REG_READ(port->base + UART_STATUS_REG);

  /* TX FIFO count is in bits [23:16].
   * The FIFO is UART_FIFO_MAX (128) bytes deep; if not full,
   * we can send.
   */

  return ((status >> 16) & 0xff) < UART_FIFO_MAX;
}

/****************************************************************************
 * Name: esp32p4_uart_txempty
 *
 * Description:
 *   Return true if the transmit FIFO is empty.
 *
 *   This is used by the upper half to determine when it is safe to
 *   shut down the UART (all data has been transmitted).
 *
 ****************************************************************************/

static bool esp32p4_uart_txempty(struct uart_dev_s *dev)
{
  struct esp32p4_uart_port_s *port = dev->priv;
  uint32_t status;

  /* Check if TX FIFO is empty */

  status = REG_READ(port->base + UART_STATUS_REG);

  return ((status >> 16) & 0xff) == 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: riscv_earlyserialinit
 *
 * Description:
 *   Performs the low level UART initialization early in debug so that the
 *   serial console will be available during bootup.  This must be called
 *   before riscv_serialinit.
 *
 *   The early console was already set up by the boot ROM for basic
 *   debug output.  We configure the driver structures here so they
 *   are ready when the full serial subsystem comes up.
 *
 ****************************************************************************/

void riscv_earlyserialinit(void)
{
  /* NOTE: All function pointers are already assigned in the static
   * structure initializers above.  The boot ROM has already configured
   * UART0 at 115200 baud for early debug output.
   */
}

/****************************************************************************
 * Name: riscv_serialinit
 *
 * Description:
 *   Register serial console and serial ports.  This assumes that
 *   riscv_earlyserialinit was called previously.
 *
 ****************************************************************************/

void riscv_serialinit(void)
{
  /* Register UART0 as console */

#ifdef CONFIG_ESP32P4_UART0
  uart_register("/dev/console", &g_uart0_priv);
#endif

  /* Register UART1 if configured */

#ifdef CONFIG_ESP32P4_UART1
  uart_register("/dev/ttyS1", &g_uart1_priv);
#endif
}

/****************************************************************************
 * Name: up_putc
 *
 * Description:
 *   Output a byte on the serial console.
 *
 *   This function sends a character directly to the UART0 TX FIFO,
 *   waiting for space to become available.  It also handles the
 *   CR-LF conversion: a newline character ('\n') is preceded by
 *   a carriage return ('\r') to produce proper line endings on
 *   serial terminals.
 *
 ****************************************************************************/

int up_putc(int ch)
{
#ifdef CONFIG_UART0_SERIAL_CONSOLE
  uint32_t status;

  /* For newline, send CR first (standard serial terminal convention) */

  if (ch == '\n')
    {
      /* Wait for TX FIFO to have space */

      do
        {
          status = REG_READ(DR_REG_UART0_BASE + UART_STATUS_REG);
        }
      while (((status >> 16) & 0xff) >= UART_FIFO_MAX);

      REG_WRITE(DR_REG_UART0_BASE + UART_FIFO_REG, (uint32_t)'\r');
    }

  /* Wait for TX FIFO to have space */

  do
    {
      status = REG_READ(DR_REG_UART0_BASE + UART_STATUS_REG);
    }
  while (((status >> 16) & 0xff) >= UART_FIFO_MAX);

  /* Write the character */

  REG_WRITE(DR_REG_UART0_BASE + UART_FIFO_REG, (uint32_t)(ch & 0xff));
#endif

  return ch;
}

/****************************************************************************
 * Name: up_lowputc
 *
 * Description:
 *   Output a byte on the serial console (low-level, no interrupts).
 *   This is used for early boot debug output before the serial
 *   subsystem is initialized.
 *
 ****************************************************************************/

void up_lowputc(char ch)
{
#ifdef CONFIG_UART0_SERIAL_CONSOLE
  uint32_t status;

  /* For newline, send CR first (standard serial terminal convention) */

  if (ch == '\n')
    {
      do
        {
          status = REG_READ(DR_REG_UART0_BASE + UART_STATUS_REG);
        }
      while (((status >> 16) & 0xff) >= UART_FIFO_MAX);

      REG_WRITE(DR_REG_UART0_BASE + UART_FIFO_REG, (uint32_t)'\r');
    }

  /* Wait for TX FIFO to have space */

  do
    {
      status = REG_READ(DR_REG_UART0_BASE + UART_STATUS_REG);
    }
  while (((status >> 16) & 0xff) >= UART_FIFO_MAX);

  /* Write the character */

  REG_WRITE(DR_REG_UART0_BASE + UART_FIFO_REG, (uint32_t)(ch & 0xff));
#endif
}
