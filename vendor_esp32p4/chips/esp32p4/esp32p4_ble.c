/****************************************************************************
 * vendor_esp32p4/chips/esp32p4/esp32p4_ble.c
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
#include <string.h>
#include <errno.h>
#include <debug.h>

#include <nuttx/kmalloc.h>
#include <nuttx/wqueue.h>
#include <nuttx/mutex.h>

#ifdef CONFIG_ESP32P4_BLE_BLUEDROID
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_common_api.h"
#endif

#ifdef CONFIG_ESP32P4_BLE_NIMBLE
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "host/ble_gatt.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#endif

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define BLE_TAG "esp32p4_ble"

/* GATT service and characteristic UUIDs */

#define ESP32P4_BLE_SERVICE_UUID       0x00FF
#define ESP32P4_BLE_CHAR_UUID          0xFF01
#define ESP32P4_BLE_DESCR_UUID         0x3333
#define ESP32P4_BLE_NUM_HANDLES        4

/* Heart Rate Service UUIDs (standard BLE) */

#define ESP32P4_BLE_HRS_UUID           0x180D
#define ESP32P4_BLE_HRS_MEAS_UUID      0x2A37

/* Maximum characteristic value length */

#define ESP32P4_BLE_CHAR_VAL_LEN_MAX   0x40

/* Advertising parameters */

#define ESP32P4_BLE_ADV_INT_MIN        0x20
#define ESP32P4_BLE_ADV_INT_MAX        0x40

/* Scan parameters */

#define ESP32P4_BLE_SCAN_ITVL          0x50
#define ESP32P4_BLE_SCAN_WINDOW        0x30

/* Connection parameters */

#define ESP32P4_BLE_CONN_ITVL_MIN      0x10   /* 20ms */
#define ESP32P4_BLE_CONN_ITVL_MAX      0x20   /* 40ms */
#define ESP32P4_BLE_CONN_LATENCY       0
#define ESP32P4_BLE_CONN_TIMEOUT       400    /* 4s */

/* Device name */

#define ESP32P4_BLE_DEVICE_NAME        CONFIG_ESP32P4_BLE_DEVICE_NAME

/****************************************************************************
 * Private Types
 ****************************************************************************/

/* BLE driver state */

enum esp32p4_ble_state_e
{
  BLE_STATE_UNINIT = 0,       /* Not initialized */
  BLE_STATE_IDLE,             /* Initialized but not advertising */
  BLE_STATE_ADVERTISING,      /* Advertising */
  BLE_STATE_SCANNING,         /* Scanning */
  BLE_STATE_CONNECTING,       /* Initiating connection */
  BLE_STATE_CONNECTED,        /* Connected to a peer */
};

/* BLE connection info */

struct esp32p4_ble_conn_s
{
  bool     connected;
  uint16_t conn_id;
  uint8_t  remote_bda[6];
};

/* BLE driver private data */

struct esp32p4_ble_priv_s
{
  enum esp32p4_ble_state_e state;      /* Current driver state */
  struct esp32p4_ble_conn_s conn;      /* Connection info */
  mutex_t lock;                        /* Mutex for thread safety */

#ifdef CONFIG_ESP32P4_BLE_BLUEDROID
  esp_gatt_if_t gatts_if;             /* GATT server interface */
  esp_gatt_if_t gattc_if;             /* GATT client interface */
  uint16_t service_handle;            /* Service handle */
  uint16_t char_handle;               /* Characteristic handle */
  uint16_t descr_handle;              /* Descriptor handle */
  uint16_t heart_rate_handle;         /* Heart rate char handle */
  uint16_t heart_rate_descr_handle;   /* Heart rate descr handle */
#endif

#ifdef CONFIG_ESP32P4_BLE_NIMBLE
  uint16_t heart_rate_chr_val_handle; /* Heart rate char val handle */
  uint16_t heart_rate_chr_conn;       /* Heart rate subscribed conn */
  bool     heart_rate_ind_status;     /* Heart rate indication enabled */
#endif

  /* Callback functions */

