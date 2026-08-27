/****************************************************************************
 * vendor_esp32p4/chips/esp32p4/include/esp32p4_wifi.h
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

#ifndef __VENDOR_ESP32P4_CHIPS_ESP32P4_INCLUDE_ESP32P4_WIFI_H
#define __VENDOR_ESP32P4_CHIPS_ESP32P4_INCLUDE_ESP32P4_WIFI_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdint.h>
#include <stdbool.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* ESP-Hosted protocol version */

#define ESP_HOSTED_VERSION_MAJOR        1
#define ESP_HOSTED_VERSION_MINOR        0

/* ESP-Hosted frame types */

#define ESP_HOSTED_FRAME_TYPE_DATA      0x01
#define ESP_HOSTED_FRAME_TYPE_CMD       0x02
#define ESP_HOSTED_FRAME_TYPE_RESP      0x03
#define ESP_HOSTED_FRAME_TYPE_EVENT     0x04

/* ESP-Hosted interface types */

#define ESP_HOSTED_IFACE_STA            0x00
#define ESP_HOSTED_IFACE_SOFTAP         0x01
#define ESP_HOSTED_IFACE_MAX            0x02

/* ESP-Hosted command IDs */

#define ESP_HOSTED_CMD_BASE             0x00
#define ESP_HOSTED_CMD_GET_MAC          (ESP_HOSTED_CMD_BASE + 0x00)
#define ESP_HOSTED_CMD_SCAN             (ESP_HOSTED_CMD_BASE + 0x01)
#define ESP_HOSTED_CMD_CONNECT          (ESP_HOSTED_CMD_BASE + 0x02)
#define ESP_HOSTED_CMD_DISCONNECT       (ESP_HOSTED_CMD_BASE + 0x03)
#define ESP_HOSTED_CMD_GET_RSSI         (ESP_HOSTED_CMD_BASE + 0x04)
#define ESP_HOSTED_CMD_SET_MODE         (ESP_HOSTED_CMD_BASE + 0x05)
#define ESP_HOSTED_CMD_GET_MODE         (ESP_HOSTED_CMD_BASE + 0x06)
#define ESP_HOSTED_CMD_SOFTAP_START     (ESP_HOSTED_CMD_BASE + 0x07)
#define ESP_HOSTED_CMD_SOFTAP_STOP      (ESP_HOSTED_CMD_BASE + 0x08)
#define ESP_HOSTED_CMD_SET_TX_POWER     (ESP_HOSTED_CMD_BASE + 0x09)
#define ESP_HOSTED_CMD_GET_TX_POWER     (ESP_HOSTED_CMD_BASE + 0x0a)
#define ESP_HOSTED_CMD_MAX              (ESP_HOSTED_CMD_BASE + 0x0b)

/* ESP-Hosted event IDs */

#define ESP_HOSTED_EVT_BASE             0x80
#define ESP_HOSTED_EVT_STA_CONNECTED    (ESP_HOSTED_EVT_BASE + 0x00)
#define ESP_HOSTED_EVT_STA_DISCONNECTED (ESP_HOSTED_EVT_BASE + 0x01)
#define ESP_HOSTED_EVT_AP_STA_CONNECTED (ESP_HOSTED_EVT_BASE + 0x02)
#define ESP_HOSTED_EVT_AP_STA_DISCONNECTED (ESP_HOSTED_EVT_BASE + 0x03)
#define ESP_HOSTED_EVT_SCAN_DONE        (ESP_HOSTED_EVT_BASE + 0x04)
#define ESP_HOSTED_EVT_MAX              (ESP_HOSTED_EVT_BASE + 0x05)

/* ESP-Hosted status codes */

#define ESP_HOSTED_STATUS_SUCCESS       0x00
#define ESP_HOSTED_STATUS_FAILURE       0x01
#define ESP_HOSTED_STATUS_TIMEOUT       0x02
#define ESP_HOSTED_STATUS_INVALID_PARAM 0x03
#define ESP_HOSTED_STATUS_NO_MEM        0x04
#define ESP_HOSTED_STATUS_BUSY          0x05

/* ESP-Hosted WiFi auth modes */

