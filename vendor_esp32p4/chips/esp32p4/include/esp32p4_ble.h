/****************************************************************************
 * vendor_esp32p4/chips/esp32p4/include/esp32p4_ble.h
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

#ifndef __VENDOR_ESP32P4_CHIPS_ESP32P4_ESP32P4_BLE_H
#define __VENDOR_ESP32P4_CHIPS_ESP32P4_ESP32P4_BLE_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdint.h>
#include <stdbool.h>

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifdef CONFIG_ESP32P4_BLE

/****************************************************************************
 * Name: esp32p4_ble_init
 *
 * Description:
 *   Initialize the BLE driver. Supports both Bluedroid and NimBLE
 *   protocol stacks. Configures the GATT server with a custom service
 *   (0x00FF) and a Heart Rate Service (0x180D).
 *
 * Input Parameters:
 *   None
 *
 * Returned Value:
 *   Zero (OK) on success; a negated errno on failure.
 *
 ****************************************************************************/

int esp32p4_ble_init(void);

/****************************************************************************
 * Name: esp32p4_ble_deinit
 *
 * Description:
 *   Deinitialize the BLE driver. Stops advertising, scanning, and
 *   releases all BLE resources.
 *
 * Input Parameters:
 *   None
 *
 * Returned Value:
 *   Zero (OK) on success; a negated errno on failure.
 *
 ****************************************************************************/

int esp32p4_ble_deinit(void);

/****************************************************************************
 * Name: esp32p4_ble_start_advertising
 *
 * Description:
 *   Start BLE advertising. The device becomes discoverable and
 *   connectable by BLE central devices.
 *
 * Input Parameters:
 *   None
 *
 * Returned Value:
 *   Zero (OK) on success; a negated errno on failure.
 *
 ****************************************************************************/

int esp32p4_ble_start_advertising(void);

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

int esp32p4_ble_stop_advertising(void);

/****************************************************************************
 * Name: esp32p4_ble_start_scan
 *
 * Description:
 *   Start BLE scanning to discover nearby BLE devices.
 *
 * Input Parameters:
 *   duration_ms - Scan duration in milliseconds (0 = indefinite)
 *   callback    - Callback function invoked for each scan result.
 *                 Parameters: addr (6 bytes), rssi, data, data_len
 *
 * Returned Value:
 *   Zero (OK) on success; a negated errno on failure.
 *
 ****************************************************************************/

int esp32p4_ble_start_scan(uint32_t duration_ms,
                            void (*callback)(const uint8_t *addr,
                                             int8_t rssi,
                                             const uint8_t *data,
                                             uint8_t data_len));

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

int esp32p4_ble_stop_scan(void);

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

bool esp32p4_ble_is_connected(void);

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

int esp32p4_ble_get_conn_id(uint16_t *conn_id);

/****************************************************************************
 * Name: esp32p4_ble_get_peer_addr
 *
 * Description:
 *   Get the peer device address of the current connection.
 *
 * Input Parameters:
 *   addr - Buffer to store the address (6 bytes)
 *
 * Returned Value:
 *   Zero (OK) on success; a negated errno on failure.
 *
 ****************************************************************************/

int esp32p4_ble_get_peer_addr(uint8_t *addr);

/****************************************************************************
 * Name: esp32p4_ble_send_notification
 *
 * Description:
 *   Send a BLE notification to the connected client via the custom
 *   service characteristic.
 *
 * Input Parameters:
 *   data - Data to send
 *   len  - Length of data
 *
 * Returned Value:
 *   Zero (OK) on success; a negated errno on failure.
 *
 ****************************************************************************/

int esp32p4_ble_send_notification(const uint8_t *data, uint16_t len);

/****************************************************************************
 * Name: esp32p4_ble_send_indication
 *
 * Description:
 *   Send a BLE indication to the connected client via the custom
 *   service characteristic.
 *
 * Input Parameters:
 *   data - Data to send
 *   len  - Length of data
 *
 * Returned Value:
 *   Zero (OK) on success; a negated errno on failure.
 *
 ****************************************************************************/

int esp32p4_ble_send_indication(const uint8_t *data, uint16_t len);

/****************************************************************************
 * Name: esp32p4_ble_send_heart_rate_indication
 *
 * Description:
 *   Send a heart rate indication to the connected client via the
 *   standard Heart Rate Service (0x180D).
 *
 * Input Parameters:
 *   heart_rate - Heart rate value (BPM)
 *
 * Returned Value:
 *   Zero (OK) on success; a negated errno on failure.
 *
 ****************************************************************************/

int esp32p4_ble_send_heart_rate_indication(uint8_t heart_rate);

/****************************************************************************
 * Name: esp32p4_ble_register_rx_callback
 *
 * Description:
 *   Register a callback function for received data on the custom
 *   service characteristic.
 *
 * Input Parameters:
 *   callback - Callback function
 *
 * Returned Value:
 *   None
 *
 ****************************************************************************/

void esp32p4_ble_register_rx_callback(
    void (*callback)(const uint8_t *data, uint16_t len));

/****************************************************************************
 * Name: esp32p4_ble_register_scan_callback
 *
 * Description:
 *   Register a callback function for BLE scan results.
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
                     const uint8_t *data, uint8_t data_len));

/****************************************************************************
 * Name: esp32p4_ble_register_connect_callback
 *
 * Description:
 *   Register a callback function for BLE connection/disconnection events.
 *
 * Input Parameters:
 *   callback - Callback function for connection events
 *
 * Returned Value:
 *   None
 *
 ****************************************************************************/

void esp32p4_ble_register_connect_callback(
    void (*callback)(bool connected, uint16_t conn_id));

#endif /* CONFIG_ESP32P4_BLE */
#endif /* __VENDOR_ESP32P4_CHIPS_ESP32P4_ESP32P4_BLE_H */
