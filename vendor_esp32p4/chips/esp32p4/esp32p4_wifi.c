/****************************************************************************
 * vendor_esp32p4/chips/esp32p4/esp32p4_wifi.c
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
 * ESP-Hosted WiFi Driver for ESP32-P4 (NuttX)
 *
 * This driver implements WiFi connectivity for the ESP32-P4 SoC via the
 * ESP-Hosted protocol.  The ESP32-P4 has no built-in WiFi radio; instead,
 * an ESP32-C6 co-processor runs the WiFi firmware and communicates with
 * the ESP32-P4 host over SDIO.
 *
 * Architecture:
 *
 *   +------------------+       SDIO        +------------------+
 *   |    ESP32-P4      |<=================>|    ESP32-C6      |
 *   |  (NuttX Host)    |   4-bit SDIO bus  |  (WiFi FW Slave) |
 *   +------------------+                   +------------------+
 *   |  esp32p4_wifi.c  |                   |  ESP-Hosted FW   |
 *   |  - SDIO transport|                   |  - WiFi stack    |
 *   |  - ESP-Hosted    |                   |  - SDIO slave    |
 *   |    protocol      |                   |  - Command proc  |
 *   |  - NuttX netdev  |                   |                  |
 *   +------------------+                   +------------------+
 *
 * FreeRTOS to NuttX API Mapping:
 *   xTaskCreate        ->  nxtask_create / kthread_create
 *   xSemaphoreTake     ->  nxsem_wait / nxmutex_lock
 *   xSemaphoreGive     ->  nxsem_post / nxmutex_unlock
 *   vTaskDelay         ->  nxsig_usleep / up_mdelay
 *   xQueueSend         ->  nxsem_post (binary sem)
 *   xQueueReceive      ->  nxsem_wait (binary sem)
 *   pvPortMalloc       ->  kmm_malloc
 *   vPortFree          ->  kmm_free
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <debug.h>
#include <time.h>

#include <nuttx/arch.h>
#include <nuttx/kmalloc.h>
#include <nuttx/wqueue.h>
#include <nuttx/mutex.h>
#include <nuttx/semaphore.h>
#include <nuttx/net/netdev.h>
#include <nuttx/net/ethernet.h>
#include <nuttx/net/net.h>
#include <nuttx/sdio.h>
#include <nuttx/irq.h>
#include <nuttx/wireless/wireless.h>

#include "esp32p4_wifi.h"
#include "esp32p4_sdmmc.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define WIFI_TAG "esp32p4_wifi"

/* Debug levels */

#ifdef CONFIG_DEBUG_WIRELESS_ERROR
#  define wifierr(fmt, ...)   _err(fmt, ##__VA_ARGS__)
#else
#  define wifierr(fmt, ...)
#endif

#ifdef CONFIG_DEBUG_WIRELESS_WARN
#  define wifiwarn(fmt, ...)  _warn(fmt, ##__VA_ARGS__)
#else
#  define wifiwarn(fmt, ...)
#endif

#ifdef CONFIG_DEBUG_WIRELESS_INFO
#  define wifiinfo(fmt, ...)  _info(fmt, ##__VA_ARGS__)
#else
#  define wifiinfo(fmt, ...)
#endif

/* Work queue for WiFi event processing */

#define ESP_WIFI_WORK_LP         0  /* Low priority work queue */
#define ESP_WIFI_WORK_THREADNAME "esp_wifi_wq"

/* Sequence number increment */

#define SEQ_NEXT(s)              ((uint8_t)(((s) + 1) & 0xff))

/* SDIO function number for ESP32-C6 WiFi data */

#define ESP_HOSTED_SDIO_FUNC     1

/* Interrupt handling */

#define ESP_WIFI_POLL_INTERVAL   50  /* ms - polling SDIO for data */

/* TX/RX buffer management */

#define ESP_WIFI_TX_BUFFERS      4
#define ESP_WIFI_RX_BUFFERS      8
#define ESP_WIFI_TXQ_DEPTH       4

/****************************************************************************
 * Private Types
 ****************************************************************************/

/* Driver operating mode */

enum esp_wifi_mode_e
{
  ESP_WIFI_MODE_OFF = 0,
  ESP_WIFI_MODE_STA,
  ESP_WIFI_MODE_AP,
  ESP_WIFI_MODE_STA_AP
};

/* Pending command state */

struct esp_wifi_cmd_ctx_s
{
  sem_t        sem;             /* Completion semaphore */
  uint8_t      cmd_id;          /* Outstanding command ID */
  uint16_t     seq;             /* Command sequence number */
  int          status;          /* Result status */
  uint8_t     *resp_buf;        /* Response buffer (points to caller buf) */
  uint16_t     resp_buf_len;    /* Response buffer size */
  uint16_t     resp_data_len;   /* Actual response data length */
  bool         pending;         /* True if a command is in flight */
};

/* Network device private data */

struct esp_wifi_priv_s
{
  /* NuttX net device -- must be first */

  struct net_driver_s       dev;

  /* Driver state */

  enum esp_hosted_state_e   state;
  enum esp_wifi_mode_e      mode;

  /* Mutex protecting the driver */

  mutex_t                   lock;

  /* Command context */

  struct esp_wifi_cmd_ctx_s cmd;

  /* SDIO device handle */

  struct sdio_dev_s        *sdio;

  /* Sequence numbers */

  uint8_t                   tx_seq;
  uint8_t                   rx_seq;

  /* MAC addresses */

  uint8_t                   sta_mac[ESP_HOSTED_MAC_ADDR_LEN];
  uint8_t                   ap_mac[ESP_HOSTED_MAC_ADDR_LEN];

  /* Scan results */

  struct esp_hosted_scan_result_s
                            *scan_results;
  uint8_t                   scan_count;
  sem_t                     scan_sem;

  /* Connection state */

  bool                      sta_connected;
  int8_t                    rssi;

  /* Work queues for deferred processing */

  struct work_s             poll_work;
  struct work_s             event_work;

  /* TX buffer management */

  uint8_t                  *tx_buf;
  size_t                    tx_buf_size;

  /* RX buffer */

  uint8_t                  *rx_buf;
  size_t                    rx_buf_size;

  /* Statistics */

  uint32_t                  tx_packets;
  uint32_t                  rx_packets;
  uint32_t                  tx_errors;
  uint32_t                  rx_errors;
  uint32_t                  tx_dropped;
  uint32_t                  cmd_timeouts;

  /* Firmware version info */