#define ESP_HOSTED_AUTH_OPEN            0x00
#define ESP_HOSTED_AUTH_WEP             0x01
#define ESP_HOSTED_AUTH_WPA_PSK         0x02
#define ESP_HOSTED_AUTH_WPA2_PSK        0x03
#define ESP_HOSTED_AUTH_WPA_WPA2_PSK    0x04
#define ESP_HOSTED_AUTH_WPA3_PSK        0x05
#define ESP_HOSTED_AUTH_WPA2_WPA3_PSK   0x06

/* ESP-Hosted WiFi cipher types */

#define ESP_HOSTED_CIPHER_NONE          0x00
#define ESP_HOSTED_CIPHER_WEP40         0x01
#define ESP_HOSTED_CIPHER_WEP104        0x02
#define ESP_HOSTED_CIPHER_TKIP          0x03
#define ESP_HOSTED_CIPHER_CCMP          0x04
#define ESP_HOSTED_CIPHER_TKIP_CCMP     0x05

/* Maximum sizes */

#define ESP_HOSTED_MAX_SSID_LEN         32
#define ESP_HOSTED_MAX_PASSWORD_LEN     64
#define ESP_HOSTED_MAX_SCAN_RESULTS     32
#define ESP_HOSTED_MAC_ADDR_LEN         6
#define ESP_HOSTED_MAX_FRAME_SIZE       1600
#define ESP_HOSTED_MAX_CMD_SIZE         512
#define ESP_HOSTED_MAX_RESP_SIZE        1600

/* SDIO communication parameters */

#define ESP_HOSTED_SDIO_BLOCK_SIZE      512
#define ESP_HOSTED_SDIO_FREQ_HZ         20000000  /* 20 MHz SDIO clock */

/* Command timeout (milliseconds) */

#define ESP_HOSTED_CMD_TIMEOUT_MS       5000
#define ESP_HOSTED_SCAN_TIMEOUT_MS      15000
#define ESP_HOSTED_CONNECT_TIMEOUT_MS   20000

/* Network device name */

#define ESP_HOSTED_NETDEV_NAME          "wlan0"

/* TX power range (dBm) */

#define ESP_HOSTED_TX_POWER_MIN         2
#define ESP_HOSTED_TX_POWER_MAX         20

/****************************************************************************
 * Public Types
 ****************************************************************************/

/* ESP-Hosted frame header (prepended to all SDIO frames) */

struct esp_hosted_frame_hdr_s
{
  uint8_t  type;          /* Frame type (DATA, CMD, RESP, EVENT) */
  uint8_t  iface;         /* Interface (STA=0, SOFTAP=1) */
  uint8_t  seq;           /* Sequence number */
  uint8_t  flags;         /* Flags (bit0: encrypted, bit1: more data) */
  uint16_t payload_len;   /* Payload length in bytes */
  uint16_t reserved;      /* Reserved / padding */
};

#define ESP_HOSTED_FRAME_HDR_SIZE   sizeof(struct esp_hosted_frame_hdr_s)

/* ESP-Hosted command header */

struct esp_hosted_cmd_hdr_s
{
  uint8_t  cmd_id;        /* Command ID */
  uint8_t  status;        /* Status (in response) */
  uint16_t seq;           /* Command sequence number */
};

/* WiFi scan result entry */

struct esp_hosted_scan_result_s
{
  uint8_t  bssid[ESP_HOSTED_MAC_ADDR_LEN];
  int8_t   rssi;
  uint8_t  authmode;
  uint8_t  channel;
  uint8_t  cipher;
  uint8_t  reserved[2];
  char     ssid[ESP_HOSTED_MAX_SSID_LEN + 1];
};

/* WiFi scan command payload */

struct esp_hosted_scan_cmd_s
{
  uint8_t  show_hidden;   /* Show hidden APs */
  uint8_t  reserved[3];
};

/* WiFi scan response payload */

struct esp_hosted_scan_resp_s
{
  uint8_t  num_results;
  uint8_t  reserved[3];
  struct esp_hosted_scan_result_s results[0];  /* Flexible array */
};

/* WiFi connect command payload */

