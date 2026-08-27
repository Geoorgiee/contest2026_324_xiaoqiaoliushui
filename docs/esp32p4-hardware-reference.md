# ESP32-P4 Hardware Reference Guide

This document provides a comprehensive hardware reference for the Espressif ESP32-P4 system-on-chip, compiled from the ESP-IDF v6.0.2 source code and documentation.

## Table of Contents

- [1. Chip Overview](#1-chip-overview)
- [2. CPU and Processing](#2-cpu-and-processing)
- [3. Memory Layout](#3-memory-layout)
- [4. GPIO and Pin Configuration](#4-gpio-and-pin-configuration)
- [5. Clock System](#5-clock-system)
- [6. Power Management](#6-power-management)
- [7. Peripheral Interfaces](#7-peripheral-interfaces)
- [8. Boot Process](#8-boot-process)
- [9. Security Features](#9-security-features)
- [10. DMA System](#10-dma-system)

---

## 1. Chip Overview

The ESP32-P4 is a high-performance RISC-V based SoC from Espressif Systems, designed for advanced embedded applications requiring multimedia processing, high-speed connectivity, and low-power operation.

### Key Features

- **CPU**: Dual-core RISC-V processor with FPU, hardware loop, and PIE coprocessor
- **Clock**: Up to 360 MHz CPU clock (from CPLL)
- **GPIO**: 55 GPIO pins (GPIO0-GPIO54)
- **USB**: Dual USB OTG (1x High-Speed USB DWC v4.30a, 1x Full-Speed USB DWC)
- **Display**: MIPI-DSI and MIPI-CSI interfaces
- **Memory**: Support for PSRAM, Flash, and internal SRAM
- **Connectivity**: EMAC (Ethernet MAC), SDMMC, SPI, I2C, UART, I2S
- **Security**: AES, SHA, RSA, ECC, ECDSA, HMAC, Digital Signature, Secure Boot, Flash Encryption
- **Low Power**: LP Core, LP peripherals, deep sleep support

### Supported Features Summary

| Category | Features |
|----------|----------|
| **CPU** | Dual RISC-V cores, FPU, hardware loop, PIE coprocessor |
| **DMA** | AHB-GDMA, AXI-GDMA, DW-GDMA, DMA2D |
| **Display** | MIPI-DSI, MIPI-CSI, RGB LCD, I80 LCD, ISP |
| **Connectivity** | EMAC, SDMMC (2 slots), USB OTG, USB Serial/JTAG |
| **Audio** | I2S (3 channels), LP I2S, PDM |
| **Storage** | SPI Flash, PSRAM, SDMMC |
| **Security** | AES, SHA, RSA, ECC, ECDSA, HMAC, DS, Key Manager |
| **Timers** | GPTimer, SYSTIMER, LP Timer, WDT |
| **PWM** | LEDC, MCPWM, Sigma-Delta Modulator |
| **Communication** | SPI (3), I2C (3), UART (6), TWAI (3), I3C |
| **Sensing** | ADC (2 units), Touch Sensor, Temperature Sensor |
| **Low Power** | LP Core, LP UART, LP I2C, LP SPI, LP I2S, LP ADC |

---

## 2. CPU and Processing

### Core Architecture

- **Architecture**: RISC-V (RV32IMAFCP)
- **Cores**: 2 HP (High-Performance) cores
- **FPU**: Single-precision floating-point unit
- **Extensions**: Zc (code size reduction), SIMD instructions
- **Interrupt Controller**: CLIC (Core Local Interrupt Controller) with 32 external interrupts
- **Debug**: 3 breakpoints, 3 watchpoints per core

### Memory Protection

- **MPU**: 8 regions, minimum region size 512MB
- **PMA**: Physical Memory Attributes
- **PMP**: Physical Memory Protection with 128-byte granularity

### Coprocessors

- **PIE**: Programmable Instruction Extension (GCC only)
- **HWLOOP**: Hardware loop support
- **FPU**: Floating-point unit with F/D extensions

---

## 3. Memory Layout

### Address Map Overview

| Region | Start Address | End Address | Size | Description |
|--------|---------------|-------------|------|-------------|
| **IROM (Flash)** | 0x40000000 | 0x44000000 | 64MB | Instruction ROM (Flash via cache) |
| **DROM (Flash)** | 0x40000000 | 0x44000000 | 64MB | Data ROM (Flash via cache) |
| **PSRAM** | 0x48000000 | 0x4C000000 | 64MB | External PSRAM |
| **IRAM** | 0x4FF00000 | 0x4FFC0000 | 768KB | Internal instruction RAM |
| **DRAM** | 0x4FF00000 | 0x4FFC0000 | 768KB | Internal data RAM |
| **LP ROM** | 0x50100000 | 0x50104000 | 16KB | Low-power ROM |
| **LP RAM** | 0x50108000 | 0x50110000 | 32KB | Low-power RAM |
| **HP Peripherals** | 0x50000000 | 0x50100000 | 1MB | HP peripheral registers |
| **LP Peripherals** | 0x50110000 | 0x50130000 | 128KB | LP peripheral registers |
| **CPU Subsystem** | 0x20000000 | 0x30000000 | 256MB | CPU configuration |

### Internal Memory Details

- **IRAM**: 768KB (shared between instruction and data)
- **DRAM**: 768KB (shared between instruction and data)
- **LP RAM**: 32KB (for low-power core)
- **LP ROM**: 16KB (boot code for LP domain)
- **SPM**: 8KB at 0x30100000-0x30102000

### DMA-Capable Memory

- **DMA Region**: 0x4FF00000 - 0x4FFC0000 (768KB)
- **Byte-Accessible**: Same as DMA region

### Cache System

- **Type**: Shared instruction/data cache
- **Features**: Writeback support, freeze support
- **L1 Cache**: Access internal memory via L1 cache
- **MMU**: 2 MMU peripherals, 1024 entries each
- **Page Size**: 64KB

### MMU Configuration

| Parameter | Value |
|-----------|-------|
| Page Size | 64KB (0x10000) |
| Flash MMU Entries | 1024 |
| PSRAM MMU Entries | 1024 |
| Max Flash Pages | 32768 (2GB) |
| Max PSRAM Pages | 16384 (1GB) |
| Flash VAddr Base | 0x40000000 |
| PSRAM VAddr Base | 0x48000000 |

### ROM Stack

- **Stack Start (Rev2)**: 0x4FFBCFC0
- **Stack Start (Default)**: 0x4FF3CFC0
- **Stack Size**: 8KB (0x2000)

---

## 4. GPIO and Pin Configuration

### GPIO Overview

- **Total GPIO Pins**: 55 (GPIO0 - GPIO54)
- **GPIO Port**: 1
- **Digital I/O**: GPIO16 - GPIO54 (powered by VDD3P3_CPU or VDD_SPI)
- **LP GPIO**: GPIO0 - GPIO15 (can wake from deep sleep)

### GPIO Capabilities

| Feature | Support |
|---------|---------|
| Pin Count | 55 |
| Glitch Filter | 8 flexible glitch filters |
| Hysteresis Filter | Yes |
| ETM Support | Yes |
| Deep Sleep Wakeup | GPIO0-GPIO15 |
| Clock Output | 2 channels via GPIO matrix |
| Force Hold | Yes |
| Single IO Hold (Deep Sleep) | Yes (Rev >= 3.0) |

### GPIO Signal Mapping

The ESP32-P4 uses a GPIO matrix to route peripheral signals to any GPIO pin. Key signal indices include:

#### UART Signals
| Signal | Index |
|--------|-------|
| UART0_RXD | 10 |
| UART0_TXD | 10 |
| UART1_RXD | 13 |
| UART1_TXD | 13 |
| UART2_RXD | 16 |
| UART2_TXD | 16 |
| UART3_RXD | 19 |
| UART3_TXD | 19 |
| UART4_RXD | 22 |
| UART4_TXD | 22 |

#### SPI Signals
| Signal | Index |
|--------|-------|
| SPI2_CK | 53 |
| SPI2_D | 55 |
| SPI2_Q | 54 |
| SPI2_CS | 62 |
| SPI3_CK | 47 |
| SPI3_D | 49 |
| SPI3_Q | 48 |
| SPI3_CS | 52 |

#### I2C Signals
| Signal | Index |
|--------|-------|
| I2C0_SCL | 68 |
| I2C0_SDA | 69 |
| I2C1_SCL | 70 |
| I2C1_SDA | 71 |

#### I2S Signals
| Signal | Index |
|--------|-------|
| I2S0_BCK | 25 |
| I2S0_MCLK | 26 |
| I2S0_WS | 27 |
| I2S0_SD | 28 |
| I2S1_BCK | 31 |
| I2S1_MCLK | 32 |
| I2S1_WS | 33 |
| I2S1_SD | 34 |
| I2S2_BCK | 37 |
| I2S2_MCLK | 38 |
| I2S2_WS | 39 |
| I2S2_SD | 40 |

#### SDMMC Signals
| Signal | Index |
|--------|-------|
| SD_CCLK | 0 |
| SD_CCMD | 1 |
| SD_CDATA0-7 | 2-9 |

#### USB Signals
| Signal | Index |
|--------|-------|
| USB_OTG11_IDDIG | 110 |
| USB_OTG11_AVALID | 111 |
| USB_SRP_BVALID | 112 |
| USB_OTG11_VBUSVALID | 113 |
| USB_HSPHY_REFCLK | 118 |

#### EMAC Signals
| Signal | Index |
|--------|-------|
| EMAC_PHY_RXDV | 178 |
| EMAC_PHY_TXEN | 178 |
| EMAC_PHY_RXD0-3 | 179-182 |
| EMAC_PHY_TXD0-3 | 179-182 |
| EMAC_RX_CLK | 184 |
| EMAC_TX_CLK | 185 |

#### MIPI CSI/DSI Signals
| Signal | Index |
|--------|-------|
| CAM_PCLK | 158 |
| CAM_H_ENABLE | 159 |
| CAM_H_SYNC | 160 |
| CAM_V_SYNC | 161 |
| CAM_DATA_IN0-15 | 162-177 |
| LCD_PCLK | 149 |
| LCD_H_ENABLE | 151 |
| LCD_H_SYNC | 152 |
| LCD_V_SYNC | 153 |
| LCD_DATA_OUT0-23 | 154-177 |

### Default UART Pin Assignment (IOMUX)

| UART | TX | RX | RTS | CTS |
|------|----|----|-----|-----|
| UART0 | GPIO37 | GPIO38 | GPIO8 | GPIO9 |
| UART1 | GPIO10 | GPIO11 | GPIO12 | GPIO13 |
| UART2-4 | GPIO Matrix | GPIO Matrix | GPIO Matrix | GPIO Matrix |
| LP UART | GPIO14 | GPIO15 | - | - |

### ADC Channel Mapping

| ADC Unit | Channel | GPIO |
|----------|---------|------|
| ADC1 | 0 | GPIO16 |
| ADC1 | 1 | GPIO17 |
| ADC1 | 2 | GPIO18 |
| ADC1 | 3 | GPIO19 |
| ADC1 | 4 | GPIO20 |
| ADC1 | 5 | GPIO21 |
| ADC1 | 6 | GPIO22 |
| ADC1 | 7 | GPIO23 |
| ADC2 | 0 | GPIO49 |
| ADC2 | 1 | GPIO50 |
| ADC2 | 2 | GPIO51 |
| ADC2 | 3 | GPIO52 |
| ADC2 | 4 | GPIO53 |
| ADC2 | 5 | GPIO54 |

### Touch Sensor Channels

| Channel | GPIO |
|---------|------|
| 1 | GPIO2 |
| 2 | GPIO3 |
| 3 | GPIO4 |
| 4 | GPIO5 |
| 5 | GPIO6 |
| 6 | GPIO7 |
| 7 | GPIO8 |
| 8 | GPIO9 |
| 9 | GPIO10 |
| 10 | GPIO11 |
| 11 | GPIO12 |
| 12 | GPIO13 |
| 13 | GPIO14 |
| 14 | GPIO15 |
| 14 (internal) | N/A |

---

## 5. Clock System

### Root Clock Sources

| Clock | Type | Frequency | Description |
|-------|------|-----------|-------------|
| **XTAL** | External Crystal | 40 MHz | Main crystal oscillator |
| **RC_FAST** | Internal RC | ~17.5 MHz | Fast RC oscillator (calibratable) |
| **RC_SLOW** | Internal RC | ~136 kHz | Slow RC oscillator (calibratable) |
| **XTAL32K** | External Crystal | 32.768 kHz | Optional 32kHz crystal |
| **RC32K** | Internal RC | ~32.768 kHz | 32kHz RC oscillator (not recommended) |

### PLL Clocks (from 40MHz XTAL)

| PLL | Default Frequency | Description |
|-----|-------------------|-------------|
| **CPLL** | 320/360/400 MHz | CPU PLL, used for CPU, MSPI |
| **SPLL** | 480 MHz | System PLL (AXI/AHB), used for peripherals |
| **MPLL** | 400 MHz | MSPI PLL, used for PSRAM |
| **APLL** | Configurable | Audio PLL for I2S, LCD, CAM |
| **SDIO PLL** | 200 MHz | SDIO PLL for SDMMC |
| **LP PLL** | 8 MHz | Low-power PLL from 32kHz source |

### Derived Clock Signals

| Clock | Source | Default Frequency |
|-------|--------|-------------------|
| PLL_F240M | SPLL/2 | 240 MHz |
| PLL_F160M | SPLL/3 | 160 MHz |
| PLL_F120M | SPLL/4 | 120 MHz |
| PLL_F80M | SPLL/6 | 80 MHz |
| PLL_F50M | MPLL/10 | 50 MHz |
| PLL_F25M | MPLL/16 | 25 MHz |
| PLL_F20M | SPLL/24 | 20 MHz |

### CPU Clock Configuration

| Max CPU Freq | Lock Acquisition | CPU Freq | APB Freq |
|--------------|------------------|----------|----------|
| 360 MHz | `ESP_PM_CPU_FREQ_MAX` | 360 MHz | 90 MHz |
| 360 MHz | `ESP_PM_APB_FREQ_MAX` | 90 MHz | 90 MHz |
| 360 MHz | None | Min (configurable) | Min (configurable) |

### Peripheral Clock Sources

| Peripheral | Available Clock Sources | Default |
|------------|------------------------|---------|
| UART | PLL_F80M, XTAL, RC_FAST | PLL_F80M |
| LP UART | RC_FAST, XTAL_D2 | RC_FAST |
| SPI | XTAL, RC_FAST, SPLL | SPLL |
| I2C | XTAL, RC_FAST | XTAL |
| I2S | XTAL, PLL_F160M, APLL, External | Auto |
| LCD | PLL_F160M, XTAL, APLL | PLL_F160M |
| CAM | PLL_F160M, XTAL, APLL | PLL_F160M |
| MIPI DSI DPI | XTAL, PLL_F160M, PLL_F240M, APLL | PLL_F240M |
| MIPI CSI PHY | RC_FAST, PLL_F25M, PLL_F20M | PLL_F20M |
| ISP | XTAL, PLL_F160M, PLL_F240M | PLL_F240M |
| SDMMC | PLL_F160M, SDIO_PLL | PLL_F160M |
| PSRAM | XTAL, CPLL, SPLL, MPLL | SPLL |
| Flash | XTAL, CPLL, SPLL | SPLL |
| ADC | XTAL, PLL_F80M, RC_FAST | PLL_F80M |
| LEDC | XTAL, PLL_F80M, RC_FAST | PLL_F80M |
| MCPWM | PLL_F160M, XTAL | PLL_F160M |
| TWAI | XTAL | XTAL |
| SYSTIMER | XTAL, RC_FAST | XTAL |
| GPTimer | PLL_F80M, RC_FAST, XTAL | PLL_F80M |
| PARLIO | XTAL, PLL_F160M, RC_FAST, External | PLL_F160M |
| I3C | XTAL, PLL_F160M | XTAL |
| EMAC PTP | XTAL, PLL_F80M | XTAL |

---

## 6. Power Management

### Power Domains

The ESP32-P4 has multiple power domains:

1. **HP (High-Performance) Domain**: Main CPU and peripherals
2. **LP (Low-Power) Domain**: LP Core, LP peripherals
3. **RTC Domain**: RTC peripherals, always-on logic

### Sleep Modes

| Mode | Description | Wakeup Sources |
|------|-------------|----------------|
| **Active** | Normal operation | N/A |
| **Light Sleep** | CPU suspended, peripherals active | GPIO, UART, Timer, WiFi, Touch |
| **Deep Sleep** | Most logic powered off | GPIO (EXT1), LP UART, Touch, Timer |

### Power Management Capabilities

| Feature | Support |
|---------|---------|
| CPU Power Down | Yes |
| XTAL32K Power Down | Yes |
| RC32K Power Down | Yes |
| RC_FAST Power Down | Yes |
| VDDSDIO Power Down | Yes |
| Top Power Down | Yes |
| Connection Power Down | Yes |
| RTC Peripheral Power Down | Yes |
| CPU Retention (SW) | Yes |
| FPU Retention (SW) | Yes |
| Cache Retention (PAU) | Yes |
| PAU Links | 4 |
| Retention Modules | 64 |

### Wakeup Sources

| Source | Support |
|--------|---------|
| EXT1 Wakeup | Yes (per-pin trigger level) |
| WiFi Wakeup | Yes |
| Touch Sensor Wakeup | Yes |
| LP UART Wakeup | Yes |
| GPIO Wakeup | GPIO0-GPIO15 |

### PMU (Power Management Unit)

- **PMU Registers**: 0x50115000
- **PVT Support**: Yes (Process, Voltage, Temperature monitoring)
- **DCDC Converter**: Yes
- **GP LDO**: Yes (General Purpose LDO)

### Sleep Retention Modules

The following peripherals support register retention during sleep:

| Module | ID | Module | ID |
|--------|-----|--------|-----|
| NULL | 0 | UART0 | 15 |
| CLOCK_SYSTEM | 1 | UART1 | 16 |
| SYS_PERIPH | 2 | UART2 | 17 |
| TG0_WDT | 3 | UART3 | 18 |
| TG1_WDT | 4 | UART4 | 19 |
| TG0_TIMER0 | 5 | RMT0 | 20 |
| TG0_TIMER1 | 6 | I2S0 | 21 |
| TG1_TIMER0 | 7 | I2S1 | 22 |
| TG1_TIMER1 | 8 | I2S2 | 23 |
| AHB_DMA_CH0 | 9 | I2C0 | 24 |
| AHB_DMA_CH1 | 10 | I2C1 | 25 |
| AHB_DMA_CH2 | 11 | ETM0 | 26 |
| AXI_DMA_CH0 | 12 | TWAI0 | 27 |
| AXI_DMA_CH1 | 13 | TWAI1 | 28 |
| AXI_DMA_CH2 | 14 | TWAI2 | 29 |
| PARLIO0 | 30 | GPSPI2 | 31 |
| GPSPI3 | 32 | LEDC | 33 |
| MCPWM0 | 34 | MCPWM1 | 35 |
| SDM0 | 36 | EMAC | 37 |
| JPEG | 38 | LCDCAM | 39 |
| H264 | 40 | | |

---

## 7. Peripheral Interfaces

### 7.1 UART

- **HP UARTs**: 5 (UART0-UART4)
- **LP UART**: 1
- **Total**: 6 UARTs
- **FIFO Size**: 128 bytes (HP), 16 bytes (LP)
- **Max Bitrate**: 5 Mbps
- **Features**: RTC clock, XTAL clock, wakeup interrupt, sleep retention

### 7.2 SPI

- **MSPI (Flash/PSRAM)**: SPI0, SPI1 (dedicated)
- **GPSPI**: SPI2, SPI3 (general purpose)
- **Max CS per SPI**: SPI0=2, SPI1=6, SPI2=3
- **Max Buffer Size**: 64 bytes
- **Features**: DDR clock, octal mode, sleep retention, slave HD ver2

### 7.3 I2C

- **HP I2C**: 2 (I2C0, I2C1)
- **LP I2C**: 1
- **Total**: 3 I2C controllers
- **Features**: 10-bit address, slave broadcast, sleep retention

### 7.4 I2S

- **I2S Controllers**: 3 (I2S0, I2S1, I2S2)
- **LP I2S**: 1
- **Features**: PCM, PDM (TX/RX), TDM, APLL clock, ETM
- **PDM Lines**: TX=2, RX=4 (on I2S0)

### 7.5 USB

#### USB OTG
- **Instances**: 2
  - USB DWC HS (High-Speed) v4.30a: 0x50000000
  - USB DWC FS (Full-Speed): 0x50040000
- **PHY**: 1 FS/LS PHY, 1 UTMI PHY

#### USB Serial/JTAG
- **Base**: 0x500D2000
- **Features**: JTAG debugging, serial communication

### 7.6 MIPI Interfaces

#### MIPI-DSI (Display)
- **Host**: 0x500A0000
- **Bridge**: 0x500A0800
- **Memory**: 0x50105000
- **DPI Clock Sources**: XTAL, PLL_F160M, PLL_F240M, APLL
- **PHY Config Clock**: RC_FAST, PLL_F25M, PLL_F20M

#### MIPI-CSI (Camera)
- **Host**: 0x5009F000
- **Bridge**: 0x5009F800
- **Memory**: 0x50104000
- **PHY Clock Sources**: RC_FAST, PLL_F25M, PLL_F20M

### 7.7 LCD/Camera

- **LCD_CAM**: 0x500DC000
- **LCD Types**: RGB LCD, I80 LCD
- **Camera**: Parallel interface (DVP)
- **ISP Features**: BLC, CCM, Color, Crop, Demosaic, LSC, Sharpen, WBG, AE, AF, AWB

### 7.8 SDMMC

- **Base**: 0x50083000
- **Slots**: 2
- **Max Data Width**: 8-bit
- **Features**: IOMUX (slot 0), GPIO Matrix (slot 1), UHS-I support
- **IO Power**: External control
- **PSRAM DMA**: Capable

### 7.9 EMAC (Ethernet)

- **MAC**: 0x50098000
- **PTP**: 0x50098700 (IEEE 1588v2)
- **DMA**: 0x50099000
- **Features**: MII/RMII via GPIO Matrix, sleep retention

### 7.10 TWAI (CAN)

- **Controllers**: 3 (TWAI0, TWAI1, TWAI2)
- **Base Addresses**: 0x500D7000, 0x500D8000, 0x500D9000
- **Features**: Sleep retention

### 7.11 I3C

- **Master**: 1 (0x500DA000)
- **Slave**: 1 (0x500DB000)
- **Address Table**: 12 entries
- **Command Table**: 12 entries

### 7.12 PARLIO (Parallel I/O)

- **Base**: 0x500CF000
- **Max Data Width**: 16 bits (TX and RX)
- **Features**: Clock gating, loop transmission, sleep retention, I80 LCD support

### 7.13 JPEG Codec

- **Base**: 0x50086000
- **Features**: Hardware JPEG encode and decode

### 7.14 H264 Encoder

- **Base**: Hardware H264 encoding support

### 7.15 PPA (Pixel Processing Accelerator)

- **Base**: 0x50087000

### 7.16 DMA2D

- **Base**: 0x50088000

### 7.17 ADC

- **Units**: 2 (ADC1, ADC2)
- **ADC1 Channels**: 8 (GPIO16-GPIO23)
- **ADC2 Channels**: 6 (GPIO49-GPIO54)
- **Resolution**: 12-bit
- **Sample Rate**: 611 - 83,333 Hz
- **Features**: DMA, calibration, IIR filter, monitor

### 7.18 Touch Sensor

- **Version**: 3
- **Channels**: 14 (GPIO2-GPIO15 + 1 internal)
- **Features**: Sleep wakeup, benchmark, waterproof, proximity sensing (3 channels)

### 7.19 Temperature Sensor

- **Clock**: LP_PERI
- **Features**: Interrupt, ETM, sleep retention

### 7.20 LEDC (LED PWM)

- **Timers**: 4
- **Channels**: 8
- **Resolution**: 20-bit
- **Features**: Gamma curve fade, sleep retention, ETM

### 7.21 MCPWM (Motor Control PWM)

- **Instances**: 2 (MCPWM0, MCPWM1)
- **Features**: Software sync, ETM, event comparator, sleep retention

### 7.22 RMT (Remote Control)

- **Memory per Channel**: 48 words (192 bytes)
- **Features**: RX ping-pong, TX loop count, DMA, sleep retention

### 7.23 Sigma-Delta Modulator

- **Features**: Sleep retention

### 7.24 PCNT (Pulse Counter)

- **Features**: Runtime threshold update, clear signal

### 7.25 SYSTIMER

- **Counters**: 2
- **Alarms**: 3
- **Resolution**: 52-bit (32 low + 20 high)
- **Features**: RC_FAST clock, ETM

### 7.26 GPTimer

- **Features**: ETM, sleep retention

### 7.27 TWAI (CAN Bus)

- **Controllers**: 3
- **Features**: Sleep retention

---

## 8. Boot Process

### Boot Modes

The ESP32-P4 supports multiple boot modes, determined by the GPIO strap pins at reset:

| Mode | Strap Pins | Description |
|------|------------|-------------|
| **SPI Boot** | 1XXX | Normal boot from Flash |
| **Legacy SPI Boot** | 0100 | Legacy SPI boot mode |
| **Download Boot (UART)** | 0111 | Download mode via UART0 |
| **Download Boot (SPI)** | 0110 | Download mode via SPI |
| **Download Boot (SDIO V2)** | 0000-0011 | Download mode via SDIO (various edge configurations) |
| **ATE/Analog Mode** | 0101 | Factory test mode |
| **Joint Download** | 00XX | Combined download modes |

### Boot Mode Detection Macros

```c
#define ETS_IS_UART_BOOT()       IS_0111(BOOT_MODE_GET())
#define ETS_IS_FLASH_BOOT()      (IS_1XXX(BOOT_MODE_GET()) || IS_0100(BOOT_MODE_GET()))
#define ETS_IS_FAST_FLASH_BOOT() IS_1XXX(BOOT_MODE_GET())
```

### Flash Modes

- **QIO**: Quad I/O (fastest)
- **QOUT**: Quad Output
- **DIO**: Dual I/O
- **DOUT**: Dual Output
- **Fast Read**: Supported
- **Octal Mode**: Supported (SOC_SPI_SUPPORT_OCT)

### Recovery Bootloader

- **Support**: Yes (SOC_RECOVERY_BOOTLOADER_SUPPORTED)

---

## 9. Security Features

### Cryptographic Hardware

| Engine | Base Address | Capabilities |
|--------|--------------|--------------|
| **AES** | 0x50090000 | AES-128, AES-256, GCM, DMA, pseudo-round |
| **SHA** | 0x50091000 | SHA-1, SHA-224, SHA-256, SHA-384, SHA-512, SHA-512/224, SHA-512/256, SHA-512t |
| **RSA** | 0x50092000 | Up to 4096-bit |
| **ECC** | 0x50093000 | P384 curve, constant-time point multiplication |
| **ECDSA** | 0x50096000 | P192, P256, P384, deterministic mode |
| **HMAC** | 0x50095000 | Hardware HMAC |
| **Digital Signature** | 0x50094000 | Up to 4096-bit signatures |

### Key Management

- **Key Manager**: Supports key deployment for ECDSA, Flash Encryption, HMAC, DS
- **eFuse**: Disablable JTAG (pad and USB), disablable direct boot
- **XTS-AES**: 128-bit and 256-bit keys

### Secure Boot

- **Version**: V2
- **Algorithms**: RSA, ECC
- **Key Digests**: 3 boot key digests
- **Key Revocation**: 1 revocable boot key

### Flash Encryption

- **Algorithm**: XTS-AES (128-bit and 256-bit)
- **Max Block Size**: 64 bytes
- **Features**: Page-configurable encryption, pseudo-round support (Rev >= 3.0)

### PSRAM Encryption

- **Separate Key**: Yes (independent from flash encryption)
- **Page Configurable**: Yes

---

## 10. DMA System

### DMA Controllers

| Controller | Base Address | Description |
|------------|--------------|-------------|
| **AHB DMA** | 0x50085000 | AHB bus DMA |
| **AXI DMA** | 0x5008A000 | AXI bus DMA |
| **DW DMA** | 0x50081000 | DesignWare DMA |
| **DMA2D** | 0x50088000 | 2D DMA for graphics |

### GDMA Features

- **Version**: AHB-GDMA v2
- **CRC Support**: Yes
- **ETM Support**: Yes
- **Sleep Retention**: Yes
- **Flash Encryption Alignment**: 16 bytes

### DMA Capabilities

- **Flash Access**: Yes
- **PSRAM Access**: Yes
- **Internal Memory**: 0x4FF00000 - 0x4FFC0000

---

## Appendix A: Peripheral Base Addresses

| Peripheral | Base Address |
|------------|--------------|
| USB_DWC_HS | 0x50000000 |
| USB_DWC_FS | 0x50040000 |
| DW_GDMA | 0x50081000 |
| PAU | 0x50082000 |
| SDMMC | 0x50083000 |
| AHB_DMA | 0x50085000 |
| JPEG | 0x50086000 |
| PPA | 0x50087000 |
| DMA2D | 0x50088000 |
| AXI_DMA | 0x5008A000 |
| SPIMEM0 | 0x5008C000 |
| SPIMEM1 | 0x5008D000 |
| SPIMEM2 | 0x5008E000 |
| SPIMEM3 | 0x5008F000 |
| AES | 0x50090000 |
| SHA | 0x50091000 |
| RSA | 0x50092000 |
| ECC | 0x50093000 |
| DS | 0x50094000 |
| HMAC | 0x50095000 |
| ECDSA | 0x50096000 |
| EMAC_MAC | 0x50098000 |
| EMAC_PTP | 0x50098700 |
| EMAC_DMA | 0x50099000 |
| USB_UTMI | 0x5009C000 |
| MIPI_CSI_HOST | 0x5009F000 |
| MIPI_CSI_BRIDGE | 0x5009F800 |
| MIPI_DSI_HOST | 0x500A0000 |
| MIPI_DSI_BRIDGE | 0x500A0800 |
| ISP | 0x500A1000 |
| RMT | 0x500A2000 |
| BITSCRAMBLER | 0x500A3000 |
| AXI_ICM | 0x500A4000 |
| HP_PERI_PMS | 0x500A5000 |
| LP2HP_PERI_PMS | 0x500A5800 |
| DMA_PMS | 0x500A6000 |
| AXI_PERF_MON | 0x500A8000 |
| MCPWM0 | 0x500C0000 |
| MCPWM1 | 0x500C1000 |
| TIMERG0 | 0x500C2000 |
| TIMERG1 | 0x500C3000 |
| I2C0 | 0x500C4000 |
| I2C1 | 0x500C5000 |
| I2S0 | 0x500C6000 |
| I2S1 | 0x500C7000 |
| I2S2 | 0x500C8000 |
| PCNT | 0x500C9000 |
| UART0 | 0x500CA000 |
| UART1 | 0x500CB000 |
| UART2 | 0x500CC000 |
| UART3 | 0x500CD000 |
| UART4 | 0x500CE000 |
| PARL_IO | 0x500CF000 |
| GPSPI2 | 0x500D0000 |
| GPSPI3 | 0x500D1000 |
| USB_SERIAL_JTAG | 0x500D2000 |
| LEDC | 0x500D3000 |
| SOC_ETM | 0x500D5000 |
| INTMTX | 0x500D6000 |
| TWAI0 | 0x500D7000 |
| TWAI1 | 0x500D8000 |
| TWAI2 | 0x500D9000 |
| I3C_MST | 0x500DA000 |
| I3C_SLV | 0x500DB000 |
| LCD_CAM | 0x500DC000 |
| ADC | 0x500DE000 |
| GPIO | 0x500E0000 |
| IO_MUX | 0x500E1000 |
| SYSTIMER | 0x500E2000 |
| HP_SYSTEM | 0x500E5000 |
| HP_SYS_CLKRST | 0x500E6000 |
| USB_WRAP | 0x50080000 |
| CACHE | 0x3FF10000 |
| TRACE0 | 0x3FF04000 |
| TRACE1 | 0x3FF05000 |
| ASSIST_DEBUG | 0x3FF06000 |

### LP (Low-Power) Peripherals

| Peripheral | Base Address |
|------------|--------------|
| LP_SYS | 0x50110000 |
| LP_AON_CLKRST | 0x50111000 |
| LP_TIMER | 0x50112000 |
| LP_ANA_PERI | 0x50113000 |
| PMU | 0x50115000 |
| LP_WDT | 0x50116000 |
| LP_MAILBOX | 0x50118000 |
| LPPERI | 0x50120000 |
| LP_UART | 0x50121000 |
| LP_I2C | 0x50122000 |
| LP_SPI | 0x50123000 |
| I2C_ANA_MST | 0x50124000 |
| LP_I2S | 0x50125000 |
| LP_TRNG | 0x50126000 |
| LP_ADC | 0x50127000 |
| LP_TOUCH | 0x50128000 |
| LP_GPIO | 0x5012A000 |
| LP_IOMUX | 0x5012B000 |
| EFUSE | 0x5012D000 |
| LP_PERI_PMS | 0x5012E000 |
| HP2LP_PERI_PMS | 0x5012E800 |
| LP_TSENS | 0x5012F000 |
| MIPI_CSI_MEM | 0x50104000 |
| MIPI_DSI_MEM | 0x50105000 |

---

## Appendix B: Clock Frequency Constants

| Constant | Value | Description |
|----------|-------|-------------|
| APB_CLK_FREQ | 90 MHz | APB clock frequency |
| REF_CLK_FREQ | 1 MHz | Reference clock frequency |
| SOC_CLK_RC_FAST_FREQ_APPROX | 17.5 MHz | RC_FAST approximate frequency |
| SOC_CLK_RC_SLOW_FREQ_APPROX | 136 kHz | RC_SLOW approximate frequency |
| SOC_CLK_RC32K_FREQ_APPROX | 32.768 kHz | RC32K approximate frequency |
| SOC_CLK_XTAL32K_FREQ_APPROX | 32.768 kHz | XTAL32K approximate frequency |

---

## Appendix C: Interrupt Sources

The ESP32-P4 has a comprehensive interrupt system with sources including:

- **LP Domain**: RTC, WDT, Timer, Mailbox, ADC, GPIO, I2C, I2S, SPI, Touch, UART, eFuse
- **HP Domain**: USB Serial/JTAG, SDIO, GDMA, SPI, I2S, UART (0-4), LCD/CAM, ADC, PWM, TWAI (0-2), RMT, I2C, Timer, LEDC, SYSTIMER, DMA, Crypto engines, GPIO, EMAC, MIPI, ISP, JPEG, H264, I3C

---

## References

- ESP-IDF v6.0.2 Source: `/home/geo/esp/v6.0.2/esp-idf/`
- SOC Capabilities: `components/soc/esp32p4/include/soc/soc_caps.h`
- Clock Definitions: `components/soc/esp32p4/include/soc/clk_tree_defs.h`
- Memory Map: `components/soc/esp32p4/include/soc/soc.h`
- GPIO Signal Map: `components/soc/esp32p4/include/soc/gpio_sig_map.h`
- Peripheral Addresses: `components/soc/esp32p4/ld/esp32p4.peripherals.ld`
- Boot Modes: `components/soc/esp32p4/include/soc/boot_mode.h`