  uint8_t                   fw_version_major;
  uint8_t                   fw_version_minor;
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/* SDIO transport layer */

static int  esp_wifi_sdio_init(struct esp_wifi_priv_s *priv);
static int  esp_wifi_sdio_send_frame(struct esp_wifi_priv_s *priv,
                uint8_t type, uint8_t iface, const uint8_t *payload,
                uint16_t payload_len);
static int  esp_wifi_sdio_recv_frame(struct esp_wifi_priv_s *priv,
                uint8_t *buf, uint16_t buf_size, uint16_t *frame_len,
                uint32_t timeout_ms);
static int  esp_wifi_sdio_read_reg(struct esp_wifi_priv_s *priv,
                uint8_t reg, uint8_t *value);
static int  esp_wifi_sdio_write_reg(struct esp_wifi_priv_s *priv,
                uint8_t reg, uint8_t value);

/* ESP-Hosted protocol layer */

static int  esp_wifi_fw_handshake(struct esp_wifi_priv_s *priv);
static int  esp_wifi_send_command(struct esp_wifi_priv_s *priv,
                uint8_t cmd_id, const uint8_t *cmd_payload,
                uint16_t cmd_len, uint8_t *resp_payload,
                uint16_t resp_buf_len, uint16_t *resp_data_len,
                uint32_t timeout_ms);
static void esp_wifi_process_event(struct esp_wifi_priv_s *priv,
                const uint8_t *payload, uint16_t len);
static void esp_wifi_process_response(struct esp_wifi_priv_s *priv,
                const uint8_t *payload, uint16_t len);

/* NuttX netdev interface */

static int  esp_wifi_ifup(struct net_driver_s *dev);
static int  esp_wifi_ifdown(struct net_driver_s *dev);
static int  esp_wifi_transmit(struct net_driver_s *dev);
static int  esp_wifi_txpoll(struct net_driver_s *dev);
#ifdef CONFIG_NET_MCASTGROUP
static int  esp_wifi_addmac(struct net_driver_s *dev,
                const uint8_t *mac);
static int  esp_wifi_rmmac(struct net_driver_s *dev,
                const uint8_t *mac);
#endif
#ifdef CONFIG_NETDEV_IOCTL
static int  esp_wifi_ioctl(struct net_driver_s *dev, int cmd,
                unsigned long arg);
#endif

/* Work queue handlers */

static void esp_wifi_poll_worker(void *arg);
static void esp_wifi_event_worker(void *arg);

/* Utility functions */

static int  esp_wifi_wait_for_connection(struct esp_wifi_priv_s *priv,
                uint32_t timeout_ms);
static void esp_wifi_update_link_status(struct esp_wifi_priv_s *priv,
                bool up);

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* Singleton driver instance */

static struct esp_wifi_priv_s g_esp_wifi;

/* Net device operations */

static const struct netdev_ops_s g_esp_wifi_ops =
{
  .ifup     = esp_wifi_ifup,
  .ifdown   = esp_wifi_ifdown,
  .transmit = esp_wifi_transmit,
  .txpoll   = esp_wifi_txpoll,
#ifdef CONFIG_NET_MCASTGROUP
  .addmac   = esp_wifi_addmac,
  .rmmac    = esp_wifi_rmmac,
#endif
#ifdef CONFIG_NETDEV_IOCTL
  .ioctl    = esp_wifi_ioctl,
#endif
};

/****************************************************************************
 * Private Functions - SDIO Transport Layer
 ****************************************************************************/

/****************************************************************************
 * Name: esp_wifi_sdio_init
 *
 * Description:
 *   Initialize the SDIO interface for communication with ESP32-C6.
 *
 ****************************************************************************/

static int esp_wifi_sdio_init(struct esp_wifi_priv_s *priv)
{
  struct sdio_dev_s *sdio;
  int ret;

  wifiinfo("Initializing SDIO for ESP-Hosted\n");

  /* Initialize the SDMMC controller in SDIO mode */

  sdio = sdmmc_initialize(0);
  if (sdio == NULL)
    {
      wifierr("ERROR: Failed to initialize SDMMC controller\n");
      return -ENODEV;
    }

  priv->sdio = sdio;

  /* Select SDIO function 1 (ESP32-C6 WiFi) */

  sdio->lock(sdio, true);

  /* Set SDIO bus clock to initialization frequency (400 kHz) */

  ret = sdio->sendcmd(sdio, SDIO_CMD5, 0);
  if (ret < 0)
    {
      wifierr("ERROR: SDIO CMD5 (IO_SEND_OP_COND) failed: %d\n", ret);
      sdio->lock(sdio, false);
      return ret;
    }

  /* Wait for SDIO card to be ready */

  uint32_t ocr;
  int retries = 100;
  do
    {
      ret = sdio->sendcmd(sdio, SDIO_CMD5, 0x00ff8000);
      if (ret < 0)
        {
          up_mdelay(10);
          retries--;
          continue;
        }

      ret = sdio->recvshortocr(sdio, &ocr);
      if (ret < 0 || retries <= 0)
        {
          wifierr("ERROR: SDIO OCR negotiation failed: %d\n", ret);
          sdio->lock(sdio, false);
          return ret < 0 ? ret : -ETIMEDOUT;
        }

      if (ocr & (1 << 31))
        {
          break;  /* Card is ready */
        }

      up_mdelay(10);
      retries--;
    }
  while (retries > 0);

  /* Set relative address (CMD3) */

  ret = sdio->sendcmd(sdio, SDIO_CMD3, 0);
  if (ret < 0)
    {
      wifierr("ERROR: SDIO CMD3 failed: %d\n", ret);
      sdio->lock(sdio, false);
      return ret;
    }

  /* Select the card (CMD7) */

  ret = sdio->sendcmd(sdio, SDIO_CMD7, 0);
  if (ret < 0)
    {
      wifierr("ERROR: SDIO CMD7 failed: %d\n", ret);
      sdio->lock(sdio, false);
      return ret;
    }

  /* Set bus width to 4-bit for higher throughput */

  sdio_set_bus_width(4);

  /* Increase clock to operational frequency */

  sdio_set_clock(CONFIG_ESP32P4_WIFI_SDIO_FREQ);

  sdio->lock(sdio, false);

  wifiinfo("SDIO initialized successfully\n");

  return OK;
}

/****************************************************************************
 * Name: esp_wifi_sdio_read_reg
 *
 * Description:
 *   Read a single byte from an SDIO register.
 *
 ****************************************************************************/

static int esp_wifi_sdio_read_reg(struct esp_wifi_priv_s *priv,
                                  uint8_t reg, uint8_t *value)
{
  struct sdio_dev_s *sdio = priv->sdio;
  uint32_t arg;
  uint32_t resp;
  int ret;

  /* SDIO CMD52 read: arg = 0 | (func << 28) | (raw << 27) | (reg << 9) */

  arg = (ESP_HOSTED_SDIO_FUNC << 28) | (reg << 9);

  sdio->lock(sdio, true);

  ret = sdio->sendcmd(sdio, SDIO_CMD52, arg);
  if (ret < 0)
    {
      sdio->lock(sdio, false);
      return ret;
    }

  ret = sdio->recvshort(sdio, &resp);
  if (ret < 0)
    {
      sdio->lock(sdio, false);
      return ret;
    }

  /* Response data is in bits [7:0] of the R5 response */

  *value = (uint8_t)(resp & 0xff);

  sdio->lock(sdio, false);

  return OK;
}

/****************************************************************************
 * Name: esp_wifi_sdio_write_reg
 *
 * Description:
 *   Write a single byte to an SDIO register.
 *
 ****************************************************************************/

static int esp_wifi_sdio_write_reg(struct esp_wifi_priv_s *priv,
                                   uint8_t reg, uint8_t value)
{
  struct sdio_dev_s *sdio = priv->sdio;
  uint32_t arg;
  int ret;

  /* SDIO CMD52 write: arg = 1 | (func << 28) | (reg << 9) | value */

  arg = (1 << 31) | (ESP_HOSTED_SDIO_FUNC << 28) | (reg << 9) | value;

  sdio->lock(sdio, true);

  ret = sdio->sendcmd(sdio, SDIO_CMD52, arg);
  if (ret < 0)
    {
      sdio->lock(sdio, false);
      return ret;
    }

  sdio->lock(sdio, false);

  return OK;
}

/****************************************************************************
 * Name: esp_wifi_sdio_send_frame
 *
 * Description:
 *   Send a frame to ESP32-C6 over SDIO.
 *
 ****************************************************************************/

static int esp_wifi_sdio_send_frame(struct esp_wifi_priv_s *priv,
                                    uint8_t type, uint8_t iface,
                                    const uint8_t *payload,
                                    uint16_t payload_len)
{
  struct sdio_dev_s *sdio = priv->sdio;
  struct esp_hosted_frame_hdr_s hdr;
  uint16_t total_len;
  int ret;

  if (payload_len > ESP_HOSTED_MAX_FRAME_SIZE)
    {
      wifierr("ERROR: Frame too large: %u\n", payload_len);
      return -EINVAL;
    }

  /* Build frame header */

  memset(&hdr, 0, sizeof(hdr));
  hdr.type        = type;
  hdr.iface       = iface;
  hdr.seq         = priv->tx_seq;
  hdr.flags       = 0;
  hdr.payload_len = payload_len;
  hdr.reserved    = 0;

  priv->tx_seq = SEQ_NEXT(priv->tx_seq);

  /* Assemble the complete frame in TX buffer */

  total_len = ESP_HOSTED_FRAME_HDR_SIZE + payload_len;

  /* Align to SDIO block size */

  uint16_t aligned_len = (total_len + ESP_HOSTED_SDIO_BLOCK_SIZE - 1) &
                         ~(ESP_HOSTED_SDIO_BLOCK_SIZE - 1);

  if (aligned_len > priv->tx_buf_size)
    {
      wifierr("ERROR: TX buffer overflow: %u > %zu\n",
              aligned_len, priv->tx_buf_size);
      return -ENOMEM;
    }

  memcpy(priv->tx_buf, &hdr, ESP_HOSTED_FRAME_HDR_SIZE);
  if (payload_len > 0 && payload != NULL)
    {
      memcpy(priv->tx_buf + ESP_HOSTED_FRAME_HDR_SIZE,
             payload, payload_len);
    }

  /* Pad to block boundary */

  if (aligned_len > total_len)
    {
      memset(priv->tx_buf + total_len, 0, aligned_len - total_len);
    }

  /* Signal ESP32-C6 that data is ready to read */

  esp_wifi_sdio_write_reg(priv, 0x01, 0x01);

  /* Send data via SDIO block write (CMD53) */

  sdio->lock(sdio, true);

  ret = sdio->sendsetup(sdio, (const uint32_t *)priv->tx_buf,
                        aligned_len);
  if (ret < 0)
    {
      sdio->lock(sdio, false);
      wifierr("ERROR: SDIO sendsetup failed: %d\n", ret);
      priv->tx_errors++;
      return ret;
    }

  /* Issue CMD53 write: multi-block to function 1, address 0 */

  uint32_t cmd53_arg = (1 << 31) |             /* R/W = write */
                       (ESP_HOSTED_SDIO_FUNC << 28) |  /* Function */
                       (0 << 27) |             /* Block mode */
                       (0 << 26) |             /* Fixed address */
                       (0 << 9)  |             /* Address */
                       (aligned_len / ESP_HOSTED_SDIO_BLOCK_SIZE);

  ret = sdio->sendcmd(sdio, SDIO_CMD53, cmd53_arg);
  if (ret < 0)
    {
      sdio->lock(sdio, false);
      wifierr("ERROR: SDIO CMD53 write failed: %d\n", ret);
      priv->tx_errors++;
      return ret;
    }

  sdio->lock(sdio, false);

  return OK;
}

/****************************************************************************
 * Name: esp_wifi_sdio_recv_frame
 *
 * Description:
 *   Receive a frame from ESP32-C6 over SDIO.
 *
 ****************************************************************************/

static int esp_wifi_sdio_recv_frame(struct esp_wifi_priv_s *priv,
                                    uint8_t *buf, uint16_t buf_size,
                                    uint16_t *frame_len,
                                    uint32_t timeout_ms)
{
  struct sdio_dev_s *sdio = priv->sdio;
  struct esp_hosted_frame_hdr_s *hdr;
  uint32_t start_tick;
  uint32_t elapsed;
  uint8_t int_status;
  int ret;

  start_tick = clock_systime_ticks();

  /* Poll for data ready indication from ESP32-C6 */

  while (true)
    {
      /* Check if slave has data ready (interrupt status register) */

      ret = esp_wifi_sdio_read_reg(priv, 0x04, &int_status);
      if (ret < 0)
        {
          return ret;
        }

      if (int_status & 0x01)
        {
          break;  /* Data ready */
        }

      /* Check timeout */

      elapsed = TICK2MSEC(clock_systime_ticks() - start_tick);
      if (elapsed >= timeout_ms)
        {
          return -ETIMEDOUT;
        }

      up_mdelay(1);
    }

  /* Read frame via SDIO block read (CMD53) */

  uint16_t read_len = ESP_HOSTED_SDIO_BLOCK_SIZE;
  if (read_len > buf_size)
    {
      read_len = buf_size;
    }

  sdio->lock(sdio, true);

  ret = sdio->recvsetup(sdio, (uint32_t *)buf, read_len);
  if (ret < 0)
    {
      sdio->lock(sdio, false);
      wifierr("ERROR: SDIO recvsetup failed: %d\n", ret);
      return ret;
    }

  /* Issue CMD53 read: multi-block from function 1, address 0 */

  uint32_t cmd53_arg = (0 << 31) |             /* R/W = read */
                       (ESP_HOSTED_SDIO_FUNC << 28) |  /* Function */
                       (0 << 27) |             /* Block mode */
                       (0 << 26) |             /* Fixed address */
                       (0 << 9)  |             /* Address */
                       (read_len / ESP_HOSTED_SDIO_BLOCK_SIZE);

  ret = sdio->sendcmd(sdio, SDIO_CMD53, cmd53_arg);
  if (ret < 0)
    {
      sdio->lock(sdio, false);
      wifierr("ERROR: SDIO CMD53 read failed: %d\n", ret);
      return ret;
    }

  sdio->lock(sdio, false);

  /* Parse frame header */

  hdr = (struct esp_hosted_frame_hdr_s *)buf;

  /* Validate frame */

  if (hdr->payload_len > ESP_HOSTED_MAX_FRAME_SIZE)
    {
      wifierr("ERROR: Invalid frame payload length: %u\n",
              hdr->payload_len);
      return -EPROTO;
    }

  *frame_len = ESP_HOSTED_FRAME_HDR_SIZE + hdr->payload_len;

  /* Update rx sequence */

  priv->rx_seq = hdr->seq;

  return OK;
}

/****************************************************************************
 * Private Functions - ESP-Hosted Protocol Layer
 ****************************************************************************/

/****************************************************************************
 * Name: esp_wifi_fw_handshake
 *
 * Description:
 *   Perform firmware handshake with ESP32-C6 to establish communication.
 *
 ****************************************************************************/

static int esp_wifi_fw_handshake(struct esp_wifi_priv_s *priv)
{
  uint8_t resp_buf[ESP_HOSTED_MAX_RESP_SIZE];
  uint16_t resp_len;
  int ret;
  int retries = 50;

  wifiinfo("Starting firmware handshake with ESP32-C6\n");

  /* Wait for ESP32-C6 to signal readiness */

  while (retries-- > 0)
    {
      uint8_t ready;
      ret = esp_wifi_sdio_read_reg(priv, 0x00, &ready);
      if (ret == OK && (ready & 0x01))
        {
          break;
        }

      up_mdelay(100);
    }

  if (retries <= 0)
    {
      wifierr("ERROR: ESP32-C6 not responding\n");
      return -ETIMEDOUT;
    }

  /* Send a get-MAC command as a handshake check */

  ret = esp_wifi_send_command(priv, ESP_HOSTED_CMD_GET_MAC,
                              NULL, 0,
                              resp_buf, sizeof(resp_buf), &resp_len,
                              ESP_HOSTED_CMD_TIMEOUT_MS);
  if (ret < 0)
    {
      wifierr("ERROR: Handshake command failed: %d\n", ret);
      return ret;
    }

  if (resp_len >= sizeof(struct esp_hosted_mac_resp_s))
    {
      struct esp_hosted_mac_resp_s *mac_resp =
        (struct esp_hosted_mac_resp_s *)resp_buf;
      memcpy(priv->sta_mac, mac_resp->mac, ESP_HOSTED_MAC_ADDR_LEN);
      wifiinfo("STA MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
               priv->sta_mac[0], priv->sta_mac[1], priv->sta_mac[2],
               priv->sta_mac[3], priv->sta_mac[4], priv->sta_mac[5]);
    }

  priv->state = ESP_HOSTED_STATE_FW_READY;
  wifiinfo("Firmware handshake completed\n");

  return OK;
}

/****************************************************************************
 * Name: esp_wifi_send_command
 *
 * Description:
 *   Send a command to ESP32-C6 and wait for the response.
 *
 ****************************************************************************/

static int esp_wifi_send_command(struct esp_wifi_priv_s *priv,
                                 uint8_t cmd_id,
                                 const uint8_t *cmd_payload,
                                 uint16_t cmd_len,
                                 uint8_t *resp_payload,
                                 uint16_t resp_buf_len,
                                 uint16_t *resp_data_len,
                                 uint32_t timeout_ms)
{
  struct esp_wifi_cmd_ctx_s *cmd = &priv->cmd;
  uint8_t cmd_buf[ESP_HOSTED_MAX_CMD_SIZE];
  struct esp_hosted_cmd_hdr_s *cmd_hdr;
  int ret;

  /* Build command buffer: header + payload */

  if (sizeof(struct esp_hosted_cmd_hdr_s) + cmd_len >
      ESP_HOSTED_MAX_CMD_SIZE)
    {
      return -EINVAL;
    }

  cmd_hdr = (struct esp_hosted_cmd_hdr_s *)cmd_buf;
  cmd_hdr->cmd_id = cmd_id;
  cmd_hdr->status = 0;
  cmd_hdr->seq    = priv->cmd.seq;

  if (cmd_len > 0 && cmd_payload != NULL)
    {
      memcpy(cmd_buf + sizeof(struct esp_hosted_cmd_hdr_s),
             cmd_payload, cmd_len);
    }

  /* Set up command context for synchronous wait */

  nxsem_reset(&cmd->sem, 0);
  cmd->cmd_id      = cmd_id;
  cmd->seq         = priv->cmd.seq;
  cmd->status      = -ETIMEDOUT;
  cmd->resp_buf    = resp_payload;
  cmd->resp_buf_len = resp_buf_len;
  cmd->resp_data_len = 0;
  cmd->pending     = true;

  priv->cmd.seq++;

  /* Send the command frame */

  ret = esp_wifi_sdio_send_frame(priv, ESP_HOSTED_FRAME_TYPE_CMD,
                                 ESP_HOSTED_IFACE_STA,
                                 cmd_buf,
                                 sizeof(struct esp_hosted_cmd_hdr_s) +
                                 cmd_len);
  if (ret < 0)
    {
      cmd->pending = false;
      wifierr("ERROR: Failed to send command %02x: %d\n", cmd_id, ret);
      return ret;
    }

  /* Wait for response */

  ret = nxsem_tickwait(&cmd->sem, MSEC2TICK(timeout_ms));
  if (ret < 0)
    {
      cmd->pending = false;
      if (ret == -ETIMEDOUT)
        {
          wifierr("ERROR: Command %02x timed out\n", cmd_id);
          priv->cmd_timeouts++;
        }

      return ret;
    }

  /* Check command status */

  if (cmd->status != ESP_HOSTED_STATUS_SUCCESS)
    {
      wifierr("ERROR: Command %02x failed with status %d\n",
              cmd_id, cmd->status);
      return -EIO;
    }

  /* Return response data */

  if (resp_data_len != NULL)
    {
      *resp_data_len = cmd->resp_data_len;
    }

  return OK;
}

/****************************************************************************
 * Name: esp_wifi_process_response
 *
 * Description:
 *   Process a response frame from ESP32-C6.
 *
 ****************************************************************************/

static void esp_wifi_process_response(struct esp_wifi_priv_s *priv,
                                      const uint8_t *payload,
                                      uint16_t len)
{
  struct esp_wifi_cmd_ctx_s *cmd = &priv->cmd;
  const struct esp_hosted_cmd_hdr_s *resp_hdr;

  if (len < sizeof(struct esp_hosted_cmd_hdr_s))
    {
      wifierr("ERROR: Response too short: %u\n", len);
      return;
    }

  resp_hdr = (const struct esp_hosted_cmd_hdr_s *)payload;

  if (!cmd->pending)
    {
      wifiwarn("WARNING: Unexpected response, no pending command\n");
      return;
    }

  /* Verify this is the response we are waiting for */

  if (resp_hdr->cmd_id != cmd->cmd_id)
    {
      wifiwarn("WARNING: Response cmd_id mismatch: %02x != %02x\n",
               resp_hdr->cmd_id, cmd->cmd_id);
      return;
    }

  /* Copy response status and data */

  cmd->status = resp_hdr->status;

  uint16_t data_len = len - sizeof(struct esp_hosted_cmd_hdr_s);
  if (data_len > 0 && cmd->resp_buf != NULL && cmd->resp_buf_len > 0)
    {
      uint16_t copy_len = data_len;
      if (copy_len > cmd->resp_buf_len)
        {
          copy_len = cmd->resp_buf_len;
        }

      memcpy(cmd->resp_buf,
             payload + sizeof(struct esp_hosted_cmd_hdr_s),
             copy_len);
      cmd->resp_data_len = copy_len;
    }

  cmd->pending = false;

  /* Signal the waiting thread */

  nxsem_post(&cmd->sem);
}

/****************************************************************************
 * Name: esp_wifi_process_event
 *
 * Description:
 *   Process an event frame from ESP32-C6.
 *
 ****************************************************************************/

static void esp_wifi_process_event(struct esp_wifi_priv_s *priv,
                                   const uint8_t *payload,
                                   uint16_t len)
{
  uint8_t event_id;

  if (len < 1)
    {
      return;
    }

  event_id = payload[0];

  wifiinfo("Received event: 0x%02x\n", event_id);

  switch (event_id)
    {
      case ESP_HOSTED_EVT_STA_CONNECTED:
        {
          wifiinfo("STA connected to AP\n");
          priv->sta_connected = true;
          priv->state = ESP_HOSTED_STATE_STA_CONNECTED;
          esp_wifi_update_link_status(priv, true);
          break;
        }

      case ESP_HOSTED_EVT_STA_DISCONNECTED:
        {
          wifiinfo("STA disconnected from AP\n");
          priv->sta_connected = false;
          priv->state = ESP_HOSTED_STATE_STA_DISCONNECTED;
          esp_wifi_update_link_status(priv, false);
          break;
        }

      case ESP_HOSTED_EVT_AP_STA_CONNECTED:
        {
          if (len >= 1 + sizeof(struct esp_hosted_evt_ap_sta_s))
            {
              const struct esp_hosted_evt_ap_sta_s *evt =
                (const struct esp_hosted_evt_ap_sta_s *)(payload + 1);
              wifiinfo("Station connected to AP: "
                       "%02x:%02x:%02x:%02x:%02x:%02x (AID=%u)\n",
                       evt->mac[0], evt->mac[1], evt->mac[2],
                       evt->mac[3], evt->mac[4], evt->mac[5],
                       evt->aid);
            }

          break;
        }

      case ESP_HOSTED_EVT_AP_STA_DISCONNECTED:
        {
          if (len >= 1 + sizeof(struct esp_hosted_evt_ap_sta_s))
            {
              const struct esp_hosted_evt_ap_sta_s *evt =
                (const struct esp_hosted_evt_ap_sta_s *)(payload + 1);
              wifiinfo("Station disconnected from AP: "
                       "%02x:%02x:%02x:%02x:%02x:%02x\n",
                       evt->mac[0], evt->mac[1], evt->mac[2],
                       evt->mac[3], evt->mac[4], evt->mac[5]);
            }

          break;
        }

      case ESP_HOSTED_EVT_SCAN_DONE:
        {
          wifiinfo("Scan completed\n");
          nxsem_post(&priv->scan_sem);
          break;
        }

      default:
        {
          wifiwarn("WARNING: Unknown event: 0x%02x\n", event_id);
          break;
        }
    }
}

/****************************************************************************
 * Private Functions - Work Queue Handlers
 ****************************************************************************/

/****************************************************************************
 * Name: esp_wifi_poll_worker
 *
 * Description:
 *   Periodically poll SDIO for incoming frames from ESP32-C6.
 *
 ****************************************************************************/

static void esp_wifi_poll_worker(void *arg)
{
  struct esp_wifi_priv_s *priv = (struct esp_wifi_priv_s *)arg;
  uint16_t frame_len;
  struct esp_hosted_frame_hdr_s *hdr;
  int ret;

  if (priv->state < ESP_HOSTED_STATE_FW_READY)
    {
      goto reschedule;
    }

  /* Try to receive a frame */

  ret = esp_wifi_sdio_recv_frame(priv, priv->rx_buf,
                                 priv->rx_buf_size,
                                 &frame_len, 10);
  if (ret == OK && frame_len >= ESP_HOSTED_FRAME_HDR_SIZE)
    {
      hdr = (struct esp_hosted_frame_hdr_s *)priv->rx_buf;

      switch (hdr->type)
        {
          case ESP_HOSTED_FRAME_TYPE_DATA:
            {
              /* Ethernet data frame - deliver to network stack */

              priv->rx_packets++;

              if (hdr->payload_len > 0 &&
                  hdr->payload_len <= CONFIG_NET_ETH_PKTSIZE)
                {
                  net_lock();
                  memcpy(priv->dev.d_buf,
                         priv->rx_buf + ESP_HOSTED_FRAME_HDR_SIZE,
                         hdr->payload_len);
                  priv->dev.d_len = hdr->payload_len;
                  priv->dev.d_appdata = priv->dev.d_buf;
                  ipv4_input(&priv->dev);
                  net_unlock();
                }

              break;
            }

          case ESP_HOSTED_FRAME_TYPE_RESP:
            {
              esp_wifi_process_response(priv,
                priv->rx_buf + ESP_HOSTED_FRAME_HDR_SIZE,
                hdr->payload_len);
              break;
            }

          case ESP_HOSTED_FRAME_TYPE_EVENT:
            {
              esp_wifi_process_event(priv,
                priv->rx_buf + ESP_HOSTED_FRAME_HDR_SIZE,
                hdr->payload_len);
              break;
            }

          default:
            {
              wifiwarn("WARNING: Unknown frame type: 0x%02x\n",
                       hdr->type);
              break;
            }
        }
    }

reschedule:

  /* Re-schedule the poll work */

  work_queue(ESP_WIFI_WORK_LP, &priv->poll_work,
             esp_wifi_poll_worker, priv,
             MSEC2TICK(ESP_WIFI_POLL_INTERVAL));
}

/****************************************************************************
 * Private Functions - NuttX netdev Interface
 ****************************************************************************/

/****************************************************************************
 * Name: esp_wifi_ifup
 *
 * Description:
 *   Bring the WiFi network interface up.
 *
 ****************************************************************************/

static int esp_wifi_ifup(struct net_driver_s *dev)
{
  struct esp_wifi_priv_s *priv = (struct esp_wifi_priv_s *)dev;
  int ret;

  wifiinfo("Bringing wlan0 up\n");

  nxmutex_lock(&priv->lock);

  if (priv->state < ESP_HOSTED_STATE_FW_READY)
    {
      nxmutex_unlock(&priv->lock);
      wifierr("ERROR: Firmware not ready\n");
      return -ENODEV;
    }

  /* Set WiFi mode to STA */

  ret = esp_wifi_send_command(priv, ESP_HOSTED_CMD_SET_MODE,
                              (const uint8_t *)
                                &(uint8_t){ESP_HOSTED_WIFI_MODE_STA},
                              1, NULL, 0, NULL,
                              ESP_HOSTED_CMD_TIMEOUT_MS);
  if (ret < 0)
    {
      wifierr("ERROR: Failed to set WiFi mode: %d\n", ret);
      nxmutex_unlock(&priv->lock);
      return ret;
    }

  priv->mode = ESP_WIFI_MODE_STA;
  priv->state = ESP_HOSTED_STATE_STA_DISCONNECTED;

  /* Start the poll worker */

  work_queue(ESP_WIFI_WORK_LP, &priv->poll_work,
             esp_wifi_poll_worker, priv,
             MSEC2TICK(ESP_WIFI_POLL_INTERVAL));

  /* Set MAC address in net device */

  memcpy(dev->d_mac.ether.ether_addr_octet,
         priv->sta_mac, ESP_HOSTED_MAC_ADDR_LEN);

  nxmutex_unlock(&priv->lock);

  wifiinfo("wlan0 is up\n");

  return OK;
}

/****************************************************************************
 * Name: esp_wifi_ifdown
 *
 * Description:
 *   Bring the WiFi network interface down.
 *
 ****************************************************************************/

static int esp_wifi_ifdown(struct net_driver_s *dev)
{
  struct esp_wifi_priv_s *priv = (struct esp_wifi_priv_s *)dev;
  int ret;

  wifiinfo("Bringing wlan0 down\n");

  nxmutex_lock(&priv->lock);

  /* Stop poll worker */

  work_cancel(ESP_WIFI_WORK_LP, &priv->poll_work);

  /* Disconnect if connected */

  if (priv->sta_connected)
    {
      esp_wifi_send_command(priv, ESP_HOSTED_CMD_DISCONNECT,
                            NULL, 0, NULL, 0, NULL,
                            ESP_HOSTED_CMD_TIMEOUT_MS);
      priv->sta_connected = false;
    }

  /* Set mode to NULL */

  ret = esp_wifi_send_command(priv, ESP_HOSTED_CMD_SET_MODE,
                              (const uint8_t *)
                                &(uint8_t){ESP_HOSTED_WIFI_MODE_NULL},
                              1, NULL, 0, NULL,
                              ESP_HOSTED_CMD_TIMEOUT_MS);

  priv->mode = ESP_WIFI_MODE_OFF;
  priv->state = ESP_HOSTED_STATE_FW_READY;

  esp_wifi_update_link_status(priv, false);

  nxmutex_unlock(&priv->lock);

  wifiinfo("wlan0 is down\n");

  return OK;
}

/****************************************************************************
 * Name: esp_wifi_transmit
 *
 * Description:
 *   Transmit a packet through the WiFi interface.
 *
 ****************************************************************************/

static int esp_wifi_transmit(struct net_driver_s *dev)
{
  struct esp_wifi_priv_s *priv = (struct esp_wifi_priv_s *)dev;
  int ret;

  if (priv->state != ESP_HOSTED_STATE_STA_CONNECTED &&
      priv->state != ESP_HOSTED_STATE_AP_ACTIVE)
    {
      priv->tx_dropped++;
      return -ENOTCONN;
    }

  /* Send Ethernet frame as a DATA frame via SDIO */

  ret = esp_wifi_sdio_send_frame(priv, ESP_HOSTED_FRAME_TYPE_DATA,
                                 ESP_HOSTED_IFACE_STA,
                                 dev->d_buf, dev->d_len);
  if (ret < 0)
    {
      priv->tx_errors++;
      return ret;
    }

  priv->tx_packets++;

  return OK;
}

/****************************************************************************
 * Name: esp_wifi_txpoll
 *
 * Description:
 *   Poll for TX data availability.
 *
 ****************************************************************************/

static int esp_wifi_txpoll(struct net_driver_s *dev)
{
  /* Return OK to indicate we can accept more data */

  return OK;
}

#ifdef CONFIG_NET_MCASTGROUP

/****************************************************************************
 * Name: esp_wifi_addmac
 *
 * Description:
 *   Add a multicast MAC address filter.
 *
 ****************************************************************************/

static int esp_wifi_addmac(struct net_driver_s *dev,
                           const uint8_t *mac)
{
  /* ESP-Hosted handles multicast at the firmware level */

  return OK;
}

/****************************************************************************
 * Name: esp_wifi_rmmac
 *
 * Description:
 *   Remove a multicast MAC address filter.
 *
 ****************************************************************************/

static int esp_wifi_rmmac(struct net_driver_s *dev,
                          const uint8_t *mac)
{
  return OK;
}

#endif /* CONFIG_NET_MCASTGROUP */

#ifdef CONFIG_NETDEV_IOCTL

/****************************************************************************
 * Name: esp_wifi_ioctl
 *
 * Description:
 *   Handle network device ioctl commands.
 *
 ****************************************************************************/

static int esp_wifi_ioctl(struct net_driver_s *dev, int cmd,
                          unsigned long arg)
{
  struct esp_wifi_priv_s *priv = (struct esp_wifi_priv_s *)dev;
  int ret = -ENOTTY;

  switch (cmd)
    {
      /* SIOCSIWSCAN - Initiate scan */

      case SIOCSIWSCAN:
        {
          ret = esp_wifi_scan(NULL, 0, NULL);
          break;
        }

      /* SIOCGIWSCAN - Get scan results */

      case SIOCGIWSCAN:
        {
          /* Scan results are stored in priv->scan_results */

          ret = OK;
          break;
        }

      default:
        {
          wifiwarn("WARNING: Unhandled ioctl: %d\n", cmd);
          break;
        }
    }

  return ret;
}

#endif /* CONFIG_NETDEV_IOCTL */

/****************************************************************************
 * Name: esp_wifi_update_link_status
 *
 * Description:
 *   Update network link status.
 *
 ****************************************************************************/

static void esp_wifi_update_link_status(struct esp_wifi_priv_s *priv,
                                        bool up)
{
  net_lock();

  if (up)
    {
      netdev_lower_carrier_on(&priv->dev);
    }
  else
    {
      netdev_lower_carrier_off(&priv->dev);
    }

  net_unlock();
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: esp_wifi_initialize
 *
 * Description:
 *   Initialize the ESP-Hosted WiFi driver.
 *
 ****************************************************************************/

int esp_wifi_initialize(void)
{
  struct esp_wifi_priv_s *priv = &g_esp_wifi;
  int ret;

  wifiinfo("Initializing ESP-Hosted WiFi driver\n");

  /* Initialize private data */

  memset(priv, 0, sizeof(struct esp_wifi_priv_s));

  nxmutex_init(&priv->lock);
  nxsem_init(&priv->cmd.sem, 0, 0);
  nxsem_init(&priv->scan_sem, 0, 0);

  priv->state = ESP_HOSTED_STATE_UNINIT;
  priv->mode  = ESP_WIFI_MODE_OFF;

  /* Allocate TX buffer */

  priv->tx_buf_size = ESP_HOSTED_MAX_FRAME_SIZE +
                      ESP_HOSTED_SDIO_BLOCK_SIZE;
  priv->tx_buf = kmm_zalloc(priv->tx_buf_size);
  if (priv->tx_buf == NULL)
    {
      wifierr("ERROR: Failed to allocate TX buffer\n");
      ret = -ENOMEM;
      goto errout_mutex;
    }

  /* Allocate RX buffer */

  priv->rx_buf_size = ESP_HOSTED_MAX_FRAME_SIZE +
                      ESP_HOSTED_SDIO_BLOCK_SIZE;
  priv->rx_buf = kmm_zalloc(priv->rx_buf_size);
  if (priv->rx_buf == NULL)
    {
      wifierr("ERROR: Failed to allocate RX buffer\n");
      ret = -ENOMEM;
      goto errout_txbuf;
    }

  /* Allocate scan results buffer */

  priv->scan_results = kmm_zalloc(
    ESP_HOSTED_MAX_SCAN_RESULTS *
    sizeof(struct esp_hosted_scan_result_s));
  if (priv->scan_results == NULL)
    {
      wifierr("ERROR: Failed to allocate scan results buffer\n");
      ret = -ENOMEM;
      goto errout_rxbuf;
    }

  /* Initialize SDIO transport */

  ret = esp_wifi_sdio_init(priv);
  if (ret < 0)
    {
      wifierr("ERROR: SDIO init failed: %d\n", ret);
      goto errout_scan;
    }

  priv->state = ESP_HOSTED_STATE_SDIO_READY;

  /* Perform firmware handshake */

  ret = esp_wifi_fw_handshake(priv);
  if (ret < 0)
    {
      wifierr("ERROR: Firmware handshake failed: %d\n", ret);
      goto errout_scan;
    }

  /* Initialize NuttX network device */

  priv->dev.d_buf     = kmm_zalloc(CONFIG_NET_ETH_PKTSIZE);
  if (priv->dev.d_buf == NULL)
    {
      ret = -ENOMEM;
      goto errout_scan;
    }

  priv->dev.d_ifup    = esp_wifi_ifup;
  priv->dev.d_ifdown  = esp_wifi_ifdown;
  priv->dev.d_txpoll  = esp_wifi_txpoll;
  priv->dev.d_transmit = esp_wifi_transmit;
  priv->dev.d_addmac  = esp_wifi_addmac;
  priv->dev.d_rmmac   = esp_wifi_rmmac;
#ifdef CONFIG_NETDEV_IOCTL
  priv->dev.d_ioctl   = esp_wifi_ioctl;
#endif

  /* Set the hardware address */

  memcpy(priv->dev.d_mac.ether.ether_addr_octet,
         priv->sta_mac, ESP_HOSTED_MAC_ADDR_LEN);

  priv->dev.d_pktsize = CONFIG_NET_ETH_PKTSIZE;

  /* Register the network device */

  ret = netdev_register(&priv->dev, NET_LL_ETHERNET);
  if (ret < 0)
    {
      wifierr("ERROR: netdev_register failed: %d\n", ret);
      goto errout_devbuf;
    }

  wifiinfo("ESP-Hosted WiFi driver initialized successfully\n");
  wifiinfo("  Interface: %s\n", ESP_HOSTED_NETDEV_NAME);
  wifiinfo("  STA MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
           priv->sta_mac[0], priv->sta_mac[1], priv->sta_mac[2],
           priv->sta_mac[3], priv->sta_mac[4], priv->sta_mac[5]);

  return OK;

errout_devbuf:
  kmm_free(priv->dev.d_buf);
errout_scan:
  kmm_free(priv->scan_results);
errout_rxbuf:
  kmm_free(priv->rx_buf);
errout_txbuf:
  kmm_free(priv->tx_buf);
errout_mutex:
  nxmutex_destroy(&priv->lock);
  nxsem_destroy(&priv->cmd.sem);
  nxsem_destroy(&priv->scan_sem);
  return ret;
}

/****************************************************************************
 * Name: esp_wifi_deinitialize
 *
 * Description:
 *   Deinitialize the ESP-Hosted WiFi driver.
 *
 ****************************************************************************/

int esp_wifi_deinitialize(void)
{
  struct esp_wifi_priv_s *priv = &g_esp_wifi;

  wifiinfo("Deinitializing ESP-Hosted WiFi driver\n");

  nxmutex_lock(&priv->lock);

  /* Stop poll worker */

  work_cancel(ESP_WIFI_WORK_LP, &priv->poll_work);

  /* Unregister network device */

  netdev_unregister(&priv->dev);

  /* Free resources */

  if (priv->dev.d_buf != NULL)
    {
      kmm_free(priv->dev.d_buf);
    }

  if (priv->scan_results != NULL)
    {
      kmm_free(priv->scan_results);
    }

  if (priv->rx_buf != NULL)
    {
      kmm_free(priv->rx_buf);
    }

  if (priv->tx_buf != NULL)
    {
      kmm_free(priv->tx_buf);
    }

  priv->state = ESP_HOSTED_STATE_UNINIT;
  priv->mode  = ESP_WIFI_MODE_OFF;

  nxmutex_unlock(&priv->lock);

  nxmutex_destroy(&priv->lock);
  nxsem_destroy(&priv->cmd.sem);
  nxsem_destroy(&priv->scan_sem);

  wifiinfo("ESP-Hosted WiFi driver deinitialized\n");

  return OK;
}

/****************************************************************************
 * Name: esp_wifi_scan
 *
 * Description:
 *   Perform WiFi scan.
 *
 ****************************************************************************/

int esp_wifi_scan(struct esp_hosted_scan_result_s *results,
                  uint8_t max_results, uint8_t *num_results)
{
  struct esp_wifi_priv_s *priv = &g_esp_wifi;
  struct esp_hosted_scan_cmd_s scan_cmd;
  uint8_t resp_buf[ESP_HOSTED_MAX_RESP_SIZE];
  uint16_t resp_len;
  int ret;

  nxmutex_lock(&priv->lock);

  if (priv->state < ESP_HOSTED_STATE_FW_READY)
    {
      nxmutex_unlock(&priv->lock);
      return -ENODEV;
    }

  /* Reset scan semaphore */

  nxsem_reset(&priv->scan_sem, 0);
  priv->scan_count = 0;

  /* Send scan command */

  memset(&scan_cmd, 0, sizeof(scan_cmd));
  scan_cmd.show_hidden = 0;

  ret = esp_wifi_send_command(priv, ESP_HOSTED_CMD_SCAN,
                              (const uint8_t *)&scan_cmd,
                              sizeof(scan_cmd),
                              resp_buf, sizeof(resp_buf), &resp_len,
                              ESP_HOSTED_SCAN_TIMEOUT_MS);
  if (ret < 0)
    {
      wifierr("ERROR: Scan command failed: %d\n", ret);
      nxmutex_unlock(&priv->lock);
      return ret;
    }

  /* Parse scan results from response */

  if (resp_len >= sizeof(struct esp_hosted_scan_resp_s))
    {
      struct esp_hosted_scan_resp_s *scan_resp =
        (struct esp_hosted_scan_resp_s *)resp_buf;

      uint8_t count = scan_resp->num_results;
      if (count > ESP_HOSTED_MAX_SCAN_RESULTS)
        {
          count = ESP_HOSTED_MAX_SCAN_RESULTS;
        }

      priv->scan_count = count;

      /* Copy results to internal buffer and optionally to caller */

      if (resp_len >= sizeof(struct esp_hosted_scan_resp_s) +
          count * sizeof(struct esp_hosted_scan_result_s))
        {
          memcpy(priv->scan_results, scan_resp->results,
                 count * sizeof(struct esp_hosted_scan_result_s));

          if (results != NULL && max_results > 0)
            {
              uint8_t copy_count = count;
              if (copy_count > max_results)
                {
                  copy_count = max_results;
                }

              memcpy(results, scan_resp->results,
                     copy_count * sizeof(struct esp_hosted_scan_result_s));

              if (num_results != NULL)
                {
                  *num_results = copy_count;
                }
            }
        }
    }

  nxmutex_unlock(&priv->lock);

  wifiinfo("Scan completed, %u results\n", priv->scan_count);

  return OK;
}

/****************************************************************************
 * Name: esp_wifi_connect
 *
 * Description:
 *   Connect to a WiFi network.
 *
 ****************************************************************************/

int esp_wifi_connect(const char *ssid, const char *password)
{
  struct esp_wifi_priv_s *priv = &g_esp_wifi;
  struct esp_hosted_connect_cmd_s connect_cmd;
  int ret;

  if (ssid == NULL || strlen(ssid) == 0 ||
      strlen(ssid) > ESP_HOSTED_MAX_SSID_LEN)
    {
      return -EINVAL;
    }

  nxmutex_lock(&priv->lock);

  if (priv->state < ESP_HOSTED_STATE_FW_READY)
    {
      nxmutex_unlock(&priv->lock);
      return -ENODEV;
    }

  /* Build connect command */

  memset(&connect_cmd, 0, sizeof(connect_cmd));
  strncpy(connect_cmd.ssid, ssid, ESP_HOSTED_MAX_SSID_LEN);

  if (password != NULL && strlen(password) > 0)
    {
      strncpy(connect_cmd.password, password,
              ESP_HOSTED_MAX_PASSWORD_LEN);
      connect_cmd.authmode = ESP_HOSTED_AUTH_WPA2_PSK;
    }
  else
    {
      connect_cmd.authmode = ESP_HOSTED_AUTH_OPEN;
    }

  connect_cmd.channel = 0;  /* Auto */

  /* Send connect command */

  ret = esp_wifi_send_command(priv, ESP_HOSTED_CMD_CONNECT,
                              (const uint8_t *)&connect_cmd,
                              sizeof(connect_cmd),
                              NULL, 0, NULL,
                              ESP_HOSTED_CONNECT_TIMEOUT_MS);
  if (ret < 0)
    {
      wifierr("ERROR: Connect command failed: %d\n", ret);
      nxmutex_unlock(&priv->lock);
      return ret;
    }

  priv->state = ESP_HOSTED_STATE_STA_CONNECTING;

  nxmutex_unlock(&priv->lock);

  /* Wait for connection to be established */

  ret = esp_wifi_wait_for_connection(priv,
                                     ESP_HOSTED_CONNECT_TIMEOUT_MS);
  if (ret < 0)
    {
      wifierr("ERROR: Connection timed out\n");
      return ret;
    }

  wifiinfo("Connected to '%s'\n", ssid);

  return OK;
}

/****************************************************************************
 * Name: esp_wifi_disconnect
 *
 * Description:
 *   Disconnect from the current WiFi network.
 *
 ****************************************************************************/

int esp_wifi_disconnect(void)
{
  struct esp_wifi_priv_s *priv = &g_esp_wifi;
  int ret;

  nxmutex_lock(&priv->lock);

  if (priv->state < ESP_HOSTED_STATE_FW_READY)
    {
      nxmutex_unlock(&priv->lock);
      return -ENODEV;
    }

  ret = esp_wifi_send_command(priv, ESP_HOSTED_CMD_DISCONNECT,
                              NULL, 0, NULL, 0, NULL,
                              ESP_HOSTED_CMD_TIMEOUT_MS);
  if (ret < 0)
    {
      wifierr("ERROR: Disconnect command failed: %d\n", ret);
    }

  priv->sta_connected = false;
  priv->state = ESP_HOSTED_STATE_STA_DISCONNECTED;
  esp_wifi_update_link_status(priv, false);

  nxmutex_unlock(&priv->lock);

  wifiinfo("Disconnected\n");

  return OK;
}

/****************************************************************************
 * Name: esp_wifi_softap_start
 *
 * Description:
 *   Start a soft AP.
 *
 ****************************************************************************/

int esp_wifi_softap_start(const char *ssid, const char *password,
                          uint8_t channel, uint8_t max_connections)
{
  struct esp_wifi_priv_s *priv = &g_esp_wifi;
  struct esp_hosted_softap_start_cmd_s ap_cmd;
  int ret;

  if (ssid == NULL || strlen(ssid) == 0 ||
      strlen(ssid) > ESP_HOSTED_MAX_SSID_LEN)
    {
      return -EINVAL;
    }

  nxmutex_lock(&priv->lock);

  if (priv->state < ESP_HOSTED_STATE_FW_READY)
    {
      nxmutex_unlock(&priv->lock);
      return -ENODEV;
    }

  /* Build softAP start command */

  memset(&ap_cmd, 0, sizeof(ap_cmd));
  strncpy(ap_cmd.ssid, ssid, ESP_HOSTED_MAX_SSID_LEN);

  if (password != NULL && strlen(password) > 0)
    {
      strncpy(ap_cmd.password, password, ESP_HOSTED_MAX_PASSWORD_LEN);
      ap_cmd.authmode = ESP_HOSTED_AUTH_WPA2_PSK;
    }
  else
    {
      ap_cmd.authmode = ESP_HOSTED_AUTH_OPEN;
    }

  ap_cmd.channel         = channel;
  ap_cmd.max_connections = max_connections;
  ap_cmd.ssid_hidden     = 0;
  ap_cmd.beacon_interval = 100;

  /* First set mode to AP or STA+AP */

  uint8_t mode = (priv->mode == ESP_WIFI_MODE_STA) ?
                 ESP_HOSTED_WIFI_MODE_AP_STA :
                 ESP_HOSTED_WIFI_MODE_AP;

  ret = esp_wifi_send_command(priv, ESP_HOSTED_CMD_SET_MODE,
                              &mode, 1, NULL, 0, NULL,
                              ESP_HOSTED_CMD_TIMEOUT_MS);
  if (ret < 0)
    {
      wifierr("ERROR: Failed to set AP mode: %d\n", ret);
      nxmutex_unlock(&priv->lock);
      return ret;
    }

  /* Send softAP start command */

  ret = esp_wifi_send_command(priv, ESP_HOSTED_CMD_SOFTAP_START,
                              (const uint8_t *)&ap_cmd,
                              sizeof(ap_cmd),
                              NULL, 0, NULL,
                              ESP_HOSTED_CMD_TIMEOUT_MS);
  if (ret < 0)
    {
      wifierr("ERROR: SoftAP start command failed: %d\n", ret);
      nxmutex_unlock(&priv->lock);
      return ret;
    }

  if (priv->mode == ESP_WIFI_MODE_STA)
    {
      priv->mode = ESP_WIFI_MODE_STA_AP;
    }
  else
    {
      priv->mode = ESP_WIFI_MODE_AP;
    }

  priv->state = ESP_HOSTED_STATE_AP_ACTIVE;

  nxmutex_unlock(&priv->lock);

  wifiinfo("SoftAP started: SSID='%s', channel=%u\n", ssid, channel);

  return OK;
}

/****************************************************************************
 * Name: esp_wifi_softap_stop
 *
 * Description:
 *   Stop the soft AP.
 *
 ****************************************************************************/

int esp_wifi_softap_stop(void)
{
  struct esp_wifi_priv_s *priv = &g_esp_wifi;
  int ret;

  nxmutex_lock(&priv->lock);

  if (priv->state != ESP_HOSTED_STATE_AP_ACTIVE)
    {
      nxmutex_unlock(&priv->lock);
      return -EINVAL;
    }

  ret = esp_wifi_send_command(priv, ESP_HOSTED_CMD_SOFTAP_STOP,
                              NULL, 0, NULL, 0, NULL,
                              ESP_HOSTED_CMD_TIMEOUT_MS);
  if (ret < 0)
    {
      wifierr("ERROR: SoftAP stop command failed: %d\n", ret);
    }

  if (priv->mode == ESP_WIFI_MODE_STA_AP)
    {
      priv->mode = ESP_WIFI_MODE_STA;
      priv->state = priv->sta_connected ?
                    ESP_HOSTED_STATE_STA_CONNECTED :
                    ESP_HOSTED_STATE_STA_DISCONNECTED;
    }
  else
    {
      priv->mode = ESP_WIFI_MODE_OFF;
      priv->state = ESP_HOSTED_STATE_FW_READY;
    }

  nxmutex_unlock(&priv->lock);

  wifiinfo("SoftAP stopped\n");

  return OK;
}

/****************************************************************************
 * Name: esp_wifi_get_rssi
 *
 * Description:
 *   Get the current RSSI.
 *
 ****************************************************************************/

int esp_wifi_get_rssi(int8_t *rssi)
{
  struct esp_wifi_priv_s *priv = &g_esp_wifi;
  struct esp_hosted_rssi_resp_s rssi_resp;
  uint16_t resp_len;
  int ret;

  if (rssi == NULL)
    {
      return -EINVAL;
    }

  nxmutex_lock(&priv->lock);

  if (!priv->sta_connected)
    {
      nxmutex_unlock(&priv->lock);
      return -ENOTCONN;
    }

  ret = esp_wifi_send_command(priv, ESP_HOSTED_CMD_GET_RSSI,
                              NULL, 0,
                              (uint8_t *)&rssi_resp,
                              sizeof(rssi_resp), &resp_len,
                              ESP_HOSTED_CMD_TIMEOUT_MS);
  if (ret < 0)
    {
      nxmutex_unlock(&priv->lock);
      return ret;
    }

  *rssi = rssi_resp.rssi;
  priv->rssi = rssi_resp.rssi;

  nxmutex_unlock(&priv->lock);

  return OK;
}

/****************************************************************************
 * Name: esp_wifi_get_mac
 *
 * Description:
 *   Get the MAC address of the specified interface.
 *
 ****************************************************************************/

int esp_wifi_get_mac(uint8_t iface, uint8_t *mac)
{
  struct esp_wifi_priv_s *priv = &g_esp_wifi;

  if (mac == NULL || iface >= ESP_HOSTED_IFACE_MAX)
    {
      return -EINVAL;
    }

  nxmutex_lock(&priv->lock);

  if (iface == ESP_HOSTED_IFACE_STA)
    {
      memcpy(mac, priv->sta_mac, ESP_HOSTED_MAC_ADDR_LEN);
    }
  else
    {
      memcpy(mac, priv->ap_mac, ESP_HOSTED_MAC_ADDR_LEN);
    }

  nxmutex_unlock(&priv->lock);

  return OK;
}

/****************************************************************************
 * Name: esp_wifi_set_tx_power
 *
 * Description:
 *   Set the WiFi TX power.
 *
 ****************************************************************************/

int esp_wifi_set_tx_power(int8_t power_dbm)
{
  struct esp_wifi_priv_s *priv = &g_esp_wifi;
  struct esp_hosted_tx_power_s tx_power;
  int ret;

  if (power_dbm < ESP_HOSTED_TX_POWER_MIN ||
      power_dbm > ESP_HOSTED_TX_POWER_MAX)
    {
      return -EINVAL;
    }

  nxmutex_lock(&priv->lock);

  tx_power.power_dbm = power_dbm;

  ret = esp_wifi_send_command(priv, ESP_HOSTED_CMD_SET_TX_POWER,
                              (const uint8_t *)&tx_power,
                              sizeof(tx_power),
                              NULL, 0, NULL,
                              ESP_HOSTED_CMD_TIMEOUT_MS);

  nxmutex_unlock(&priv->lock);

  return ret;
}

/****************************************************************************
 * Name: esp_wifi_get_tx_power
 *
 * Description:
 *   Get the current WiFi TX power.
 *
 ****************************************************************************/

int esp_wifi_get_tx_power(int8_t *power_dbm)
{
  struct esp_wifi_priv_s *priv = &g_esp_wifi;
  struct esp_hosted_tx_power_s tx_power;
  uint16_t resp_len;
  int ret;

  if (power_dbm == NULL)
    {
      return -EINVAL;
    }

  nxmutex_lock(&priv->lock);

  ret = esp_wifi_send_command(priv, ESP_HOSTED_CMD_GET_TX_POWER,
                              NULL, 0,
                              (uint8_t *)&tx_power,
                              sizeof(tx_power), &resp_len,
                              ESP_HOSTED_CMD_TIMEOUT_MS);
  if (ret < 0)
    {
      nxmutex_unlock(&priv->lock);
      return ret;
    }

  *power_dbm = tx_power.power_dbm;

  nxmutex_unlock(&priv->lock);

  return OK;
}

/****************************************************************************
 * Name: esp_wifi_wait_for_connection
 *
 * Description:
 *   Wait for STA connection to be established.
 *
 ****************************************************************************/

static int esp_wifi_wait_for_connection(struct esp_wifi_priv_s *priv,
                                        uint32_t timeout_ms)
{
  uint32_t start_tick;
  uint32_t elapsed;

  start_tick = clock_systime_ticks();

  while (true)
    {
      if (priv->state == ESP_HOSTED_STATE_STA_CONNECTED)
        {
          return OK;
        }

      if (priv->state != ESP_HOSTED_STATE_STA_CONNECTING)
        {
          return -ECONNREFUSED;
        }

      elapsed = TICK2MSEC(clock_systime_ticks() - start_tick);
      if (elapsed >= timeout_ms)
        {
          return -ETIMEDOUT;
        }

      up_mdelay(100);
    }
}