  void (*rx_callback)(const uint8_t *data, uint16_t len);
  void (*scan_callback)(const uint8_t *addr, int8_t rssi,
                        const uint8_t *data, uint8_t data_len);
  void (*connect_callback)(bool connected, uint16_t conn_id);
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

#ifdef CONFIG_ESP32P4_BLE_BLUEDROID
static void esp32p4_ble_gap_event_handler(esp_gap_ble_cb_event_t event,
                                           esp_ble_gap_cb_param_t *param);
static void esp32p4_ble_gatts_event_handler(esp_gatts_cb_event_t event,
                                             esp_gatt_if_t gatts_if,
                                             esp_ble_gatts_cb_param_t *param);
static void esp32p4_ble_gatts_profile_event_handler(
    esp_gatts_cb_event_t event,
    esp_gatt_if_t gatts_if,
    esp_ble_gatts_cb_param_t *param);
#endif

#ifdef CONFIG_ESP32P4_BLE_NIMBLE
static void esp32p4_ble_nimble_host_task(void *param);
static void esp32p4_ble_on_stack_reset(int reason);
static void esp32p4_ble_on_stack_sync(void);
static int esp32p4_ble_gap_event_handler(struct ble_gap_event *event,
                                          void *arg);
static int esp32p4_ble_gatt_chr_access(uint16_t conn_handle,
                                        uint16_t attr_handle,
                                        struct ble_gatt_access_ctxt *ctxt,
                                        void *arg);
static int esp32p4_ble_heart_rate_chr_access(uint16_t conn_handle,
                                              uint16_t attr_handle,
                                              struct ble_gatt_access_ctxt *ctxt,
                                              void *arg);
static void esp32p4_ble_gatt_svr_register_cb(
    struct ble_gatt_register_ctxt *ctxt, void *arg);
static void esp32p4_ble_gatt_svr_subscribe_cb(struct ble_gap_event *event);
#endif

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct esp32p4_ble_priv_s g_ble_priv;

#ifdef CONFIG_ESP32P4_BLE_BLUEDROID
/* Advertising data */

static esp_ble_adv_data_t g_adv_data =
{
  .set_scan_rsp = false,
  .include_name = true,
  .include_txpower = false,
  .min_interval = ESP32P4_BLE_ADV_INT_MIN,
  .max_interval = ESP32P4_BLE_ADV_INT_MAX,
  .appearance = 0x00,
  .manufacturer_len = 0,
  .p_manufacturer_data = NULL,
  .service_data_len = 0,
  .p_service_data = NULL,
  .service_uuid_len = 0,
  .p_service_uuid = NULL,
  .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

/* Scan response data */

static esp_ble_adv_data_t g_scan_rsp_data =
{
  .set_scan_rsp = true,
  .include_name = true,
  .include_txpower = true,
  .appearance = 0x00,
  .manufacturer_len = 0,
  .p_manufacturer_data = NULL,
  .service_data_len = 0,
  .p_service_data = NULL,
  .service_uuid_len = 0,
  .p_service_uuid = NULL,
  .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

/* Advertising parameters */

static esp_ble_adv_params_t g_adv_params =
{
  .adv_int_min = ESP32P4_BLE_ADV_INT_MIN,
  .adv_int_max = ESP32P4_BLE_ADV_INT_MAX,
  .adv_type = ADV_TYPE_IND,
  .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
  .channel_map = ADV_CHNL_ALL,
  .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

/* Scan parameters */

static esp_ble_scan_params_t g_scan_params =
{
  .scan_type = BLE_SCAN_TYPE_ACTIVE,
  .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
  .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
  .scan_interval = ESP32P4_BLE_SCAN_ITVL,
  .scan_window = ESP32P4_BLE_SCAN_WINDOW,
  .scan_duplicate = BLE_SCAN_DUPLICATE_DISABLE,
};

/* Characteristic value */

static uint8_t g_char_val[] = {0x11, 0x22, 0x33};

static esp_attr_value_t gatts_demo_char_val =
{
  .attr_max_len = ESP32P4_BLE_CHAR_VAL_LEN_MAX,
  .attr_len = sizeof(g_char_val),
  .attr_value = g_char_val,
};

/* Heart rate measurement value */

static uint8_t g_heart_rate_val[2] = {0x00, 0x40};

static esp_attr_value_t gatts_heart_rate_val =
{
  .attr_max_len = ESP32P4_BLE_CHAR_VAL_LEN_MAX,
  .attr_len = sizeof(g_heart_rate_val),
  .attr_value = g_heart_rate_val,
};

/* Advertising configuration done flags */

static uint8_t g_adv_config_done = 0;
#define ADV_CONFIG_FLAG       (1 << 0)
#define SCAN_RSP_CONFIG_FLAG  (1 << 1)
#endif /* CONFIG_ESP32P4_BLE_BLUEDROID */

#ifdef CONFIG_ESP32P4_BLE_NIMBLE
/* Custom GATT service definition */

static const ble_uuid16_t g_svc_uuid = BLE_UUID16_INIT(ESP32P4_BLE_SERVICE_UUID);
static const ble_uuid16_t g_chr_uuid = BLE_UUID16_INIT(ESP32P4_BLE_CHAR_UUID);

static uint16_t g_chr_val_handle;

/* Heart Rate Service definition */

static const ble_uuid16_t g_hrs_svc_uuid =
    BLE_UUID16_INIT(ESP32P4_BLE_HRS_UUID);
static const ble_uuid16_t g_hrs_chr_uuid =
    BLE_UUID16_INIT(ESP32P4_BLE_HRS_MEAS_UUID);

static uint8_t g_heart_rate_val[2] = {0x00, 0x40};

static const struct ble_gatt_svc_def gatt_svr_svcs[] =
{
  /* Heart Rate Service */

  {
    .type = BLE_GATT_SVC_TYPE_PRIMARY,
    .uuid = &g_hrs_svc_uuid.u,
    .characteristics = (struct ble_gatt_chr_def[])
    {
      {
        .uuid = &g_hrs_chr_uuid.u,
        .access_cb = esp32p4_ble_heart_rate_chr_access,
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_INDICATE,
      },
      {
        0, /* No more characteristics */
      },
    },
  },

  /* Custom Service */

  {
    .type = BLE_GATT_SVC_TYPE_PRIMARY,
    .uuid = &g_svc_uuid.u,
    .characteristics = (struct ble_gatt_chr_def[])
    {
      {
        .uuid = &g_chr_uuid.u,
        .access_cb = esp32p4_ble_gatt_chr_access,
        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE |
                 BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &g_chr_val_handle,
      },
      {
        0, /* No more characteristics */
      },
    },
  },
  {
    0, /* No more services */
  },
};

/* NimBLE address */

static uint8_t g_own_addr_type;
static uint8_t g_addr_val[6] = {0};

/* Scan state */

static bool g_scan_active = false;
#endif /* CONFIG_ESP32P4_BLE_NIMBLE */

/****************************************************************************
 * Private Functions - Utility
 ****************************************************************************/

/****************************************************************************
 * Name: esp32p4_ble_format_addr
 *
 * Description:
 *   Format a BLE address into a string.
 *
 ****************************************************************************/

static void esp32p4_ble_format_addr(char *buf, size_t len,
                                     const uint8_t *addr)
{
  snprintf(buf, len, "%02X:%02X:%02X:%02X:%02X:%02X",
           addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
}

/****************************************************************************
 * Private Functions - Bluedroid
 ****************************************************************************/

#ifdef CONFIG_ESP32P4_BLE_BLUEDROID

/****************************************************************************
 * Name: esp32p4_ble_bluedroid_start_advertising
 *
 * Description:
 *   Start BLE advertising using Bluedroid stack.
 *
 ****************************************************************************/

static void esp32p4_ble_bluedroid_start_advertising(void)
{
  esp_err_t ret;

  /* Configure advertising data */

  ret = esp_ble_gap_config_adv_data(&g_adv_data);
  if (ret)
    {
      bterr("config adv data failed: %d\n", ret);
      return;
    }

  g_adv_config_done |= ADV_CONFIG_FLAG;

  /* Configure scan response data */

  ret = esp_ble_gap_config_adv_data(&g_scan_rsp_data);
  if (ret)
    {
      bterr("config scan response data failed: %d\n", ret);
      return;
    }

  g_adv_config_done |= SCAN_RSP_CONFIG_FLAG;
}

/****************************************************************************
 * Name: esp32p4_ble_gap_event_handler
 *
 * Description:
 *   GAP event handler for Bluedroid.
 *   Reference: ESP-IDF Bluedroid_GATT_Server example.
 *
 ****************************************************************************/

static void esp32p4_ble_gap_event_handler(esp_gap_ble_cb_event_t event,
                                           esp_ble_gap_cb_param_t *param)
{
  switch (event)
    {
      case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        g_adv_config_done &= (~ADV_CONFIG_FLAG);
        if (g_adv_config_done == 0)
          {
            esp_ble_gap_start_advertising(&g_adv_params);
          }
        break;

      case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
        g_adv_config_done &= (~SCAN_RSP_CONFIG_FLAG);
        if (g_adv_config_done == 0)
          {
            esp_ble_gap_start_advertising(&g_adv_params);
          }
        break;

      case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS)
          {
            bterr("advertising start failed: %d\n",
                  param->adv_start_cmpl.status);
          }
        else
          {
            btinfo("advertising started\n");
            g_ble_priv.state = BLE_STATE_ADVERTISING;
          }
        break;

      case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS)
          {
            bterr("advertising stop failed: %d\n",
                  param->adv_stop_cmpl.status);
          }
        else
          {
            btinfo("advertising stopped\n");
            if (g_ble_priv.state == BLE_STATE_ADVERTISING)
              {
                g_ble_priv.state = BLE_STATE_IDLE;
              }
          }
        break;

      case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
        btinfo("scan params configured\n");
        break;

      case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
        if (param->scan_start_cmpl.status != ESP_BT_STATUS_SUCCESS)
          {
            bterr("scan start failed: %d\n",
                  param->scan_start_cmpl.status);
          }
        else
          {
            btinfo("scan started\n");
            g_ble_priv.state = BLE_STATE_SCANNING;
          }
        break;

      case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
        if (param->scan_stop_cmpl.status != ESP_BT_STATUS_SUCCESS)
          {
            bterr("scan stop failed: %d\n",
                  param->scan_stop_cmpl.status);
          }
        else
          {
            btinfo("scan stopped\n");
            if (g_ble_priv.state == BLE_STATE_SCANNING)
              {
                g_ble_priv.state = BLE_STATE_IDLE;
              }
          }
        break;

      case ESP_GAP_BLE_SCAN_RESULT_EVT:
        {
          esp_ble_gap_cb_param_t *scan_result = param;

          switch (scan_result->scan_rst.search_evt)
            {
              case ESP_GAP_SEARCH_INQ_RES_EVT:
                {
                  char addr_str[18];

                  esp32p4_ble_format_addr(addr_str, sizeof(addr_str),
                                           scan_result->scan_rst.bda);

                  btinfo("scan result: addr=%s rssi=%d\n",
                         addr_str, scan_result->scan_rst.rssi);

                  /* Invoke scan callback if registered */

                  if (g_ble_priv.scan_callback)
                    {
                      g_ble_priv.scan_callback(
                          scan_result->scan_rst.bda,
                          scan_result->scan_rst.rssi,
                          scan_result->scan_rst.ble_adv,
                          scan_result->scan_rst.adv_data_len);
                    }
                }
                break;

              case ESP_GAP_SEARCH_INQ_CMPL_EVT:
                btinfo("scan complete\n");
                g_ble_priv.state = BLE_STATE_IDLE;
                break;

              default:
                break;
            }
        }
        break;

      case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
        btinfo("conn params update: status=%d int=%d lat=%d timeout=%d\n",
               param->update_conn_params.status,
               param->update_conn_params.conn_int,
               param->update_conn_params.latency,
               param->update_conn_params.timeout);
        break;

      case ESP_GAP_BLE_SET_PKT_LENGTH_COMPLETE_EVT:
        btinfo("pkt length update: status=%d rx=%d tx=%d\n",
               param->pkt_data_length_cmpl.status,
               param->pkt_data_length_cmpl.params.rx_len,
               param->pkt_data_length_cmpl.params.tx_len);
        break;

      default:
        break;
    }
}

/****************************************************************************
 * Name: esp32p4_ble_gatts_profile_event_handler
 *
 * Description:
 *   GATT server profile event handler for Bluedroid.
 *   Reference: ESP-IDF Bluedroid_GATT_Server example.
 *
 ****************************************************************************/

static void esp32p4_ble_gatts_profile_event_handler(
    esp_gatts_cb_event_t event,
    esp_gatt_if_t gatts_if,
    esp_ble_gatts_cb_param_t *param)
{
  switch (event)
    {
      case ESP_GATTS_REG_EVT:
        {
          esp_err_t ret;

          btinfo("GATT register: status=%d app_id=%d gatts_if=%d\n",
                 param->reg.status, param->reg.app_id, gatts_if);

          g_ble_priv.gatts_if = gatts_if;

          /* Set device name */

          ret = esp_ble_gap_set_device_name(ESP32P4_BLE_DEVICE_NAME);
          if (ret)
            {
              bterr("set device name failed: %d\n", ret);
            }

          /* Start advertising */

          esp32p4_ble_bluedroid_start_advertising();

          /* Create custom service */

          esp_gatt_srvc_id_t service_id =
          {
            .is_primary = true,
            .id.inst_id = 0x00,
            .id.uuid.len = ESP_UUID_LEN_16,
            .id.uuid.uuid.uuid16 = ESP32P4_BLE_SERVICE_UUID,
          };

          esp_ble_gatts_create_service(gatts_if, &service_id,
                                       ESP32P4_BLE_NUM_HANDLES);
        }
        break;

      case ESP_GATTS_CREATE_EVT:
        {
          esp_err_t ret;
          esp_bt_uuid_t char_uuid;

          btinfo("service created: status=%d handle=%d\n",
                 param->create.status, param->create.service_handle);

          g_ble_priv.service_handle = param->create.service_handle;

          /* Start service */

          esp_ble_gatts_start_service(g_ble_priv.service_handle);

          /* Add characteristic */

          char_uuid.len = ESP_UUID_LEN_16;
          char_uuid.uuid.uuid16 = ESP32P4_BLE_CHAR_UUID;

          ret = esp_ble_gatts_add_char(
              g_ble_priv.service_handle,
              &char_uuid,
              ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
              ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE |
              ESP_GATT_CHAR_PROP_BIT_NOTIFY,
              &gatts_demo_char_val,
              NULL);

          if (ret)
            {
              bterr("add char failed: %d\n", ret);
            }
        }
        break;

      case ESP_GATTS_ADD_CHAR_EVT:
        {
          esp_err_t ret;
          esp_bt_uuid_t descr_uuid;

          btinfo("char added: status=%d handle=%d\n",
                 param->add_char.status, param->add_char.attr_handle);

          g_ble_priv.char_handle = param->add_char.attr_handle;

          /* Add CCCD descriptor */

          descr_uuid.len = ESP_UUID_LEN_16;
          descr_uuid.uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;

          ret = esp_ble_gatts_add_char_descr(
              g_ble_priv.service_handle,
              &descr_uuid,
              ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
              NULL, NULL);

          if (ret)
            {
              bterr("add char descr failed: %d\n", ret);
            }
        }
        break;

      case ESP_GATTS_ADD_CHAR_DESCR_EVT:
        btinfo("descriptor added: status=%d handle=%d\n",
               param->add_char_descr.status,
               param->add_char_descr.attr_handle);
        g_ble_priv.descr_handle = param->add_char_descr.attr_handle;
        break;

      case ESP_GATTS_START_EVT:
        btinfo("service started: status=%d\n", param->start.status);
        break;

      case ESP_GATTS_READ_EVT:
        {
          esp_gatt_rsp_t rsp;

          btinfo("char read: conn_id=%d handle=%d\n",
                 param->read.conn_id, param->read.handle);

          memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
          rsp.attr_value.handle = param->read.handle;
          rsp.attr_value.len = 4;
          rsp.attr_value.value[0] = 0xde;
          rsp.attr_value.value[1] = 0xed;
          rsp.attr_value.value[2] = 0xbe;
          rsp.attr_value.value[3] = 0xef;

          esp_ble_gatts_send_response(gatts_if, param->read.conn_id,
                                      param->read.trans_id,
                                      ESP_GATT_OK, &rsp);
        }
        break;

      case ESP_GATTS_WRITE_EVT:
        {
          btinfo("char write: conn_id=%d handle=%d len=%d\n",
                 param->write.conn_id, param->write.handle,
                 param->write.len);

          if (!param->write.is_prep)
            {
              /* Check if notification/indication enabled via CCCD */

              if (g_ble_priv.descr_handle == param->write.handle &&
                  param->write.len == 2)
                {
                  uint16_t descr_value =
                      param->write.value[1] << 8 | param->write.value[0];

                  if (descr_value == 0x0001)
                    {
                      btinfo("notification enabled\n");
                    }
                  else if (descr_value == 0x0002)
                    {
                      btinfo("indication enabled\n");
                    }
                  else if (descr_value == 0x0000)
                    {
                      btinfo("notification/indication disabled\n");
                    }
                }

              /* Call RX callback if set */

              if (g_ble_priv.rx_callback && param->write.len > 0)
                {
                  g_ble_priv.rx_callback(param->write.value,
                                         param->write.len);
                }

              /* Send response if needed */

              if (param->write.need_rsp)
                {
                  esp_ble_gatts_send_response(
                      gatts_if, param->write.conn_id,
                      param->write.trans_id, ESP_GATT_OK, NULL);
                }
            }
        }
        break;

      case ESP_GATTS_CONNECT_EVT:
        {
          esp_ble_conn_update_params_t conn_params = {0};

          btinfo("connected: conn_id=%d remote="
                 "%02x:%02x:%02x:%02x:%02x:%02x\n",
                 param->connect.conn_id,
                 param->connect.remote_bda[0],
                 param->connect.remote_bda[1],
                 param->connect.remote_bda[2],
                 param->connect.remote_bda[3],
                 param->connect.remote_bda[4],
                 param->connect.remote_bda[5]);

          g_ble_priv.conn.connected = true;
          g_ble_priv.conn.conn_id = param->connect.conn_id;
          memcpy(g_ble_priv.conn.remote_bda,
                 param->connect.remote_bda, 6);

          g_ble_priv.state = BLE_STATE_CONNECTED;

          /* Update connection parameters */

          memcpy(conn_params.bda, param->connect.remote_bda,
                 sizeof(esp_bd_addr_t));
          conn_params.latency = ESP32P4_BLE_CONN_LATENCY;
          conn_params.max_int = ESP32P4_BLE_CONN_ITVL_MAX;
          conn_params.min_int = ESP32P4_BLE_CONN_ITVL_MIN;
          conn_params.timeout = ESP32P4_BLE_CONN_TIMEOUT;

          esp_ble_gap_update_conn_params(&conn_params);

          /* Notify via callback */

          if (g_ble_priv.connect_callback)
            {
              g_ble_priv.connect_callback(true, param->connect.conn_id);
            }
        }
        break;

      case ESP_GATTS_DISCONNECT_EVT:
        btinfo("disconnected: reason=0x%02x\n",
               param->disconnect.reason);

        g_ble_priv.conn.connected = false;
        g_ble_priv.state = BLE_STATE_IDLE;

        /* Notify via callback */

        if (g_ble_priv.connect_callback)
          {
            g_ble_priv.connect_callback(false, 0);
          }

        /* Restart advertising */

        esp_ble_gap_start_advertising(&g_adv_params);
        break;

      case ESP_GATTS_CONF_EVT:
        btinfo("confirm: status=%d handle=%d\n",
               param->conf.status, param->conf.handle);
        break;

      case ESP_GATTS_MTU_EVT:
        btinfo("MTU exchange: mtu=%d\n", param->mtu.mtu);
        break;

      default:
        break;
    }
}

/****************************************************************************
 * Name: esp32p4_ble_gatts_event_handler
 *
 * Description:
 *   GATT server event handler for Bluedroid.
 *
 ****************************************************************************/

static void esp32p4_ble_gatts_event_handler(esp_gatts_cb_event_t event,
                                             esp_gatt_if_t gatts_if,
                                             esp_ble_gatts_cb_param_t *param)
{
  /* If event is register event, store the gatts_if */

  if (event == ESP_GATTS_REG_EVT)
    {
      if (param->reg.status == ESP_GATT_OK)
        {
          g_ble_priv.gatts_if = gatts_if;
        }
      else
        {
          bterr("register failed: app_id=%d status=%d\n",
                param->reg.app_id, param->reg.status);
          return;
        }
    }

  /* Call profile event handler */

  esp32p4_ble_gatts_profile_event_handler(event, gatts_if, param);
}

#endif /* CONFIG_ESP32P4_BLE_BLUEDROID */

/****************************************************************************
 * Private Functions - NimBLE
 ****************************************************************************/

#ifdef CONFIG_ESP32P4_BLE_NIMBLE

/****************************************************************************
 * Name: esp32p4_ble_nimble_start_advertising
 *
 * Description:
 *   Start BLE advertising using NimBLE.
 *   Reference: ESP-IDF NimBLE_GATT_Server example.
 *
 ****************************************************************************/

static void esp32p4_ble_nimble_start_advertising(void)
{
  int rc;
  struct ble_hs_adv_fields adv_fields = {0};
  struct ble_hs_adv_fields rsp_fields = {0};
  struct ble_gap_adv_params adv_params = {0};
  const char *name;

  /* Set advertising flags */

  adv_fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

  /* Set device name */

  name = ble_svc_gap_device_name();
  adv_fields.name = (uint8_t *)name;
  adv_fields.name_len = strlen(name);
  adv_fields.name_is_complete = 1;

  /* Set TX power level */

  adv_fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;
  adv_fields.tx_pwr_lvl_is_present = 1;

  /* Set appearance */

  adv_fields.appearance = BLE_GAP_APPEARANCE_GENERIC_TAG;
  adv_fields.appearance_is_present = 1;

  /* Set LE role */

  adv_fields.le_role = BLE_GAP_LE_ROLE_PERIPHERAL;
  adv_fields.le_role_is_present = 1;

  /* Set advertising fields */

  rc = ble_gap_adv_set_fields(&adv_fields);
  if (rc != 0)
    {
      bterr("set advertising data failed: %d\n", rc);
      return;
    }

  /* Set scan response fields with device address */

  rsp_fields.device_addr = g_addr_val;
  rsp_fields.device_addr_type = g_own_addr_type;
  rsp_fields.device_addr_is_present = 1;

  rc = ble_gap_adv_rsp_set_fields(&rsp_fields);
  if (rc != 0)
    {
      bterr("set scan response data failed: %d\n", rc);
      return;
    }

  /* Set advertising parameters - connectable, general discoverable */

  adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
  adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
  adv_params.itvl_min = BLE_GAP_ADV_ITVL_MS(30);
  adv_params.itvl_max = BLE_GAP_ADV_ITVL_MS(50);

  /* Start advertising */

  rc = ble_gap_adv_start(g_own_addr_type, NULL, BLE_HS_FOREVER,
                          &adv_params,
                          esp32p4_ble_gap_event_handler, NULL);
  if (rc != 0)
    {
      bterr("start advertising failed: %d\n", rc);
      return;
    }

  btinfo("advertising started\n");
  g_ble_priv.state = BLE_STATE_ADVERTISING;
}

/****************************************************************************
 * Name: esp32p4_ble_gap_event_handler
 *
 * Description:
 *   GAP event handler for NimBLE.
 *   Reference: ESP-IDF NimBLE_GATT_Server example.
 *
 ****************************************************************************/

static int esp32p4_ble_gap_event_handler(struct ble_gap_event *event,
                                          void *arg)
{
  int rc = 0;
  struct ble_gap_conn_desc desc;

  switch (event->type)
    {
      case BLE_GAP_EVENT_CONNECT:
        btinfo("connection %s; status=%d\n",
               event->connect.status == 0 ? "established" : "failed",
               event->connect.status);

        if (event->connect.status == 0)
          {
            /* Connection succeeded - print connection descriptor */

            rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
            if (rc == 0)
              {
                char addr_str[18];

                esp32p4_ble_format_addr(addr_str, sizeof(addr_str),
                                         desc.peer_id_addr.val);
                btinfo("conn_handle=%d peer=%s\n",
                       desc.conn_handle, addr_str);
                btinfo("conn_itvl=%d latency=%d timeout=%d\n",
                       desc.conn_itvl, desc.conn_latency,
                       desc.supervision_timeout);
              }

            g_ble_priv.conn.connected = true;
            g_ble_priv.conn.conn_id = event->connect.conn_handle;
            g_ble_priv.state = BLE_STATE_CONNECTED;

            /* Try to update connection parameters */

            struct ble_gap_upd_params params =
            {
              .itvl_min = ESP32P4_BLE_CONN_ITVL_MIN,
              .itvl_max = ESP32P4_BLE_CONN_ITVL_MAX,
              .latency = ESP32P4_BLE_CONN_LATENCY,
              .supervision_timeout = ESP32P4_BLE_CONN_TIMEOUT,
            };

            rc = ble_gap_update_params(event->connect.conn_handle, &params);
            if (rc != 0)
              {
                btwarn("conn params update failed: %d\n", rc);
              }

            /* Notify via callback */

            if (g_ble_priv.connect_callback)
              {
                g_ble_priv.connect_callback(true,
                    event->connect.conn_handle);
              }
          }
        else
          {
            /* Connection failed, restart advertising */

            esp32p4_ble_nimble_start_advertising();
          }
        break;

      case BLE_GAP_EVENT_DISCONNECT:
        btinfo("disconnected; reason=%d\n",
               event->disconnect.reason);

        g_ble_priv.conn.connected = false;
        g_ble_priv.state = BLE_STATE_IDLE;

        /* Notify via callback */

        if (g_ble_priv.connect_callback)
          {
            g_ble_priv.connect_callback(false, 0);
          }

        /* Restart advertising */

        esp32p4_ble_nimble_start_advertising();
        break;

      case BLE_GAP_EVENT_CONN_UPDATE:
        btinfo("connection updated; status=%d\n",
               event->conn_update.status);

        rc = ble_gap_conn_find(event->conn_update.conn_handle, &desc);
        if (rc == 0)
          {
            btinfo("new params: itvl=%d latency=%d timeout=%d\n",
                   desc.conn_itvl, desc.conn_latency,
                   desc.supervision_timeout);
          }
        break;

      case BLE_GAP_EVENT_ADV_COMPLETE:
        btinfo("advertise complete; reason=%d\n",
               event->adv_complete.reason);

        if (g_ble_priv.state == BLE_STATE_ADVERTISING)
          {
            esp32p4_ble_nimble_start_advertising();
          }
        break;

      case BLE_GAP_EVENT_NOTIFY_TX:
        if ((event->notify_tx.status != 0) &&
            (event->notify_tx.status != BLE_HS_EDONE))
          {
            btinfo("notify event; conn=%d attr=%d status=%d\n",
                   event->notify_tx.conn_handle,
                   event->notify_tx.attr_handle,
                   event->notify_tx.status);
          }
        break;

      case BLE_GAP_EVENT_SUBSCRIBE:
        btinfo("subscribe event; conn=%d attr=%d reason=%d "
               "prevn=%d curn=%d previ=%d curi=%d\n",
               event->subscribe.conn_handle,
               event->subscribe.attr_handle,
               event->subscribe.reason,
               event->subscribe.prev_notify,
               event->subscribe.cur_notify,
               event->subscribe.prev_indicate,
               event->subscribe.cur_indicate);

        /* Handle GATT subscription events */

        esp32p4_ble_gatt_svr_subscribe_cb(event);
        break;

      case BLE_GAP_EVENT_MTU:
        btinfo("mtu update; conn=%d cid=%d mtu=%d\n",
               event->mtu.conn_handle, event->mtu.channel_id,
               event->mtu.value);
        break;

      case BLE_GAP_EVENT_DISC:
        {
          struct ble_hs_adv_fields fields;
          char addr_str[18];

          rc = ble_hs_adv_parse_fields(&fields, event->disc.data,
                                        event->disc.length_data);
          if (rc != 0)
            {
              return 0;
            }

          esp32p4_ble_format_addr(addr_str, sizeof(addr_str),
                                   event->disc.addr.val);

          btinfo("scan result: addr=%s addr_type=%d rssi=%d\n",
                 addr_str, event->disc.addr.type, event->disc.rssi);

          /* Invoke scan callback if registered */

          if (g_ble_priv.scan_callback)
            {
              g_ble_priv.scan_callback(
                  event->disc.addr.val,
                  event->disc.rssi,
                  event->disc.data,
                  event->disc.length_data);
            }
        }
        break;

      case BLE_GAP_EVENT_DISC_COMPLETE:
        btinfo("scan complete; reason=%d\n",
               event->disc_complete.reason);
        g_scan_active = false;
        if (g_ble_priv.state == BLE_STATE_SCANNING)
          {
            g_ble_priv.state = BLE_STATE_IDLE;
          }
        break;

      case BLE_GAP_EVENT_ENC_CHANGE:
        btinfo("encryption change; status=%d conn=%d\n",
               event->enc_change.status,
               event->enc_change.conn_handle);
        break;

      case BLE_GAP_EVENT_REPEAT_PAIRING:
        {
          /* We already have a bond with the peer but it is attempting
           * to establish a new secure link. Delete the old bond and
           * accept the new link.
           */

          rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
          if (rc == 0)
            {
              ble_store_util_delete_peer(&desc.peer_id_addr);
            }

          return BLE_GAP_REPEAT_PAIRING_RETRY;
        }

      default:
        break;
    }

  return rc;
}

/****************************************************************************
 * Name: esp32p4_ble_gatt_chr_access
 *
 * Description:
 *   GATT characteristic access callback for the custom service.
 *
 ****************************************************************************/

static int esp32p4_ble_gatt_chr_access(uint16_t conn_handle,
                                        uint16_t attr_handle,
                                        struct ble_gatt_access_ctxt *ctxt,
                                        void *arg)
{
  int rc;
  static uint8_t chr_val[] = {0x11, 0x22, 0x33};

  switch (ctxt->op)
    {
      case BLE_GATT_ACCESS_OP_READ_CHR:
        btinfo("char read; conn=%d attr=%d\n", conn_handle, attr_handle);

        if (attr_handle == g_chr_val_handle)
          {
            rc = os_mbuf_append(ctxt->om, chr_val, sizeof(chr_val));
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
          }
        goto error;

      case BLE_GATT_ACCESS_OP_WRITE_CHR:
        btinfo("char write; conn=%d attr=%d len=%d\n",
               conn_handle, attr_handle, ctxt->om->om_len);

        if (attr_handle == g_chr_val_handle)
          {
            /* Update stored value */

            if (ctxt->om->om_len <= sizeof(chr_val))
              {
                memcpy(chr_val, ctxt->om->om_data, ctxt->om->om_len);
              }

            /* Call RX callback if set */

            if (g_ble_priv.rx_callback && ctxt->om->om_len > 0)
              {
                g_ble_priv.rx_callback(ctxt->om->om_data,
                                       ctxt->om->om_len);
              }
            return 0;
          }
        goto error;

      default:
        goto error;
    }

error:
  bterr("unexpected access op: %d\n", ctxt->op);
  return BLE_ATT_ERR_UNLIKELY;
}

/****************************************************************************
 * Name: esp32p4_ble_heart_rate_chr_access
 *
 * Description:
 *   GATT characteristic access callback for Heart Rate Service.
 *
 ****************************************************************************/

static int esp32p4_ble_heart_rate_chr_access(uint16_t conn_handle,
                                              uint16_t attr_handle,
                                              struct ble_gatt_access_ctxt *ctxt,
                                              void *arg)
{
  int rc;

  switch (ctxt->op)
    {
      case BLE_GATT_ACCESS_OP_READ_CHR:
        btinfo("heart rate read; conn=%d\n", conn_handle);

        rc = os_mbuf_append(ctxt->om, g_heart_rate_val,
                            sizeof(g_heart_rate_val));
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;

      default:
        bterr("unexpected heart rate access op: %d\n", ctxt->op);
        return BLE_ATT_ERR_UNLIKELY;
    }
}

/****************************************************************************
 * Name: esp32p4_ble_gatt_svr_subscribe_cb
 *
 * Description:
 *   Handle GATT subscribe events for notification/indication tracking.
 *
 ****************************************************************************/

static void esp32p4_ble_gatt_svr_subscribe_cb(struct ble_gap_event *event)
{
  /* Check if this is for the heart rate characteristic */

  if (event->subscribe.attr_handle == g_ble_priv.heart_rate_chr_val_handle)
    {
      g_ble_priv.heart_rate_chr_conn = event->subscribe.conn_handle;
      g_ble_priv.heart_rate_ind_status = event->subscribe.cur_indicate;
      btinfo("heart rate indication %s\n",
             g_ble_priv.heart_rate_ind_status ? "enabled" : "disabled");
    }
}

/****************************************************************************
 * Name: esp32p4_ble_gatt_svr_register_cb
 *
 * Description:
 *   GATT server register callback for NimBLE.
 *
 ****************************************************************************/

static void esp32p4_ble_gatt_svr_register_cb(
    struct ble_gatt_register_ctxt *ctxt, void *arg)
{
  char buf[BLE_UUID_STR_LEN];

  switch (ctxt->op)
    {
      case BLE_GATT_REGISTER_OP_SVC:
        btinfo("registered service %s handle=%d\n",
               ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf),
               ctxt->svc.handle);
        break;

      case BLE_GATT_REGISTER_OP_CHR:
        btinfo("registering characteristic %s def=%d val=%d\n",
               ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
               ctxt->chr.def_handle, ctxt->chr.val_handle);

        /* Track heart rate characteristic value handle */

        if (ble_uuid_cmp(ctxt->chr.chr_def->uuid,
                          &g_hrs_chr_uuid.u) == 0)
          {
            g_ble_priv.heart_rate_chr_val_handle = ctxt->chr.val_handle;
            btinfo("heart rate chr val handle=%d\n",
                   ctxt->chr.val_handle);
          }
        break;

      case BLE_GATT_REGISTER_OP_DSC:
        btinfo("registering descriptor %s handle=%d\n",
               ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf),
               ctxt->dsc.handle);
        break;

      default:
        break;
    }
}

/****************************************************************************
 * Name: esp32p4_ble_on_stack_reset
 *
 * Description:
 *   NimBLE stack reset callback.
 *
 ****************************************************************************/

static void esp32p4_ble_on_stack_reset(int reason)
{
  btinfo("nimble stack reset; reason=%d\n", reason);
  g_ble_priv.state = BLE_STATE_IDLE;
  g_ble_priv.conn.connected = false;
}

/****************************************************************************
 * Name: esp32p4_ble_on_stack_sync
 *
 * Description:
 *   NimBLE stack sync callback. Called when host has synced with
 *   controller. Initializes advertising.
 *
 ****************************************************************************/

static void esp32p4_ble_on_stack_sync(void)
{
  int rc;
  char addr_str[18] = {0};

  /* Ensure we have proper BT identity address set */

  rc = ble_hs_util_ensure_addr(0);
  if (rc != 0)
    {
      bterr("no available bt address\n");
      return;
    }

  /* Figure out BT address to use while advertising */

  rc = ble_hs_id_infer_auto(0, &g_own_addr_type);
  if (rc != 0)
    {
      bterr("failed to infer address type: %d\n", rc);
      return;
    }

  /* Copy address */

  rc = ble_hs_id_copy_addr(g_own_addr_type, g_addr_val, NULL);
  if (rc != 0)
    {
      bterr("failed to copy device address: %d\n", rc);
      return;
    }

  esp32p4_ble_format_addr(addr_str, sizeof(addr_str), g_addr_val);
  btinfo("device address: %s (type=%d)\n", addr_str, g_own_addr_type);

  /* Start advertising */

  esp32p4_ble_nimble_start_advertising();
}

/****************************************************************************
 * Name: esp32p4_ble_nimble_host_task
 *
 * Description:
 *   NimBLE host task. This function runs the NimBLE host event loop.
 *
 ****************************************************************************/

static void esp32p4_ble_nimble_host_task(void *param)
{
  btinfo("nimble host task started\n");

  /* This function won't return until nimble_port_stop() is executed */

  nimble_port_run();

  /* Clean up at exit */

  nimble_port_freertos_deinit();
}

/****************************************************************************
 * Name: esp32p4_ble_gatt_svc_init
 *
 * Description:
 *   Initialize GATT services for NimBLE.
 *
 ****************************************************************************/

static int esp32p4_ble_gatt_svc_init(void)
{
  int rc;

  /* GATT service initialization */

  ble_svc_gatt_init();

  /* Update GATT services counter */

  rc = ble_gatts_count_cfg(gatt_svr_svcs);
  if (rc != 0)
    {
      bterr("gatts count cfg failed: %d\n", rc);
      return rc;
    }

  /* Add GATT services */

  rc = ble_gatts_add_svcs(gatt_svr_svcs);
  if (rc != 0)
    {
      bterr("gatts add svcs failed: %d\n", rc);
      return rc;
    }

  return 0;
}

#endif /* CONFIG_ESP32P4_BLE_NIMBLE */

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: esp32p4_ble_init
 *
 * Description:
 *   Initialize the BLE driver.
 *
 * Input Parameters:
 *   None
 *
 * Returned Value:
 *   Zero (OK) on success; a negated errno on failure.
 *
 ****************************************************************************/

int esp32p4_ble_init(void)
{
  int ret;

  btinfo("initializing BLE driver\n");

  /* Check if already initialized */

  if (g_ble_priv.state != BLE_STATE_UNINIT)
    {
      btwarn("BLE already initialized\n");
      return -EALREADY;
    }

  /* Initialize mutex */

  ret = nxmutex_init(&g_ble_priv.lock);
  if (ret < 0)
    {
      bterr("mutex init failed: %d\n", ret);
      return ret;
    }

  /* Initialize state */

  g_ble_priv.state = BLE_STATE_IDLE;
  g_ble_priv.conn.connected = false;
  g_ble_priv.rx_callback = NULL;
  g_ble_priv.scan_callback = NULL;
  g_ble_priv.connect_callback = NULL;

#ifdef CONFIG_ESP32P4_BLE_NIMBLE
  g_ble_priv.heart_rate_chr_val_handle = 0;
  g_ble_priv.heart_rate_chr_conn = BLE_HS_CONN_HANDLE_NONE;
  g_ble_priv.heart_rate_ind_status = false;
#endif

#ifdef CONFIG_ESP32P4_BLE_BLUEDROID
  {
    esp_err_t esp_ret;

    /* Release classic BT memory */

    esp_ret = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    if (esp_ret != ESP_OK)
      {
        btwarn("release classic BT memory failed: %d\n", esp_ret);
      }

    /* Initialize BT controller */

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_ret = esp_bt_controller_init(&bt_cfg);
    if (esp_ret)
      {
        bterr("controller init failed: %d\n", esp_ret);
        ret = -EIO;
        goto errout;
      }

    /* Enable BT controller in BLE mode */

    esp_ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (esp_ret)
      {
        bterr("controller enable failed: %d\n", esp_ret);
        ret = -EIO;
        goto errout;
      }

    /* Initialize Bluedroid */

    esp_ret = esp_bluedroid_init();
    if (esp_ret)
      {
        bterr("bluedroid init failed: %d\n", esp_ret);
        ret = -EIO;
        goto errout;
      }

    /* Enable Bluedroid */

    esp_ret = esp_bluedroid_enable();
    if (esp_ret)
      {
        bterr("bluedroid enable failed: %d\n", esp_ret);
        ret = -EIO;
        goto errout;
      }

    /* Register GATT server callback */

    esp_ret = esp_ble_gatts_register_callback(
        esp32p4_ble_gatts_event_handler);
    if (esp_ret)
      {
        bterr("gatts register callback failed: %d\n", esp_ret);
        ret = -EIO;
        goto errout;
      }

    /* Register GAP callback */

    esp_ret = esp_ble_gap_register_callback(
        esp32p4_ble_gap_event_handler);
    if (esp_ret)
      {
        bterr("gap register callback failed: %d\n", esp_ret);
        ret = -EIO;
        goto errout;
      }

    /* Register GATT server application */

    esp_ret = esp_ble_gatts_app_register(0);
    if (esp_ret)
      {
        bterr("gatts app register failed: %d\n", esp_ret);
        ret = -EIO;
        goto errout;
      }

    /* Set local MTU */

    esp_ret = esp_ble_gatt_set_local_mtu(CONFIG_ESP32P4_BLE_MTU);
    if (esp_ret)
      {
        btwarn("set local MTU failed: %d\n", esp_ret);
      }
  }
#endif /* CONFIG_ESP32P4_BLE_BLUEDROID */

#ifdef CONFIG_ESP32P4_BLE_NIMBLE
  {
    int rc;

    /* Initialize NimBLE port */

    esp_err_t esp_ret = nimble_port_init();
    if (esp_ret != ESP_OK)
      {
        bterr("nimble port init failed: %d\n", esp_ret);
        ret = -EIO;
        goto errout;
      }

    /* Initialize GAP service */

    ble_svc_gap_init();

    /* Set device name */

    rc = ble_svc_gap_device_name_set(ESP32P4_BLE_DEVICE_NAME);
    if (rc != 0)
      {
        bterr("set device name failed: %d\n", rc);
        ret = -EIO;
        goto errout;
      }

    /* Set device appearance */

    rc = ble_svc_gap_device_appearance_set(
        BLE_GAP_APPEARANCE_GENERIC_TAG);
    if (rc != 0)
      {
        btwarn("set device appearance failed: %d\n", rc);
      }

    /* Initialize GATT services */

    rc = esp32p4_ble_gatt_svc_init();
    if (rc != 0)
      {
        bterr("gatt svc init failed: %d\n", rc);
        ret = -EIO;
        goto errout;
      }

    /* Set host callbacks */

    ble_hs_cfg.reset_cb = esp32p4_ble_on_stack_reset;
    ble_hs_cfg.sync_cb = esp32p4_ble_on_stack_sync;
    ble_hs_cfg.gatts_register_cb = esp32p4_ble_gatt_svr_register_cb;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    /* Start NimBLE host task */

    nimble_port_freertos_init(esp32p4_ble_nimble_host_task);
  }
#endif /* CONFIG_ESP32P4_BLE_NIMBLE */

  btinfo("BLE driver initialized\n");
  return OK;

errout:
  nxmutex_destroy(&g_ble_priv.lock);
  g_ble_priv.state = BLE_STATE_UNINIT;
  return ret;
}

/****************************************************************************
 * Name: esp32p4_ble_deinit
 *
 * Description:
 *   Deinitialize the BLE driver.
 *
 * Input Parameters:
 *   None
 *
 * Returned Value:
 *   Zero (OK) on success; a negated errno on failure.
 *
 ****************************************************************************/

int esp32p4_ble_deinit(void)
{
  btinfo("deinitializing BLE driver\n");

  if (g_ble_priv.state == BLE_STATE_UNINIT)
    {
      return -ENODEV;
    }

  /* Stop any ongoing operations */

  if (g_ble_priv.state == BLE_STATE_ADVERTISING)
    {
      esp32p4_ble_stop_advertising();
    }

#ifdef CONFIG_ESP32P4_BLE_NIMBLE
  if (g_scan_active)
    {
      ble_gap_disc_cancel();
      g_scan_active = false;
    }

  /* Stop NimBLE host */

  nimble_port_stop();
#endif

#ifdef CONFIG_ESP32P4_BLE_BLUEDROID
  {
    esp_err_t esp_ret;

    esp_ret = esp_bluedroid_disable();
    if (esp_ret != ESP_OK)
      {
        btwarn("bluedroid disable failed: %d\n", esp_ret);
      }

    esp_ret = esp_bluedroid_deinit();
    if (esp_ret != ESP_OK)
      {
        btwarn("bluedroid deinit failed: %d\n", esp_ret);
      }

    esp_ret = esp_bt_controller_disable();
    if (esp_ret != ESP_OK)
      {
        btwarn("controller disable failed: %d\n", esp_ret);
      }

    esp_ret = esp_bt_controller_deinit();
    if (esp_ret != ESP_OK)
      {
        btwarn("controller deinit failed: %d\n", esp_ret);
      }
  }
#endif

  nxmutex_destroy(&g_ble_priv.lock);

  g_ble_priv.state = BLE_STATE_UNINIT;
  g_ble_priv.conn.connected = false;

  btinfo("BLE driver deinitialized\n");
  return OK;
}

/****************************************************************************
 * Name: esp32p4_ble_start_advertising
 *
 * Description:
 *   Start BLE advertising.
 *
 * Input Parameters:
 *   None
 *
 * Returned Value:
 *   Zero (OK) on success; a negated errno on failure.
 *
 ****************************************************************************/

int esp32p4_ble_start_advertising(void)
{
  if (g_ble_priv.state == BLE_STATE_UNINIT)
    {
      return -ENODEV;
    }

  if (g_ble_priv.state == BLE_STATE_ADVERTISING)
    {
      return -EALREADY;
    }

#ifdef CONFIG_ESP32P4_BLE_BLUEDROID
  esp32p4_ble_bluedroid_start_advertising();
#endif

#ifdef CONFIG_ESP32P4_BLE_NIMBLE
  esp32p4_ble_nimble_start_advertising();
#endif

  return OK;
}

/****************************************************************************
 * Name: esp32p4_ble_stop_advertising
 *
 * Description:
 *   Stop BLE advertising.
 *
 * Input Parameters:
 *   None
 *
 * Returned Value:
 *   Zero (OK) on success; a negated errno on failure.
 *
 ****************************************************************************/

int esp32p4_ble_stop_advertising(void)
{
  if (g_ble_priv.state != BLE_STATE_ADVERTISING)
    {
      return -EALREADY;
    }

#ifdef CONFIG_ESP32P4_BLE_BLUEDROID
  esp_ble_gap_stop_advertising();
#endif

#ifdef CONFIG_ESP32P4_BLE_NIMBLE
  ble_gap_adv_stop();
#endif

  g_ble_priv.state = BLE_STATE_IDLE;
  return OK;
}

/****************************************************************************
 * Name: esp32p4_ble_start_scan
 *
 * Description:
 *   Start BLE scanning.
 *
 * Input Parameters:
 *   duration_ms - Scan duration in milliseconds (0 = indefinite)
 *   callback    - Callback for scan results
 *
 * Returned Value:
 *   Zero (OK) on success; a negated errno on failure.
 *
 ****************************************************************************/

int esp32p4_ble_start_scan(uint32_t duration_ms,
                            void (*callback)(const uint8_t *addr,
                                             int8_t rssi,
                                             const uint8_t *data,
                                             uint8_t data_len))
{
  if (g_ble_priv.state == BLE_STATE_UNINIT)
    {
      return -ENODEV;
    }

  if (g_ble_priv.state == BLE_STATE_SCANNING)
    {
      return -EALREADY;
    }

  g_ble_priv.scan_callback = callback;

#ifdef CONFIG_ESP32P4_BLE_BLUEDROID
  {
    esp_err_t esp_ret;

    /* Configure scan parameters */

    esp_ret = esp_ble_gap_set_scan_params(&g_scan_params);
    if (esp_ret != ESP_OK)
      {
        bterr("set scan params failed: %d\n", esp_ret);
        return -EIO;
      }

    /* Start scanning */

    esp_ret = esp_ble_gap_start_scanning(duration_ms / 1000);
    if (esp_ret != ESP_OK)
      {
        bterr("start scanning failed: %d\n", esp_ret);
        return -EIO;
      }
  }
#endif

#ifdef CONFIG_ESP32P4_BLE_NIMBLE
  {
    int rc;
    struct ble_gap_disc_params disc_params = {0};

    /* Configure scan parameters - active scanning with duplicate filter */

    disc_params.filter_duplicates = 1;
    disc_params.passive = 0;
    disc_params.itvl = ESP32P4_BLE_SCAN_ITVL;
    disc_params.window = ESP32P4_BLE_SCAN_WINDOW;
    disc_params.filter_policy = 0;
    disc_params.limited = 0;

    /* Start scanning */

    rc = ble_gap_disc(g_own_addr_type,
                       duration_ms > 0 ? duration_ms : BLE_HS_FOREVER,
                       &disc_params,
                       esp32p4_ble_gap_event_handler, NULL);
    if (rc != 0)
      {
        bterr("start scanning failed: %d\n", rc);
        return -EIO;
      }

    g_scan_active = true;
  }
#endif

  g_ble_priv.state = BLE_STATE_SCANNING;
  btinfo("scan started (duration=%lu ms)\n", (unsigned long)duration_ms);
  return OK;
}

/****************************************************************************
 * Name: esp32p4_ble_stop_scan
 *
 * Description:
 *   Stop BLE scanning.
 *
 * Input Parameters:
 *   None
 *
 * Returned Value:
 *   Zero (OK) on success; a negated errno on failure.
 *
 ****************************************************************************/

int esp32p4_ble_stop_scan(void)
{
  if (g_ble_priv.state != BLE_STATE_SCANNING)
    {
      return -EALREADY;
    }

#ifdef CONFIG_ESP32P4_BLE_BLUEDROID
  {
    esp_err_t esp_ret = esp_ble_gap_stop_scanning();
    if (esp_ret != ESP_OK)
      {
        bterr("stop scanning failed: %d\n", esp_ret);
        return -EIO;
      }
  }
#endif

#ifdef CONFIG_ESP32P4_BLE_NIMBLE
  {
    int rc = ble_gap_disc_cancel();
    if (rc != 0)
      {
        bterr("stop scanning failed: %d\n", rc);
        return -EIO;
      }

    g_scan_active = false;
  }
#endif

  g_ble_priv.state = BLE_STATE_IDLE;
  btinfo("scan stopped\n");
  return OK;
}

/****************************************************************************
 * Name: esp32p4_ble_is_connected
 *
 * Description:
 *   Check if BLE is connected.
 *
 * Input Parameters:
 *   None
 *
 * Returned Value:
 *   true if connected, false otherwise.
 *
 ****************************************************************************/

bool esp32p4_ble_is_connected(void)
{
  return g_ble_priv.conn.connected;
}

/****************************************************************************
 * Name: esp32p4_ble_get_conn_id
 *
 * Description:
 *   Get the current connection handle.
 *
 * Input Parameters:
 *   conn_id - Pointer to store the connection handle
 *
 * Returned Value:
 *   Zero (OK) on success; a negated errno on failure.
 *
 ****************************************************************************/

int esp32p4_ble_get_conn_id(uint16_t *conn_id)
{
  if (!g_ble_priv.conn.connected)
    {
      return -ENOTCONN;
    }

  if (conn_id == NULL)
    {
      return -EINVAL;
    }

  *conn_id = g_ble_priv.conn.conn_id;
  return OK;
}

/****************************************************************************
 * Name: esp32p4_ble_get_peer_addr
 *
 * Description:
 *   Get the peer device address.
 *
 * Input Parameters:
 *   addr - Buffer to store the address (6 bytes)
 *
 * Returned Value:
 *   Zero (OK) on success; a negated errno on failure.
 *
 ****************************************************************************/

int esp32p4_ble_get_peer_addr(uint8_t *addr)
{
  if (!g_ble_priv.conn.connected)
    {
      return -ENOTCONN;
    }

  if (addr == NULL)
    {
      return -EINVAL;
    }

  memcpy(addr, g_ble_priv.conn.remote_bda, 6);
  return OK;
}

/****************************************************************************
 * Name: esp32p4_ble_send_notification
 *
 * Description:
 *   Send a BLE notification to the connected client.
 *
 * Input Parameters:
 *   data - Data to send
 *   len  - Length of data
 *
 * Returned Value:
 *   Zero (OK) on success; a negated errno on failure.
 *
 ****************************************************************************/

int esp32p4_ble_send_notification(const uint8_t *data, uint16_t len)
{
  if (!g_ble_priv.conn.connected)
    {
      return -ENOTCONN;
    }

#ifdef CONFIG_ESP32P4_BLE_BLUEDROID
  {
    esp_err_t ret;

    ret = esp_ble_gatts_send_indicate(
        g_ble_priv.gatts_if,
        g_ble_priv.conn.conn_id,
        g_ble_priv.char_handle,
        len, (uint8_t *)data, false);

    if (ret != ESP_OK)
      {
        bterr("send notification failed: %d\n", ret);
        return -EIO;
      }
  }
#endif

#ifdef CONFIG_ESP32P4_BLE_NIMBLE
  {
    int rc;
    struct os_mbuf *om;

    om = ble_hs_mbuf_from_flat(data, len);
    if (om == NULL)
      {
        return -ENOMEM;
      }

    rc = ble_gatts_notify_custom(g_ble_priv.conn.conn_id,
                                  g_chr_val_handle, om);
    if (rc != 0)
      {
        bterr("send notification failed: %d\n", rc);
        return -EIO;
      }
  }
#endif

  return OK;
}

/****************************************************************************
 * Name: esp32p4_ble_send_indication
 *
 * Description:
 *   Send a BLE indication to the connected client.
 *
 * Input Parameters:
 *   data - Data to send
 *   len  - Length of data
 *
 * Returned Value:
 *   Zero (OK) on success; a negated errno on failure.
 *
 ****************************************************************************/

int esp32p4_ble_send_indication(const uint8_t *data, uint16_t len)
{
  if (!g_ble_priv.conn.connected)
    {
      return -ENOTCONN;
    }

#ifdef CONFIG_ESP32P4_BLE_BLUEDROID
  {
    esp_err_t ret;

    ret = esp_ble_gatts_send_indicate(
        g_ble_priv.gatts_if,
        g_ble_priv.conn.conn_id,
        g_ble_priv.char_handle,
        len, (uint8_t *)data, true);

    if (ret != ESP_OK)
      {
        bterr("send indication failed: %d\n", ret);
        return -EIO;
      }
  }
#endif

#ifdef CONFIG_ESP32P4_BLE_NIMBLE
  {
    int rc;
    struct os_mbuf *om;

    om = ble_hs_mbuf_from_flat(data, len);
    if (om == NULL)
      {
        return -ENOMEM;
      }

    rc = ble_gatts_indicate_custom(g_ble_priv.conn.conn_id,
                                    g_chr_val_handle, om);
    if (rc != 0)
      {
        bterr("send indication failed: %d\n", rc);
        return -EIO;
      }
  }
#endif

  return OK;
}

/****************************************************************************
 * Name: esp32p4_ble_send_heart_rate_indication
 *
 * Description:
 *   Send a heart rate indication to the connected client.
 *   This sends the current heart rate value via the standard Heart Rate
 *   Service (0x180D).
 *
 * Input Parameters:
 *   heart_rate - Heart rate value (BPM)
 *
 * Returned Value:
 *   Zero (OK) on success; a negated errno on failure.
 *
 ****************************************************************************/

int esp32p4_ble_send_heart_rate_indication(uint8_t heart_rate)
{
  if (!g_ble_priv.conn.connected)
    {
      return -ENOTCONN;
    }

  /* Update heart rate value */

  g_heart_rate_val[0] = 0x00;       /* Flags: no special flags */
  g_heart_rate_val[1] = heart_rate;  /* Heart rate measurement value */

#ifdef CONFIG_ESP32P4_BLE_BLUEDROID
  {
    esp_err_t ret;

    ret = esp_ble_gatts_send_indicate(
        g_ble_priv.gatts_if,
        g_ble_priv.conn.conn_id,
        g_ble_priv.heart_rate_handle,
        sizeof(g_heart_rate_val), g_heart_rate_val, true);

    if (ret != ESP_OK)
      {
        bterr("send heart rate indication failed: %d\n", ret);
        return -EIO;
      }
  }
#endif

#ifdef CONFIG_ESP32P4_BLE_NIMBLE
  {
    int rc;

    if (!g_ble_priv.heart_rate_ind_status)
      {
        return -EPERM;  /* Client has not subscribed */
      }

    rc = ble_gatts_indicate(g_ble_priv.heart_rate_chr_conn,
                             g_ble_priv.heart_rate_chr_val_handle);
    if (rc != 0)
      {
        bterr("send heart rate indication failed: %d\n", rc);
        return -EIO;
      }
  }
#endif

  return OK;
}

/****************************************************************************
 * Name: esp32p4_ble_register_rx_callback
 *
 * Description:
 *   Register a callback function for received data.
 *
 * Input Parameters:
 *   callback - Callback function
 *
 * Returned Value:
 *   None
 *
 ****************************************************************************/

void esp32p4_ble_register_rx_callback(
    void (*callback)(const uint8_t *data, uint16_t len))
{
  g_ble_priv.rx_callback = callback;
}

/****************************************************************************
 * Name: esp32p4_ble_register_scan_callback
 *
 * Description:
 *   Register a callback function for scan results.
 *
 * Input Parameters:
 *   callback - Callback function for scan results
 *
 * Returned Value:
 *   None
 *
 ****************************************************************************/

void esp32p4_ble_register_scan_callback(
    void (*callback)(const uint8_t *addr, int8_t rssi,
                     const uint8_t *data, uint8_t data_len))
{
  g_ble_priv.scan_callback = callback;
}

/****************************************************************************
 * Name: esp32p4_ble_register_connect_callback
 *
 * Description:
 *   Register a callback function for connection events.
 *
 * Input Parameters:
 *   callback - Callback function for connection events
 *
 * Returned Value:
 *   None
 *
 ****************************************************************************/

void esp32p4_ble_register_connect_callback(
    void (*callback)(bool connected, uint16_t conn_id))
{
  g_ble_priv.connect_callback = callback;
}
