# ESP32-P4 ESP-IDF Examples Guide for openvela

## Document Information

| Item | Content |
|------|---------|
| Target Chip | ESP32-P4 (Espressif) |
| ESP-IDF Version | v6.0.2 |
| Examples Path | `/home/geo/esp/v6.0.2/esp-idf/examples/` |
| Purpose | Catalog and adapt ESP-IDF examples for openvela development |
| Date | 2026-08-26 |

---

## Table of Contents

1. [Overview](#1-overview)
2. [Peripheral Driver Examples](#2-peripheral-driver-examples)
   - [GPIO](#21-gpio)
   - [I2C](#22-i2c)
   - [SPI](#23-spi)
   - [UART](#24-uart)
   - [USB](#25-usb)
   - [I2S Audio](#26-i2s-audio)
   - [Parallel IO (ParLIO)]#27-parallel-io-parlio)
   - [ISP (Image Signal Processor)](#28-isp-image-signal-processor)
3. [Display Examples](#3-display-examples)
   - [MIPI-DSI LCD Panel](#31-mipi-dsi-lcd-panel)
   - [RGB LCD Panel](#32-rgb-lcd-panel)
   - [I80 Parallel LCD](#33-i80-parallel-lcd)
   - [ParLIO Simulated LCD](#34-parlio-simulated-lcd)
4. [Camera Examples](#4-camera-examples)
   - [DVP Camera + DSI Display](#41-dvp-camera--dsi-display)
   - [DVP Camera + ISP + DSI Display](#42-dvp-camera--isp--dsi-display)
   - [MIPI-CSI Camera + ISP + DSI Display](#43-mipi-csi-camera--isp--dsi-display)
   - [DVP Camera + SPI LCD](#44-dvp-camera--spi-lcd)
5. [Audio/Video Examples](#5-audiovideo-examples)
   - [H.264 Encoder/Decoder](#51-h264-encoderdecoder)
   - [JPEG Hardware Encode](#52-jpeg-hardware-encode)
   - [JPEG Hardware Decode](#53-jpeg-hardware-decode)
6. [Connectivity Examples](#6-connectivity-examples)
   - [WiFi via ESP-Hosted](#61-wifi-via-esp-hosted)
   - [Bluetooth](#62-bluetooth)
   - [SDIO Host](#63-sdio-host)
7. [Storage Examples](#7-storage-examples)
8. [System Examples](#8-system-examples)
9. [Adapting Examples for openvela](#9-adapting-examples-for-openvela)
   - [Architecture Mapping](#91-architecture-mapping)
   - [Driver Adaptation Strategy](#92-driver-adaptation-strategy)
   - [Display Pipeline Adaptation](#93-display-pipeline-adaptation)
   - [Connectivity Adaptation](#94-connectivity-adaptation)
   - [Key Differences: ESP-IDF vs openvela](#95-key-differences-esp-idf-vs-openvela)
10. [Quick Reference Table](#10-quick-reference-table)

---

## 1. Overview

The ESP32-P4 is Espressif's first pure RISC-V high-performance SoC. Unlike previous Xtensa-based ESP32 chips, it features dual RISC-V Hazard3 cores at up to 400MHz, MIPI-DSI/CSI interfaces, hardware JPEG/H.264 codecs, and USB OTG HS. It lacks built-in WiFi/Bluetooth, which are provided via a co-processor (ESP32-C6) using ESP-Hosted.

### ESP32-P4 Unique Peripherals

| Peripheral | Description | ESP-IDF Component |
|-----------|-------------|-------------------|
| MIPI-DSI | Display Serial Interface (2 data lanes + 1 clock lane) | `esp_lcd` |
| MIPI-CSI | Camera Serial Interface (2 data lanes) | `esp_driver_cam` |
| DVP Camera | Parallel camera interface (8-bit data) | `esp_driver_cam` |
| ISP | Image Signal Processor (AWB, AF, denoise, gamma, etc.) | `esp_driver_isp` |
| H.264 HW Encoder | Hardware video encoder (up to 1080p) | `esp_h264` |
| JPEG HW Codec | Hardware JPEG encode/decode | `esp_driver_jpeg` |
| USB OTG HS | USB 2.0 High-Speed On-The-Go | `esp_driver_usb` |
| ParLIO | Parallel IO for high-speed data transfer | `esp_driver_parlio` |
| DMA2D | 2D DMA for display buffer operations | `esp_driver_ppa` |

### Common ESP32-P4 sdkconfig Settings

Many ESP32-P4 examples require these settings in `sdkconfig.defaults.esp32p4`:

```
CONFIG_IDF_EXPERIMENTAL_FEATURES=y
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_HEX=y
CONFIG_SPIRAM_SPEED_200M=y
```

ESP32-P4 uses hexagonal (quad) SPI PSRAM at 200MHz for framebuffer and multimedia workloads.

---

## 2. Peripheral Driver Examples

### 2.1 GPIO

**Path:** `peripherals/gpio/generic_gpio/`

**Supported Targets:** All ESP chips including ESP32-P4

**Description:** Demonstrates GPIO configuration with interrupt handling. Configures output pins to generate pulses and input pins to detect rising/falling edge interrupts.

**Key Features:**
- GPIO output pulse generation
- GPIO input with rising/falling edge interrupt
- Pin assignment configurable via `menuconfig`

**Hardware:** Connect `CONFIG_GPIO_OUTPUT_0` to `CONFIG_GPIO_INPUT_0`, and `CONFIG_GPIO_OUTPUT_1` to `CONFIG_GPIO_INPUT_1`.

**openvela Relevance:** GPIO is the most fundamental peripheral. The interrupt-driven GPIO pattern maps directly to NuttX GPIO interrupt handlers.

---

### 2.2 I2C

**Path:** `peripherals/i2c/`

| Sub-example | Description |
|-------------|-------------|
| `i2c_basic/` | Basic I2C master read/write with MPU9250 sensor |
| `i2c_eeprom/` | I2C EEPROM read/write operations |
| `i2c_slave_network_sensor/` | I2C slave network sensor |
| `i2c_tools/` | I2C bus scanning and debugging tools |
| `i2c_u8g2/` | I2C with U8G2 graphics library |

**Supported Targets:** All ESP chips including ESP32-P4

**Description:** The `i2c_basic` example demonstrates reading/writing an MPU9250 inertial measurement unit over I2C. Default pins are configurable via `menuconfig`.

**openvela Relevance:** I2C driver adaptation follows the NuttX `i2c_master_s` interface. The ESP-IDF I2C transaction model (command link) needs to be mapped to NuttX's `i2c_transfer` message-based approach.

---

### 2.3 SPI

**Path:** `peripherals/spi_master/`, `peripherals/spi_slave/`

| Sub-example | Description |
|-------------|-------------|
| `spi_master/hd_eeprom/` | SPI half-duplex EEPROM access |
| `spi_master/lcd/` | SPI LCD display driving |
| `spi_slave/receiver/` | SPI slave data receiver |
| `spi_slave/sender/` | SPI SPI slave data sender |

**Supported Targets:** Most ESP chips including ESP32-P4

**openvela Relevance:** SPI adaptation maps to NuttX `spi_dev_s` interface. The ESP-IDF DMA-based SPI transfers translate to NuttX SPI `exchange` operations.

---

### 2.4 UART

**Path:** `peripherals/uart/`

| Sub-example | Description |
|-------------|-------------|
| `uart_echo/` | Basic UART echo (data received is sent back) |
| `uart_async_rxtxtasks/` | Async UART with separate RX/TX tasks |
| `uart_events/` | UART event-driven communication |
| `uart_dma_ota/` | UART with DMA for OTA updates |
| `uart_echo_rs485/` | RS485 half-duplex communication |
| `uart_repl/` | UART REPL (read-eval-print loop) |
| `uart_select/` | UART with select() for multiplexing |
| `nmea0183_parser/` | NMEA GPS data parser |

**Supported Targets:** All ESP chips including ESP32-P4

**openvela Relevance:** UART is the console/serial port. NuttX uses `uart_dev_s` and registers via `uart_register`. The ESP-IDF UART driver pattern maps well to NuttX serial driver.

---

### 2.5 USB

**Path:** `peripherals/usb/`

**USB Device Examples:**

| Sub-example | Description |
|-------------|-------------|
| `device/tusb_serial_device/` | TinyUSB serial device (CDC ACM) |
| `device/tusb_composite_msc_serialdevice/` | Composite MSC + Serial device |
| `device/tusb_console/` | USB console output |
| `device/tusb_hid/` | USB HID device |
| `device/tusb_midi/` | USB MIDI device |
| `device/tusb_msc/` | USB Mass Storage device |
| `device/tusb_ncm/` | USB NCM network device |
| `device/cherryusb_serial_device/` | CherryUSB serial device |

**USB Host Examples:**

| Sub-example | Description |
|-------------|-------------|
| `host/cdc/` | USB CDC host |
| `host/hid/` | USB HID host |
| `host/msc/` | USB Mass Storage host |
| `host/uvc/` | USB Video Class (camera) host |
| `host/usb_host_lib/` | Low-level USB host library |
| `host/cherryusb_host/` | CherryUSB host |

**ESP32-P4 USB Notes:**
- ESP32-P4 supports USB OTG High-Speed (480 Mbps)
- USB host MSC and device examples have ESP32-P4 specific `sdkconfig.defaults.esp32p4`
- The UVC host example supports ESP32-P4 for connecting USB cameras
- Requires SPIRAM for frame buffer storage

**openvela Relevance:** USB adaptation is complex. NuttX has its own USB device/controller framework. The TinyUSB/CherryUSB stacks would need integration with NuttX's USB subsystem.

---

### 2.6 I2S Audio

**Path:** `peripherals/i2s/`

| Sub-example | Description |
|-------------|-------------|
| `i2s_basic/` | Basic I2S audio input/output |
| `i2s_advance/i2s_usb/` | I2S + USB Audio Class (UAC) device |
| `i2s_codec/` | I2S with audio codec (ES8311) |
| `i2s_recorder/` | I2S audio recording |

**ESP32-P4 Specific:** `i2s_advance/i2s_usb/` has ESP32-P4 sdkconfig with pin assignments:

```
CONFIG_EXAMPLE_I2C_SCL_IO=8
CONFIG_EXAMPLE_I2C_SDA_IO=7
CONFIG_EXAMPLE_I2S_MCK_IO=13
CONFIG_EXAMPLE_I2S_BCK_IO=12
CONFIG_EXAMPLE_I2S_WS_IO=10
CONFIG_EXAMPLE_I2S_DO_IO=9
CONFIG_EXAMPLE_I2S_DI_IO=11
CONFIG_EXAMPLE_PA_CTRL_IO=53
```

**Description:** Demonstrates ESP32-P4 as a USB speaker/microphone using I2S + ES8311 codec with rate tuning to match UAC speed.

---

### 2.7 Parallel IO (ParLIO)

**Path:** `peripherals/parlio/parlio_tx/`

| Sub-example | Description |
|-------------|-------------|
| `simple_rgb_led_matrix/` | CPU-driven HUB75 RGB LED matrix |
| `advanced_rgb_led_matrix/` | Hardware-auto-refresh HUB75 LED matrix with LVGL |

**Supported Targets:** ESP32-P4, ESP32-C5, ESP32-H2, ESP32-H4

**Description:** Demonstrates the Parallel IO TX unit for high-speed parallel data transfer. The advanced example lets hardware automatically refresh the entire LED matrix screen without CPU intervention.

---

### 2.8 ISP (Image Signal Processor)

**Path:** `peripherals/isp/multi_pipelines/`

**Supported Targets:** ESP32-P4 only

**Description:** Demonstrates the on-chip ISP working with MIPI-CSI camera sensors. ISP features include:

- **AWB** (Auto White Balance) + WBG (White Balance Gain)
- **AF** (Auto-Focus)
- **BF** (Bayer Denoise)
- **BLC** (Black Level Correction)
- **Sharpen**
- **Demosaic**
- **GAMMA correction**
- **Color processing**
- **LSC** (Lens Shading Correction)
- **Crop** (configurable)

**Hardware:** OV5647 or SC2336 camera sensor + EK79007 or ILI9881C LCD + ESP32-P4 devkit.

---

## 3. Display Examples

### 3.1 MIPI-DSI LCD Panel

**Path:** `peripherals/lcd/mipi_dsi/`

**Supported Targets:** ESP32-P4 only

**Description:** Demonstrates MIPI DSI LCD panel driving with LVGL widget display. Uses `esp_lcd` MIPI DSI driver with framebuffer managed by the driver.

**Supported LCD Panels:** ILI9881C, EK79007

**Hardware Connection:**
- DSI_CLK_P/N -- LCD DSI clock lane
- DSI_DAT0_P/N -- LCD DSI data lane 0
- DSI_DAT1_P/N -- LCD DSI data lane 1
- BK_LIGHT -- Backlight control
- Reset -- LCD reset

**Configuration Options:**
- LCD model selection via `menuconfig`
- DMA2D async buffer copy (optional)
- GPIO refresh rate monitoring (optional)

**Component Dependencies:** `lvgl` (v9.4.0), `esp_lcd_ili9881c`, `esp_lcd_ek79007`

**openvela Relevance:** This is the primary reference for implementing MIPI-DSI display support in openvela. The DSI initialization sequence, lane configuration, and panel driver registration patterns are essential for the display subsystem.

---

### 3.2 RGB LCD Panel

**Path:** `peripherals/lcd/rgb_panel/`

**Supported Targets:** ESP32-P4, ESP32-S3

**Description:** RGB interfaced LCD panel with multiple buffer modes and LVGL integration.

**Buffer Modes:**

| Mode | Description |
|------|-------------|
| 1 Frame Buffer + 2 partial LVGL buffers | Lowest memory, extra copy overhead |
| 2 Frame Buffers + direct refresh | No extra copy, higher memory cost |
| 1 Frame Buffer + 2 Bounce Buffers | Fast DMA read, more internal memory |

---

### 3.3 I80 Parallel LCD

**Path:** `peripherals/lcd/i80_controller/`

**Supported Targets:** ESP32, ESP32-P4, ESP32-S2, ESP32-S3

**Description:** Intel 8080 parallel interface LCD with LVGL. Supports ST7789, NT35510, ILI9341 panels. Images can be loaded from LittleFS or embedded binary data.

---

### 3.4 ParLIO Simulated LCD

**Path:** `peripherals/lcd/parlio_simulate/`

**Supported Targets:** ESP32-C5, ESP32-H2, ESP32-H4, ESP32-P4

**Description:** Uses Parallel IO peripheral to simulate SPI (1-bit) or I80 (8-bit) LCD interfaces. Particularly suited for high-speed data transfer scenarios.

---

## 4. Camera Examples

All camera examples use shared components from `peripherals/camera/common_components/`:
- `dsi_init/` -- MIPI DSI initialization helper
- `sensor_init/` -- Camera sensor auto-detection and initialization

### 4.1 DVP Camera + DSI Display

**Path:** `peripherals/camera/dvp_dsi/`

**Supported Targets:** ESP32-P4 only

**Description:** Captures DVP camera sensor signals (8-bit parallel) and displays via MIPI-DSI interface. Auto-detects camera sensors via ESP camera sensor driver.

**Supported Sensors:** OV2640 (RGB565 output), other DVP cameras

**Supported Displays:** EK79007, ILI9881C

**Hardware Connection:**
- Camera: XCLK, D0-D7 (8-bit data), PCLK, VSYNC, HREF/DE, I2C SDA/SCL
- Display: DSI CLK P/N, DSI DATA 0/1 P/N

**Component Dependencies:** `dsi_init`, `sensor_init` (local components)

---

### 4.2 DVP Camera + ISP + DSI Display

**Path:** `peripherals/camera/dvp_isp_dsi/`

**Supported Targets:** ESP32-P4 only

**Description:** Same as DVP+DSI but adds ISP processing for RAW sensor data. Enables all ISP pipelines (AWB, AF, denoise, sharpen, gamma, etc.) and implements dual frame buffer (ping-pong) for tear-free display.

**Supported Sensors:** OV2640, SC030IOT (RAW output sensors)

**Key Feature:** For RAW sensors, ISP is automatically enabled to optimize image quality.

---

### 4.3 MIPI-CSI Camera + ISP + DSI Display

**Path:** `peripherals/camera/mipi_isp_dsi/`

**Supported Targets:** ESP32-P4 only

**Description:** MIPI-CSI camera capture with ISP processing and DSI display output. This is the highest-quality camera pipeline example.

**Supported Sensors:** OV5647 (has own ISP), SC2336 (no ISP, uses ESP on-chip ISP)

**Hardware Connection:**
- Camera: CSI CLK P/N, CSI DATA 0/1 P/N, I2C SDA/SCL
- Display: DSI CLK P/N, DSI DATA 0/1 P/N

**openvela Relevance:** This is the most comprehensive reference for implementing a camera-to-display pipeline. The ISP configuration, CSI/DSI lane setup, and ping-pong buffering patterns are critical for openvela multimedia support.

---

### 4.4 DVP Camera + SPI LCD

**Path:** `peripherals/camera/dvp_spi_lcd/`

**Supported Targets:** ESP32-P4, ESP32-S3

**Description:** DVP camera capture with SPI LCD display output. Simpler than DSI-based examples, useful for smaller displays.

---

## 5. Audio/Video Examples

### 5.1 H.264 Encoder/Decoder

**Path:** `peripherals/h264/`

**Supported Targets:** ESP32-P4 (hardware), ESP32-S3 (software)

**Description:** Demonstrates H.264 hardware encoder (ESP32-P4) and software decoder. Generates test patterns, encodes with H.264, decodes, and displays side-by-side comparison.

**ESP32-P4 Hardware Encoder Capabilities:**

| Parameter | Range | Default |
|-----------|-------|---------|
| Resolution | 64x64 to 1920x1080 | 320x240 |
| Frame Rate | 1-60 fps | 30 fps |
| Bitrate | 64K-10M bps | 512K bps |
| GOP Size | 1-255 | 30 |
| QP Value | 10-51 | 26 |

**Video Formats:**
- `ESP_H264_RAW_FMT_I420` -- Planar YUV 4:2:0 (decoder output, SW encoder input)
- `ESP_H264_RAW_FMT_O_UYY_E_VYY` -- Interlaced YUV (HW encoder input on ESP32-P4)

**Component Dependencies:** `esp_h264` (^1.0.4)

**openvela Relevance:** H.264 hardware codec is a key ESP32-P4 differentiator. Adapting this requires registering a V4L2-like or custom video codec device in NuttX, mapping the ESP H.264 API to openvela's multimedia framework.

---

### 5.2 JPEG Hardware Encode

**Path:** `peripherals/jpeg/jpeg_encode/`

**Supported Targets:** ESP32-P4 only

**Description:** Hardware-accelerated JPEG encoding of 1080p images. Reads raw RGB data from SD card and outputs JPEG files.

**Usage:** Place `esp1080.rgb` on SD card, run example, output `outjpg.jpg` on SD card.

**Component Dependencies:** Requires SPIRAM, SD card support

---

### 5.3 JPEG Hardware Decode

**Path:** `peripherals/jpeg/jpeg_decode/`

**Supported Targets:** ESP32-P4 only

**Description:** Hardware-accelerated JPEG decoding of 1080p and 720p images. Reads JPEG from SD card and outputs raw RGB data.

**Usage:** Place `esp1080.jpg` and `esp720.jpg` on SD card, run example, outputs `out.rgb` and `out2.rgb`.

**Helper Script:** `open_raw_picture.py` for visualizing output on PC.

**openvela Relevance:** JPEG hardware codec can be exposed as a `/dev/jpeg` device in NuttX for camera capture and display pipelines.

---

## 6. Connectivity Examples

### 6.1 WiFi via ESP-Hosted

ESP32-P4 lacks built-in WiFi/Bluetooth. WiFi connectivity is achieved through a **two-chip solution** using ESP-Hosted.

**Architecture:**
```
ESP32-P4 (Host)  <--SDIO/SPI-->  ESP32-C6 (Co-processor with WiFi/BT)
```

**Required Components (auto-downloaded via idf_component.yml):**

| Component | Version | Purpose |
|-----------|---------|---------|
| `espressif/esp_hosted` | ~2 | ESP-Hosted host driver |
| `espressif/esp_wifi_remote` | >=0.10,<2.0 | Remote WiFi API shim |
| `espressif/esp-extconn` | ~0.3.0 | External connectivity (ESP32-P4 only) |

**WiFi Examples with ESP-Hosted Support:**

| Example | Path | Description |
|---------|------|-------------|
| Station | `wifi/getting_started/station/` | Connect to AP |
| SoftAP | `wifi/getting_started/softAP/` | Create WiFi hotspot |
| SoftAP+STA | `wifi/softap_sta/` | Simultaneous AP and station |
| Scan | `wifi/scan/` | Scan for available APs |
| Fast Scan | `wifi/fast_scan/` | Fast AP scanning |
| iperf | `wifi/iperf/` | WiFi performance benchmark |
| ESP-NOW | `wifi/espnow/` | Connectionless protocol |
| Power Save | `wifi/power_save/` | WiFi power save mode |
| Roaming | `wifi/roaming/` | WiFi roaming capabilities |

**Setup on ESP32-P4-Function-EV-Board:**

1. The on-board ESP32-C6 is pre-flashed with ESP-Hosted co-processor firmware
2. ESP32-P4 communicates with ESP32-C6 via SDIO bus
3. Build and flash only the host example:

```bash
idf.py set-target esp32p4
idf.py menuconfig  # Set Component config -> Wi-Fi Remote -> slave target = esp32c6
idf.py -p PORT flash monitor
```

**openvela Relevance:** WiFi via ESP-Hosted requires adapting the NuttX network stack to work with the ESP-Hosted driver. The `esp_wifi_remote` component provides a shim API that mirrors the standard `esp_wifi` API, which simplifies the adaptation. Key challenge: SDIO communication between ESP32-P4 and ESP32-C6 needs a NuttX SDIO host driver.

---

### 6.2 Bluetooth

**Path:** `bluetooth/`

| Example | Description |
|---------|-------------|
| `ble_get_started/bluedroid/` | BLE with Bluedroid stack (Beacon, Connection, GATT Server) |
| `ble_get_started/nimble/` | BLE with NimBLE stack |
| `ble_uart_service/` | BLE UART (NUS) service |
| `blufi/` | BLE WiFi provisioning |
| `esp_hid_device/` | BLE HID device |
| `esp_hid_host/` | BLE HID host |
| `esp_ble_mesh/` | BLE Mesh networking |
| `nimble/bleprph_host_only/` | NimBLE peripheral (host only, uses ESP-Hosted for BT) |

**ESP32-P4 Bluetooth Notes:**
- ESP32-P4 does not have built-in Bluetooth
- Bluetooth is available via ESP-Hosted co-processor (ESP32-C6)
- The `nimble/bleprph_host_only` example specifically uses ESP-Hosted network-adapter on ESP32-C6 connected via UART

---

### 6.3 SDIO Host

**Path:** `peripherals/sdio/host/`

**Supported Targets:** ESP32-P4 (with ESP32-C5 SDIO test runner)

**ESP32-P4 Status:** Currently disabled in build rules (TODO IDF-8357 -- "request p4 essl spi to be ready for build")

**Description:** SDIO host interface for communicating with SDIO slave devices. On ESP32-P4, this is used for the SDIO bus to the ESP32-C6 co-processor.

---

## 7. Storage Examples

ESP32-P4 supports standard storage examples:

| Example | Path | Description |
|---------|------|-------------|
| FAT on SDMMC | `storage/sd_card/sdmmc/` | SD card via SDMMC interface |
| FAT on SDSPI | `storage/sd_card/sdspi/` | SD card via SPI interface |
| NVS | `storage/nvs/` | Non-Volatile Storage (key-value) |
| SPIFFS | `storage/spiffs/` | SPI Flash File System |
| LittleFS | `storage/littlefs/` | LittleFS on flash |
| Wear Levelling | `storage/wear_levelling/` | Flash wear levelling |

---

## 8. System Examples

| Example | Path | ESP32-P4 | Description |
|---------|------|----------|-------------|
| Hello World | `get-started/hello_world/` | Yes | Basic startup |
| Blink | `get-started/blink/` | Yes (has sdkconfig) | GPIO LED blink |
| System View Tracing | `system/sysview_tracing_heap_log/` | Yes (has sdkconfig) | Heap tracing |

---

## 9. Adapting Examples for openvela

### 9.1 Architecture Mapping

ESP-IDF and openvela/NuttX have fundamentally different driver models:

| Aspect | ESP-IDF | openvela (NuttX) |
|--------|---------|------------------|
| Driver Registration | Component init | `register_driver()` / `register_blockdriver()` |
| Device Access | Direct API calls | VFS file operations (`open/read/write/ioctl`) |
| Initialization | Auto-init via `idf_component.yml` | Explicit board-level init calls |
| Memory Model | Heap + PSRAM malloc | Kernel/user space, heap regions |
| Threading | FreeRTOS tasks | NuttX tasks/threads |
| Interrupt Handling | `esp_intr_alloc` | NuttX `irq_attach` + `up_enable_irq` |
| DMA | ESP DMA engine | NuttX DMA abstraction or direct |

### 9.2 Driver Adaptation Strategy

**Step 1: Identify the ESP-IDF component API**

Each ESP-IDF example uses specific driver APIs:
- Camera: `esp_cam_sensor`, `esp_driver_cam` (MIPI-CSI, DVP)
- Display: `esp_lcd` (MIPI-DSI, RGB, I80, SPI)
- Codec: `esp_h264`, `esp_driver_jpeg`
- Connectivity: `esp_hosted`, `esp_wifi_remote`

**Step 2: Map to NuttX driver interface**

| ESP-IDF API | NuttX Equivalent |
|-------------|------------------|
| `gpio_config()` + `gpio_isr_handler_add()` | `ioctl(fd, GPIOC_*)` + `gpio_irq_attach()` |
| `i2c_master_cmd_begin()` | `i2c_transfer(dev, msgs, count)` |
| `spi_device_transmit()` | `SPI_EXCHANGE(dev, tx, rx, len)` |
| `uart_driver_install()` | `uart_register(path, &uart_dev)` |
| `esp_lcd_panel_*` | Custom frame buffer driver or `/dev/fbN` |
| `esp_cam_sensor_*` | V4L2-like video device or custom sensor driver |

**Step 3: Implement board-level initialization**

In openvela, peripheral initialization happens in board code:
```
nuttx/boards/risc-v/esp32p4/<board>/src/
```

Call driver init functions from `board_app_init()` or `board_late_initialize()`.

### 9.3 Display Pipeline Adaptation

The MIPI-DSI display pipeline from ESP-IDF examples follows this sequence:

1. **Initialize MIPI DSI bus** -- Configure lanes, clock, PHY
2. **Create LCD panel** -- Attach DSI device, set resolution/timing
3. **Initialize display driver** -- Send init commands to LCD controller (ILI9881C/EK79007)
4. **Allocate framebuffer** -- In PSRAM for large resolutions
5. **Start LVGL or direct rendering** -- Periodic flush to display

For openvela, this maps to:
1. Board-level DSI init in `board_late_initialize()`
2. Register as `/dev/fbN` frame buffer device
3. LVGL or other GUI framework connects via frame buffer

### 9.4 Connectivity Adaptation

WiFi via ESP-Hosted on openvela requires:

1. **SDIO Host Driver** -- NuttX SDIO master driver for ESP32-P4
2. **ESP-Hosted Port** -- Port the `esp_hosted` component to NuttX (replace FreeRTOS primitives)
3. **Network Stack Integration** -- Connect ESP-Hosted network interface to NuttX `netdev`
4. **WiFi API Shim** -- The `esp_wifi_remote` component provides a standard WiFi API

### 9.5 Key Differences: ESP-IDF vs openvela

| Feature | ESP-IDF Approach | openvela Approach |
|---------|------------------|-------------------|
| Build System | CMake + idf.py | Make + Kconfig |
| Component Management | `idf_component.yml` auto-download | Manual integration or package manager |
| SDK Configuration | `sdkconfig` / `menuconfig` | `defconfig` / `menuconfig` |
| PSRAM Access | `heap_caps_malloc(MALLOC_CAP_SPIRAM)` | Heap region configuration |
| FreeRTOS API | `xTaskCreate`, `xSemaphoreTake` | `nxtask_create`, `nxsem_wait` |
| Cache Management | `esp_cache_msync()` | Architecture-specific cache ops |

---

## 10. Quick Reference Table

| Example | Path | ESP32-P4 | Category | Priority for openvela |
|---------|------|----------|----------|----------------------|
| GPIO | `peripherals/gpio/generic_gpio/` | Yes | Peripheral | High |
| I2C Basic | `peripherals/i2c/i2c_basic/` | Yes | Peripheral | High |
| SPI Master | `peripherals/spi_master/` | Yes | Peripheral | High |
| UART Echo | `peripherals/uart/uart_echo/` | Yes | Peripheral | High |
| USB Host UVC | `peripherals/usb/host/uvc/` | Yes | Peripheral | Medium |
| USB Device | `peripherals/usb/device/tusb_serial_device/` | Yes | Peripheral | Medium |
| I2S USB | `peripherals/i2s/i2s_advance/i2s_usb/` | Yes | Audio | Medium |
| ParLIO LED | `peripherals/parlio/parlio_tx/advanced_rgb_led_matrix/` | Yes | Peripheral | Low |
| ISP Multi | `peripherals/isp/multi_pipelines/` | Yes | Camera | High |
| MIPI-DSI LCD | `peripherals/lcd/mipi_dsi/` | Yes | Display | High |
| RGB Panel | `peripherals/lcd/rgb_panel/` | Yes | Display | Medium |
| I80 LCD | `peripherals/lcd/i80_controller/` | Yes | Display | Medium |
| ParLIO LCD | `peripherals/lcd/parlio_simulate/` | Yes | Display | Low |
| DVP+DSI | `peripherals/camera/dvp_dsi/` | Yes | Camera | High |
| DVP+ISP+DSI | `peripherals/camera/dvp_isp_dsi/` | Yes | Camera | High |
| MIPI+ISP+DSI | `peripherals/camera/mipi_isp_dsi/` | Yes | Camera | High |
| DVP+SPI LCD | `peripherals/camera/dvp_spi_lcd/` | Yes | Camera | Medium |
| H.264 | `peripherals/h264/` | Yes | Video | High |
| JPEG Encode | `peripherals/jpeg/jpeg_encode/` | Yes | Video | High |
| JPEG Decode | `peripherals/jpeg/jpeg_decode/` | Yes | Video | High |
| WiFi (ESP-Hosted) | `wifi/getting_started/station/` | Yes | Connectivity | High |
| WiFi iperf | `wifi/iperf/` | Yes | Connectivity | Medium |
| BLE NimBLE | `bluetooth/nimble/` | Via ESP-Hosted | Connectivity | Medium |
| Blink | `get-started/blink/` | Yes | Getting Started | High |

---

## Appendix: ESP32-P4 Pin Reference (from examples)

| Function | GPIO | Example |
|----------|------|---------|
| I2C SDA | 7 | I2S USB example |
| I2C SCL | 8 | I2S USB example |
| I2S MCK | 13 | I2S USB example |
| I2S BCK | 12 | I2S USB example |
| I2S WS | 10 | I2S USB example |
| I2S DO | 9 | I2S USB example |
| I2S DI | 11 | I2S USB example |
| PA Control | 53 | I2S USB example |
| SDIO (to ESP32-C6) | Board-specific | ESP-Hosted WiFi |
| MIPI-DSI CLK P/N | Board-specific | Display examples |
| MIPI-DSI DATA 0/1 P/N | Board-specific | Display examples |
| MIPI-CSI CLK P/N | Board-specific | Camera examples |
| MIPI-CSI DATA 0/1 P/N | Board-specific | Camera examples |
| DVP D0-D7 | Board-specific | DVP camera examples |
| DVP PCLK | Board-specific | DVP camera examples |
| DVP VSYNC | Board-specific | DVP camera examples |
| DVP HREF/DE | Board-specific | DVP camera examples |

---

## References

- [ESP-IDF Camera Controller Driver](https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/api-reference/peripherals/camera_driver.html)
- [ESP-IDF LCD DSI Driver](https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/api-reference/peripherals/lcd/dsi_lcd.html)
- [ESP-IDF JPEG Driver](https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/api-reference/peripherals/jpeg.html)
- [ESP-Hosted Documentation](https://github.com/espressif/esp-hosted-mcu/blob/main/docs/esp32_p4_function_ev_board.md)
- [ESP Component Registry](https://components.espressif.com)
- [openvela Porting Guide](https://github.com/open-vela/docs/blob/dev-ai-contest-2026/zh-cn/chip_porting/porting_guide.md)
- [openvela Driver Development Guide](https://github.com/open-vela/docs/blob/dev-ai-contest-2026/zh-cn/device_dev_guide/driver/driver_development.md)
