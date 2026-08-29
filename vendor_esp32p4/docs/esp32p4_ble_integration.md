# ESP32-P4 BLE Driver Integration Guide

## Overview

This document describes how to integrate the BLE (Bluetooth Low Energy) driver
for the ESP32-P4 EV Board in the openvela NuttX environment. The driver supports
both the NimBLE and Bluedroid Bluetooth stacks from ESP-IDF.

**Important hardware note:** The ESP32-P4 has NO built-in Bluetooth radio. BLE
functionality requires an external co-processor (e.g., ESP32-C6 or ESP32-H2)
connected via HCI UART. The ESP-IDF `bt` component handles the HCI transport
and protocol stack.

## Architecture

```
+------------------+     +-------------------+     +------------------+
|   Application    |     |   esp32p4_ble.c   |     |   ESP-IDF bt     |
|   (NuttX app)    |---->|   (GAP + GATT)    |---->|   component      |
+------------------+     +-------------------+     +------------------+
                                    |                        |
                                    v                        v
                         +-------------------+     +------------------+
                         |  NimBLE / Bluedroid|    |  HCI UART to     |
                         |  protocol stack    |    |  external BLE    |
                         +-------------------+    |  co-processor    |
                                                  +------------------+
```

## GATT Services

The driver registers two GATT services:

### 1. Heart Rate Service (UUID: 0x180D)
- **Characteristic:** Heart Rate Measurement (UUID: 0x2A37)
- **Properties:** Read, Indicate
- **Purpose:** Standard BLE Heart Rate Profile for health monitoring demos

### 2. Custom Service (UUID: 0x00FF)
- **Characteristic:** Custom Data (UUID: 0xFF01)
- **Properties:** Read, Write, Notify
- **Purpose:** General-purpose bidirectional data exchange

## File Structure

```
vendor_esp32p4/
  chips/esp32p4/
    esp32p4_ble.c                    # BLE driver implementation
    include/esp32p4_ble.h            # BLE driver public API header
    Kconfig                          # BLE Kconfig options (ESP32P4_BLE, etc.)
    Make.defs                        # Build rules (includes esp32p4_ble.c)
  boards/risc-v/esp32p4/esp32p4-evb/
    configs/ble/defconfig            # BLE defconfig for NuttX build
    src/esp32p4_bringup.c            # Board init (calls esp32p4_ble_init())
```

## Build Instructions

### Step 1: Configure the build

```bash
cd <nuttx-source>
./tools/configure.sh esp32p4-evb:ble
```

This loads the `ble` defconfig which enables:
- `CONFIG_ESP_IDF=y` (ESP-IDF integration)
- `CONFIG_ESP32P4_BLE=y` (BLE driver)
- `CONFIG_ESP32P4_BLE_NIMBLE=y` (NimBLE stack)

### Step 2: Build

```bash
make -j$(nproc)
```

### Step 3: Flash

```bash
make flash ESPTOOL_PORT=/dev/ttyUSB0
```

## Kconfig Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `ESP32P4_BLE` | bool | n | Enable BLE driver (requires ESP_IDF) |
| `ESP32P4_BLE_NIMBLE` | choice | - | Use NimBLE stack (recommended) |
| `ESP32P4_BLE_BLUEDROID` | choice | y | Use Bluedroid stack |
| `ESP32P4_BLE_DEVICE_NAME` | string | "ESP32P4_BLE" | BLE advertised device name |
| `ESP32P4_BLE_ADV_INT_MIN` | int | 32 | Min advertising interval (x0.625ms) |
| `ESP32P4_BLE_ADV_INT_MAX` | int | 64 | Max advertising interval (x0.625ms) |
| `ESP32P4_BLE_MTU` | int | 500 | Maximum Transmission Unit (23-517) |

## API Reference

All functions are declared in `esp32p4_ble.h` and guarded by `CONFIG_ESP32P4_BLE`.

### Initialization

```c
int esp32p4_ble_init(void);    /* Called automatically from esp32p4_bringup() */
int esp32p4_ble_deinit(void);
```

### Advertising

```c
int esp32p4_ble_start_advertising(void);
int esp32p4_ble_stop_advertising(void);
```

Advertising starts automatically after `esp32p4_ble_init()` completes and the
NimBLE host syncs with the controller. Use these functions to restart or stop
advertising manually.

### Scanning

```c
int esp32p4_ble_start_scan(uint32_t duration_ms,
                           void (*callback)(const uint8_t *addr,
                                            int8_t rssi,
                                            const uint8_t *data,
                                            uint8_t data_len));
int esp32p4_ble_stop_scan(void);
```

### Connection

```c
bool esp32p4_ble_is_connected(void);
int  esp32p4_ble_get_conn_id(uint16_t *conn_id);
int  esp32p4_ble_get_peer_addr(uint8_t *addr);
```

### Data Transfer

```c
int esp32p4_ble_send_notification(const uint8_t *data, uint16_t len);
int esp32p4_ble_send_indication(const uint8_t *data, uint16_t len);
int esp32p4_ble_send_heart_rate_indication(uint8_t heart_rate);
```

### Callbacks

```c
void esp32p4_ble_register_rx_callback(
    void (*callback)(const uint8_t *data, uint16_t len));

void esp32p4_ble_register_scan_callback(
    void (*callback)(const uint8_t *addr, int8_t rssi,
                     const uint8_t *data, uint8_t data_len));

void esp32p4_ble_register_connect_callback(
    void (*callback)(bool connected, uint16_t conn_id));
```

## NSH Usage

Once the system boots, BLE advertising starts automatically. You can verify
via the serial console:

```
nsh> # BLE initializes during board bringup
nsh> # The device advertises as "ESP32P4_BLE"
nsh> # Connect from a BLE central (phone app, etc.)
```

To scan for nearby BLE devices from application code, use the
`esp32p4_ble_start_scan()` API.

## ESP-IDF NimBLE Reference

This driver's NimBLE integration follows the patterns from the ESP-IDF
`bleprph` example:

- **Source:** `/esp-idf/examples/bluetooth/nimble/bleprph/`
- **Key files:** `main/main.c`, `main/gatt_svr.c`

Key patterns used:
1. `nimble_port_init()` initializes the NimBLE controller and host
2. `ble_svc_gap_init()` / `ble_svc_gatt_init()` initialize GAP/GATT services
3. `ble_hs_cfg.reset_cb` / `sync_cb` handle stack reset and sync events
4. `nimble_port_freertos_init()` starts the NimBLE host task
5. GATT services are defined via `ble_gatt_svc_def` arrays
6. Advertising uses `ble_gap_adv_start()` with GAP event callbacks

## Troubleshooting

### BLE init fails with -EIO

Check that:
1. The external BLE co-processor is connected and powered
2. HCI UART pins are correctly configured
3. The ESP-IDF `bt` component is included in the build

### Advertising not visible

- Verify the co-processor firmware supports BLE advertising
- Check advertising interval settings in Kconfig
- Ensure the device name is set correctly

### Connection drops immediately

- Increase `CONFIG_ESP32P4_BLE_MTU` if data transfer fails
- Check connection parameter settings in the driver
- Verify the co-processor has sufficient resources

## References

- [ESP-IDF NimBLE Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/api-reference/bluetooth/nimble/index.html)
- [Bluetooth GATT Specification](https://www.bluetooth.com/specifications/specs/generic-attribute-profile/)
- [NuttX BLE Documentation](https://nuttx.apache.org/docs/latest/components/drivers/special/bluetooth.html)