struct esp_hosted_connect_cmd_s
{
  char     ssid[ESP_HOSTED_MAX_SSID_LEN];
  char     password[ESP_HOSTED_MAX_PASSWORD_LEN];
  uint8_t  bssid[ESP_HOSTED_MAC_ADDR_LEN];
  uint8_t  authmode;
  uint8_t  channel;       /* 0 = auto */
  uint8_t  reserved[2];
};

/* WiFi disconnect command payload */

struct esp_hosted_disconnect_cmd_s
{
  uint8_t  reserved[4];
};

/* WiFi mode command payload */

struct esp_hosted_mode_cmd_s
{
  uint8_t  mode;          /* 0=NONE, 1=STA, 2=SOFTAP, 3=STA+SOFTAP */
  uint8_t  reserved[3];
};

/* SoftAP start command payload */

struct esp_hosted_softap_start_cmd_s
{
  char     ssid[ESP_HOSTED_MAX_SSID_LEN];
  char     password[ESP_HOSTED_MAX_PASSWORD_LEN];
  uint8_t  channel;
  uint8_t  authmode;
  uint8_t  max_connections;
  uint8_t  ssid_hidden;
  uint16_t beacon_interval;
  uint8_t  reserved[2];
};

/* MAC address response payload */

struct esp_hosted_mac_resp_s
{
  uint8_t  mac[ESP_HOSTED_MAC_ADDR_LEN];
  uint8_t  reserved[2];
};

/* RSSI response payload */

struct esp_hosted_rssi_resp_s
{
  int8_t   rssi;
  uint8_t  reserved[3];
};

/* TX power command/response payload */

struct esp_hosted_tx_power_s
{
  int8_t   power_dbm;
  uint8_t  reserved[3];
};

/* WiFi event data - STA connected */

struct esp_hosted_evt_sta_conn_s
{
  uint8_t  bssid[ESP_HOSTED_MAC_ADDR_LEN];
  uint8_t  reserved[2];
};

/* WiFi event data - STA disconnected */

struct esp_hosted_evt_sta_disconn_s
{
  uint8_t  reason;
  uint8_t  reserved[3];
};

/* WiFi event data - AP station connected/disconnected */

struct esp_hosted_evt_ap_sta_s
{
  uint8_t  mac[ESP_HOSTED_MAC_ADDR_LEN];
  uint8_t  aid;           /* Association ID */
  uint8_t  reserved;
};

/* WiFi event payload (union) */

union esp_hosted_event_data_u
{
  struct esp_hosted_evt_sta_conn_s    sta_conn;
  struct esp_hosted_evt_sta_disconn_s sta_disconn;
  struct esp_hosted_evt_ap_sta_s      ap_sta;
};

/* WiFi mode constants */

enum esp_hosted_wifi_mode_e
{
  ESP_HOSTED_WIFI_MODE_NULL = 0,
  ESP_HOSTED_WIFI_MODE_STA  = 1,
  ESP_HOSTED_WIFI_MODE_AP   = 2,
  ESP_HOSTED_WIFI_MODE_AP_STA = 3
};

/* Driver state */

enum esp_hosted_state_e
{
  ESP_HOSTED_STATE_UNINIT = 0,
  ESP_HOSTED_STATE_SDIO_READY,
  ESP_HOSTED_STATE_FW_READY,
  ESP_HOSTED_STATE_STA_DISCONNECTED,
  ESP_HOSTED_STATE_STA_CONNECTING,
  ESP_HOSTED_STATE_STA_CONNECTED,
  ESP_HOSTED_STATE_AP_ACTIVE
};

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Name: esp_wifi_initialize
 *
 * Description:
 *   Initialize the ESP-Hosted WiFi driver. This function:
 *   1. Initializes SDIO communication with ESP32-C6
 *   2. Performs firmware handshake
 *   3. Registers network device (wlan0)
 *
 * Input Parameters:
 *   None
 *
 * Returned Value:
 *   OK on success; negative errno on failure.
 *
 ****************************************************************************/

int esp_wifi_initialize(void);

