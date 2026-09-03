# ESP32-P4 Function EV Board 硬件连接指南

**文档版本：** v1.0.0
**创建日期：** 2026-09-03
**最后更新：** 2026-09-03
**维护者：** geo@openvela

---

## 概述

本文档提供 ESP32-P4 Function EV Board 的详细硬件信息，包括引脚定义、外设连接、电源要求等，帮助开发者快速上手硬件平台。

---

## 目录

1. [开发板概述](#1-开发板概述)
2. [引脚定义](#2-引脚定义)
3. [外设连接](#3-外设连接)
4. [电源要求](#4-电源要求)
5. [调试接口](#5-调试接口)
6. [扩展接口](#6-扩展接口)
7. [注意事项](#7-注意事项)

---

## 1. 开发板概述

### 1.1 板卡规格

| 项目 | 规格 |
|------|------|
| **芯片** | ESP32-P4 |
| **CPU** | 双核 RISC-V (HP Core 400MHz + LP Core 40MHz) |
| **内存** | 768 KB SRAM + 32 MB PSRAM |
| **存储** | 16 MB Flash |
| **尺寸** | 约 100mm x 60mm |
| **供电** | USB 5V 或外部 3.3V |

### 1.2 板载资源

| 资源 | 说明 |
|------|------|
| **LED** | 1 个可编程 LED (GPIO 26) |
| **按钮** | BOOT 按钮 (GPIO 21) + RST 按钮 |
| **USB** | 2 个 USB-C 接口 (USB-JTAG + USB-UART) |
| **SD 卡槽** | SDMMC 接口 SD 卡槽 |
| **显示屏** | MIPI-DSI 接口 |
| **摄像头** | MIPI-CSI 接口 |
| **音频** | I2S 音频接口 |

---

## 2. 引脚定义

### 2.1 GPIO 引脚

| GPIO | 功能 | 方向 | 说明 |
|------|------|------|------|
| 0 | SPI Flash | - | 不可用作通用 GPIO |
| 1 | SPI Flash | - | 不可用作通用 GPIO |
| 2 | SPI Flash | - | 不可用作通用 GPIO |
| 3 | SPI Flash | - | 不可用作通用 GPIO |
| 4 | SPI Flash | - | 不可用作通用 GPIO |
| 5 | SPI Flash | - | 不可用作通用 GPIO |
| 6 | SPI Flash | - | 不可用作通用 GPIO |
| 7 | SPI Flash | - | 不可用作通用 GPIO |
| 21 | BOOT 按钮 | 输入 | 内部上拉，低电平有效 |
| 26 | 板载 LED | 输出 | 高电平点亮 |
| 39 | SDMMC D0 | I/O | SD 卡数据线 0 |
| 40 | SDMMC D1 | I/O | SD 卡数据线 1 |
| 41 | SDMMC D2 | I/O | SD 卡数据线 2 |
| 42 | SDMMC D3 | I/O | SD 卡数据线 3 |
| 43 | SDMMC CLK | 输出 | SD 卡时钟 |
| 44 | SDMMC CMD | I/O | SD 卡命令线 |

### 2.2 UART 引脚

| UART | TX GPIO | RX GPIO | 说明 |
|------|---------|---------|------|
| UART0 | GPIO 16 | GPIO 17 | 默认串口控制台 |
| UART1 | GPIO 18 | GPIO 19 | 通用 UART |

### 2.3 I2C 引脚

| I2C | SDA GPIO | SCL GPIO | 说明 |
|-----|----------|----------|------|
| I2C0 | GPIO 8 | GPIO 9 | 默认 I2C |
| I2C1 | GPIO 10 | GPIO 11 | 通用 I2C |

### 2.4 SPI 引脚

| SPI | MOSI GPIO | MISO GPIO | CLK GPIO | CS GPIO | 说明 |
|-----|-----------|-----------|----------|---------|------|
| SPI2 | GPIO 12 | GPIO 13 | GPIO 14 | GPIO 15 | 默认 SPI |
| SPI3 | GPIO 22 | GPIO 23 | GPIO 24 | GPIO 25 | 通用 SPI |

---

## 3. 外设连接

### 3.1 SD 卡连接

**硬件连接：**

| SD 卡引脚 | ESP32-P4 GPIO | 说明 |
|-----------|---------------|------|
| CLK | GPIO 43 | 时钟 |
| CMD | GPIO 44 | 命令 |
| D0 | GPIO 39 | 数据 0 |
| D1 | GPIO 40 | 数据 1 |
| D2 | GPIO 41 | 数据 2 |
| D3 | GPIO 42 | 数据 3 |
| VDD | 3.3V | 电源 |
| VSS | GND | 地 |

**软件配置：**

```
CONFIG_ESP32P4_SDMMC=y
CONFIG_ESP32P4_SDMMC_DMA=y
CONFIG_ESP32P4_SDMMC_FREQ=40000000
CONFIG_ESP32P4_EVB_SDCARD=y
CONFIG_ESP32P4_EVB_SDCARD_AUTOMOUNT=y
CONFIG_ESP32P4_EVB_SDCARD_MOUNTPOINT="/mnt/sdcard"
```

**验证命令：**

```bash
nsh> ls /dev/mmcsd*
nsh> mount -t vfat /dev/mmcsd0 /mnt/sdcard
nsh> ls /mnt/sdcard/
```

### 3.2 MIPI-DSI 显示屏连接

**支持的显示屏：**

| 型号 | 分辨率 | 尺寸 | 接口 |
|------|--------|------|------|
| ILI9881C | 800x1280 | 5 英寸 | MIPI-DSI |
| EK79007 | 1024x600 | 7 英寸 | MIPI-DSI |

**连接方式：**

1. 使用 FPC 排线连接显示屏到 MIPI-DSI 接口
2. 连接触摸屏 I2C 接口 (可选)
3. 连接背光电源

**软件配置：**

```
CONFIG_ESP32P4_LCD_PANEL=y
CONFIG_ESP32P4_LCD_PANEL_ILI9881C=y
CONFIG_ESP32P4_MIPI_DSI_LANES=2
CONFIG_ESP32P4_MIPI_DSI_LANE_RATE_MBPS=1000
CONFIG_ESP32P4_LCD_HRES=800
CONFIG_ESP32P4_LCD_VRES=1280
CONFIG_ESP32P4_LCD_BPP=16
```

**验证命令：**

```bash
nsh> ls /dev/fb0
nsh> dd if=/dev/zero of=/dev/fb0 bs=1024 count=1200
```

### 3.3 BLE 蓝牙连接

**硬件要求：**

ESP32-P4 没有内置蓝牙射频，需要外部 BLE 协处理器：
- ESP32-C6
- ESP32-H2
- 或其他 BLE 模块

**连接方式：**

| ESP32-P4 | BLE 协处理器 | 说明 |
|----------|--------------|------|
| GPIO 18 (UART1 TX) | HCI UART RX | HCI 命令/数据 |
| GPIO 19 (UART1 RX) | HCI UART TX | HCI 事件/数据 |
| GPIO 20 | RESET | 复位信号 |
| 3.3V | VCC | 电源 |
| GND | GND | 地 |

**软件配置：**

```
CONFIG_ESP32P4_BLE=y
CONFIG_NIMBLE=y
CONFIG_NIMBLE_HOST=y
```

**验证命令：**

```bash
nsh> ls /dev/ble*
nsh> ble_scan
```

### 3.4 I2C 外设连接

**常见 I2C 外设：**

| 外设 | I2C 地址 | 说明 |
|------|----------|------|
| 温湿度传感器 (SHT30) | 0x44 | 温湿度测量 |
| 加速度计 (ADXL345) | 0x53 | 运动检测 |
| OLED 显示 (SSD1306) | 0x3C | 0.96 寸 OLED |
| EEPROM (AT24C256) | 0x50 | 存储数据 |

**连接方式：**

| ESP32-P4 | I2C 外设 | 说明 |
|----------|----------|------|
| GPIO 8 (I2C0 SDA) | SDA | 数据线 |
| GPIO 9 (I2C0 SCL) | SCL | 时钟线 |
| 3.3V | VCC | 电源 |
| GND | GND | 地 |

**注意事项：**

- I2C 总线需要上拉电阻 (通常 4.7KΩ)
- 多个外设可以共享同一 I2C 总线
- 确保 I2C 地址不冲突

### 3.5 SPI 外设连接

**常见 SPI 外设：**

| 外设 | 说明 |
|------|------|
| SPI Flash (W25Q128) | 16MB Flash |
| TFT 显示屏 | SPI 接口显示屏 |
| SD 卡 (SPI 模式) | SD 卡读写 |
| 无线模块 (NRF24L01) | 2.4G 无线 |

**连接方式：**

| ESP32-P4 | SPI 外设 | 说明 |
|----------|----------|------|
| GPIO 12 (SPI2 MOSI) | MOSI | 主出从入 |
| GPIO 13 (SPI2 MISO) | MISO | 主入从出 |
| GPIO 14 (SPI2 CLK) | SCLK | 时钟 |
| GPIO 15 (SPI2 CS) | CS | 片选 |
| 3.3V | VCC | 电源 |
| GND | GND | 地 |

---

## 4. 电源要求

### 4.1 供电方式

| 方式 | 电压 | 电流 | 说明 |
|------|------|------|------|
| USB 供电 | 5V | 500mA+ | 通过 USB-C 接口 |
| 外部供电 | 3.3V | 500mA+ | 通过排针 |
| 电池供电 | 3.7V | - | 锂电池 |

### 4.2 电源引脚

| 引脚 | 电压 | 说明 |
|------|------|------|
| 3V3 | 3.3V | 3.3V 电源输出 |
| GND | 0V | 地 |
| 5V | 5V | USB 5V 输入 |
| VIN | 3.3-5V | 外部电源输入 |

### 4.3 电源注意事项

1. **最大电流**：ESP32-P4 峰值电流可达 500mA，确保电源供应充足
2. **去耦电容**：建议在电源引脚附近添加 100nF 去耦电容
3. **电源顺序**：先连接电源，再连接调试接口
4. **电压范围**：不要超过 3.6V，否则可能损坏芯片

---

## 5. 调试接口

### 5.1 USB-JTAG 接口

**位置：** 板载 USB-C (标记为 USB)

**功能：**
- JTAG 调试
- Flash 烧录
- OpenOCD 调试

**驱动安装：**

```bash
# Linux (通常自动识别)
ls -la /dev/ttyACM0

# Windows
# 安装 Zadig 或 libusb 驱动

# macOS
# 通常自动识别
```

**OpenOCD 配置：**

```tcl
# esp32p4-evb.cfg
adapter driver esp_usb_jtag
esp_usb_jtag vid_pid 0x303a 0x1001
esp_usb_jtag speed 20000

target create esp32p4.riscv esp32p4 -endian little -chain-position esp32p4.riscv

flash bank esp32p4.flash0 esp32p4 0x42000000 0 0 0 esp32p4.flash0

init
```

### 5.2 USB-UART 接口

**位置：** 板载 USB-C (标记为 UART)

**功能：**
- 串口控制台
- 串口通信
- 烧录 (备用)

**串口配置：**

| 参数 | 值 |
|------|-----|
| 波特率 | 115200 |
| 数据位 | 8 |
| 停止位 | 1 |
| 校验 | 无 |
| 流控 | 无 |

**串口工具：**

```bash
# minicom
minicom -D /dev/ttyUSB0 -b 115200

# screen
screen /dev/ttyUSB0 115200

# picocom
picocom -b 115200 /dev/ttyUSB0

# ESP-IDF monitor
idf.py -p /dev/ttyUSB0 monitor
```

---

## 6. 扩展接口

### 6.1 排针引脚

ESP32-P4 EVB 提供两排 2.54mm 排针，用于扩展连接。

**排针 A (左侧)：**

| 引脚 | GPIO | 功能 | 说明 |
|------|------|------|------|
| 1 | 3V3 | 电源 | 3.3V 输出 |
| 2 | GPIO 8 | I2C0 SDA | I2C 数据 |
| 3 | GPIO 9 | I2C0 SCL | I2C 时钟 |
| 4 | GPIO 10 | I2C1 SDA | I2C 数据 |
| 5 | GPIO 11 | I2C1 SCL | I2C 时钟 |
| 6 | GPIO 12 | SPI2 MOSI | SPI 数据 |
| 7 | GPIO 13 | SPI2 MISO | SPI 数据 |
| 8 | GPIO 14 | SPI2 CLK | SPI 时钟 |
| 9 | GPIO 15 | SPI2 CS | SPI 片选 |
| 10 | GND | 地 | 接地 |

**排针 B (右侧)：**

| 引脚 | GPIO | 功能 | 说明 |
|------|------|------|------|
| 1 | GPIO 16 | UART0 TX | 串口发送 |
| 2 | GPIO 17 | UART0 RX | 串口接收 |
| 3 | GPIO 18 | UART1 TX | 串口发送 |
| 4 | GPIO 19 | UART1 RX | 串口接收 |
| 5 | GPIO 20 | GPIO | 通用 IO |
| 6 | GPIO 21 | BOOT | 启动按钮 |
| 7 | GPIO 22 | SPI3 MOSI | SPI 数据 |
| 8 | GPIO 23 | SPI3 MISO | SPI 数据 |
| 9 | GPIO 24 | SPI3 CLK | SPI 时钟 |
| 10 | GND | 地 | 接地 |

### 6.2 FPC 接口

| 接口 | 引脚数 | 功能 |
|------|--------|------|
| MIPI-DSI | 30pin | 显示屏接口 |
| MIPI-CSI | 24pin | 摄像头接口 |
| SDMMC | 8pin | SD 卡接口 |

---

## 7. 注意事项

### 7.1 GPIO 使用限制

1. **Flash 引脚**：GPIO 0-7 用于 SPI Flash，不可用作通用 GPIO
2. **启动引脚**：GPIO 21 为 BOOT 按钮，上电时影响启动模式
3. **电源引脚**：3V3、GND、5V 引脚不可用作 GPIO

### 7.2 电平兼容性

1. **工作电压**：ESP32-P4 工作电压为 3.3V
2. **IO 电平**：GPIO 电平为 3.3V，不可直接连接 5V 设备
3. **电平转换**：连接 5V 设备需要电平转换器

### 7.3 抗干扰措施

1. **电源滤波**：在电源引脚添加去耦电容
2. **信号完整性**：高速信号线尽量短，避免平行走线
3. **接地**：确保良好的接地连接
4. **屏蔽**：敏感信号使用屏蔽线

### 7.4 热设计

1. **散热**：长时间高负载运行需要散热措施
2. **温度范围**：工作温度 -40°C ~ 85°C
3. **温度监控**：可以使用内部温度传感器监控芯片温度

---

## 附录

### A. 常见问题

**Q1: 如何进入下载模式？**

A1: 按住 BOOT 按钮 -> 短按 RST 按钮 -> 松开 BOOT 按钮

**Q2: 如何恢复出厂固件？**

A2: 使用 esptool.py 擦除 Flash：
```bash
esptool.py --chip esp32p4 --port /dev/ttyACM0 erase_flash
```

**Q3: 如何扩展 GPIO 数量？**

A3: 可以使用 I2C 或 SPI GPIO 扩展器：
- PCF8574 (I2C, 8 位)
- MCP23017 (I2C, 16 位)
- 74HC595 (SPI, 8 位)

### B. 参考资源

- [ESP32-P4 技术参考手册](https://www.espressif.com/zh-hans/products/socs/esp32-p4)
- [ESP32-P4 开发板文档](https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/esp32p4/esp32-p4x-function-ev-board/user_guide.html)
- [ESP-IDF 编程指南](https://docs.espressif.com/projects/esp-idf/)

---

*文档维护者：geo@openvela | 最后更新：2026-09-03*