/****************************************************************************
 * Name: esp_wifi_deinitialize
 *
 * Description:
 *   Deinitialize the ESP-Hosted WiFi driver and release resources.
 *
 * Returned Value:
 *   OK on success; negative errno on failure.
 *
 ****************************************************************************/

int esp_wifi_deinitialize(void);

/****************************************************************************
 * Name: esp_wifi_scan
 *
 * Description:
 *   Perform WiFi scan and return results.
 *
 * Input Parameters:
 *   results     - Buffer to store scan results
 *   max_results - Maximum number of results to store
 *   num_results - Pointer to store actual number of results found
 *
 * Returned Value:
 *   OK on success; negative errno on failure.
 *
 ****************************************************************************/

int esp_wifi_scan(struct esp_hosted_scan_result_s *results,
                  uint8_t max_results, uint8_t *num_results);

/****************************************************************************
 * Name: esp_wifi_connect
 *
 * Description:
 *   Connect to a WiFi network.
 *
 * Input Parameters:
 *   ssid     - Network SSID
 *   password - Network password (NULL for open networks)
 *
 * Returned Value:
 *   OK on success; negative errno on failure.
 *
 ****************************************************************************/

int esp_wifi_connect(const char *ssid, const char *password);

/****************************************************************************
 * Name: esp_wifi_disconnect
 *
 * Description:
 *   Disconnect from the current WiFi network.
 *
 * Returned Value:
 *   OK on success; negative errno on failure.
 *
 ****************************************************************************/

int esp_wifi_disconnect(void);

/****************************************************************************
 * Name: esp_wifi_softap_start
 *
 * Description:
 *   Start a soft AP.
 *
 * Input Parameters:
 *   ssid           - AP SSID
 *   password       - AP password (NULL for open AP)
 *   channel        - WiFi channel (1-13, 0=auto)
 *   max_connections - Maximum number of stations
 *
 * Returned Value:
 *   OK on success; negative errno on failure.
 *
 ****************************************************************************/

int esp_wifi_softap_start(const char *ssid, const char *password,
                          uint8_t channel, uint8_t max_connections);

/****************************************************************************
 * Name: esp_wifi_softap_stop
 *
 * Description:
 *   Stop the soft AP.
 *
 * Returned Value:
 *   OK on success; negative errno on failure.
 *
 ****************************************************************************/

int esp_wifi_softap_stop(void);

/****************************************************************************
 * Name: esp_wifi_get_rssi
 *
 * Description:
 *   Get the current RSSI of the connected AP.
 *
 * Input Parameters:
 *   rssi - Pointer to store the RSSI value
 *
 * Returned Value:
 *   OK on success; negative errno on failure.
 *
 ****************************************************************************/

int esp_wifi_get_rssi(int8_t *rssi);

/****************************************************************************
 * Name: esp_wifi_get_mac
 *
 * Description:
 *   Get the MAC address of the specified interface.
 *
 * Input Parameters:
 *   iface - Interface (ESP_HOSTED_IFACE_STA or ESP_HOSTED_IFACE_SOFTAP)
 *   mac   - Buffer to store the MAC address (6 bytes)
 *
 * Returned Value:
 *   OK on success; negative errno on failure.
 *
 ****************************************************************************/

int esp_wifi_get_mac(uint8_t iface, uint8_t *mac);

/****************************************************************************
 * Name: esp_wifi_set_tx_power
 *
 * Description:
 *   Set the WiFi TX power.
 *
 * Input Parameters:
 *   power_dbm - TX power in dBm (2-20)
 *
 * Returned Value:
 *   OK on success; negative errno on failure.
 *
 ****************************************************************************/

int esp_wifi_set_tx_power(int8_t power_dbm);

/****************************************************************************
 * Name: esp_wifi_get_tx_power
 *
 * Description:
 *   Get the current WiFi TX power.
 *
 * Input Parameters:
 *   power_dbm - Pointer to store the TX power in dBm
 *
 * Returned Value:
 *   OK on success; negative errno on failure.
 *
 ****************************************************************************/

int esp_wifi_get_tx_power(int8_t *power_dbm);

#endif /* __VENDOR_ESP32P4_CHIPS_ESP32P4_INCLUDE_ESP32P4_WIFI_H */
