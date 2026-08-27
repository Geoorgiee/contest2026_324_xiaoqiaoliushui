# ESP32-P4 Openvela Development Plan

**Created**: 2026-08-26
**Target Hardware**: Espressif ESP32-P4 EVB (Dual-core RISC-V Hazard3, 400MHz)
**Platform**: openvela (NuttX-based RTOS)

---

## Table of Contents

1. [Executive Summary](#1-executive-summary)
2. [Current Status](#2-current-status)
3. [Priority Tasks for Hardware Adaptation](#3-priority-tasks-for-hardware-adaptation)
4. [Driver Development Roadmap](#4-driver-development-roadmap)
5. [Build System Integration Steps](#5-build-system-integration-steps)
6. [Testing and Validation Plan](#6-testing-and-validation-plan)
7. [Timeline Estimation](#7-timeline-estimation)
8. [Risk Register](#8-risk-register)
9. [Appendix: Key References](#9-appendix-key-references)

---

## 1. Executive Summary

This plan covers the complete bring-up and peripheral adaptation of the ESP32-P4 EVB board under openvela. The ESP32-P4 is Espressif's first pure RISC-V SoC featuring dual Hazard3 cores at 400MHz, MIPI-DSI/CSI interfaces, hardware video codecs, USB OTG HS, and an ISP. It has no integrated WiFi/BLE and relies on an ESP32-C6 companion chip connected via SDIO.

Milestones M1 (Environment Setup) and M2 (Single-core Bring-up code) are complete. The remaining work spans from hardware validation of M2 through full multimedia platform capability. This plan organizes that work into four phases over approximately 16 weeks.

---

## 2. Current Status

| Milestone | Status | Notes |
|-----------|--------|-------|
| M1: Environment Setup | DONE | Completed 2026-08-09 |
| M2: Single-core Bring-up | Code DONE | Pending hardware flash test. UART, GPIO, Timer, basic memory, NSH shell implemented. 16 chip-layer files (3,078 lines) and 12 board-layer files (1,008 lines) completed. |

### Key Implemented Components

- `esp32p4_start.c` -- Boot and early init
- `esp32p4_irq.c` -- PLIC interrupt controller
- `esp32p4_clockconfig.c` -- CPU 400MHz, APB 80MHz
- `esp32p4_serial.c` -- UART0 console (TX=GPIO37, RX=GPIO38)
- `esp32p4_gpio.c` -- 54 GPIO pins
- `esp32p4_timerisr.c` -- HP Timer Group 0
- `esp32p4_allocateheap.c` -- Dual-region heap (PSRAM primary + HP SRAM secondary)

### Verified Boot Path

```
__start -> esp32p4_start() -> clock/cache/UART init -> clear BSS -> copy .data
-> nx_start() -> board_early_initialize() -> board_late_initialize()
-> esp32p4_bringup() -> NSH shell
```

---

## 3. Priority Tasks for Hardware Adaptation

### Phase 1: Core Platform Stabilization (Weeks 1-4)

**P1-1: Hardware Validation of M2 (Week 1)**

Flash the existing M2 build to the ESP32-P4 EVB and validate:
- UART0 console output at 115200 baud
- NSH shell prompt and basic command execution
- GPIO output/input on test pins
- Timer interrupt accuracy (tick resolution)
- BSS clearing and .data copy correctness

Success criteria: Stable NSH shell for 10 minutes with no crashes or hangs.

**P1-2: PSRAM Driver (Weeks 1-2)**

PSRAM is mandatory for any meaningful workload on ESP32-P4. The 768KB internal SRAM is insufficient for framebuffer, network buffers, or application code.

Tasks:
- Implement PSRAM initialization sequence referencing ESP-IDF `esp_psram` component
- Configure MMU page table entries for 0x48000000 region (64KB pages, 1024 entries)
- Enable MPLL clock (400MHz) for PSRAM interface
- Add PSRAM self-test (write/readback pattern test across full 32MB range)
- Register PSRAM as additional heap region in `esp32p4_allocateheap.c` (set `CONFIG_MM_REGIONS=3`)
- Validate DMA access to PSRAM from GDMA controller

Success criteria: `free` command in NSH shows ~32MB available RAM. Self-test passes with zero errors.

**P1-3: SMP Dual-Core Bring-up (Weeks 2-3)**

This is the highest-risk task and must be approached methodically.

Tasks:
- Implement `esp32p4_cpu_start()` for HP Core 1 startup
- Configure CLIC (Core Local Interrupt Controller) per-core with 32 external interrupts
- Implement IPI (Inter-Processor Interrupt) for core synchronization
- Adapt NuttX SMP scheduler integration (`CONFIG_SMP=y`, `CONFIG_SMP_NCPUS=2`)
- Implement spinlock-based critical sections for shared peripheral access
- Validate concurrent task scheduling across both cores
- Run stress test: parallel `nsh` sessions on each core, shared heap allocation

Success criteria: Both cores running NuttX tasks concurrently. No deadlocks under 1-hour stress test. `nsh> uname -a` reports 2 CPUs.

**P1-4: SPI Flash and MTD Layer (Weeks 3-4)**

Tasks:
- Implement SPI Flash driver for internal flash (16MB, mapped at 0x42000000)
- Configure flash read modes (QIO preferred, fallback DIO)
- Implement MTD (Memory Technology Device) block driver interface
- Register as `/dev/flash` or similar block device
- Add flash erase/write protection handling
- Implement partition table parsing (compatible with ESP-IDF partition format at offset 0x8000)

Success criteria: Can read flash ID, erase sectors, and write/readback data via MTD interface.

**P1-5: Filesystem -- LittleFS (Week 4)**

Tasks:
- Integrate LittleFS library into build system
- Create LittleFS partition on SPI Flash
- Mount at `/data` or `/lfs` mountpoint
- Validate file create/read/write/delete operations
- Test power-loss resilience (pull power during write, verify filesystem integrity)

Success criteria: Persistent filesystem survives reboot. File operations work correctly from NSH shell.

### Phase 2: Bus and Peripheral Drivers (Weeks 5-8)

**P2-1: SPI Bus Driver (Week 5)**

Tasks:
- Implement SPI2 and SPI3 master mode drivers
- Configure GPIO matrix for flexible pin routing
- Implement `spi_dev_s` interface (open, close, setfrequency, setmode, exchange)
- Support DMA-accelerated transfers via GDMA
- Test with SPI loopback and external SPI device (e.g., SPI flash chip)

Default pin assignments (configurable via GPIO matrix):
- SPI2: CLK=GPIO12, MOSI=GPIO11, MISO=GPIO13, CS=GPIO10

**P2-2: I2C Bus Driver (Week 5-6)**

Tasks:
- Implement I2C controller drivers (2 HP + 1 LP)
- Implement `i2c_transfer` interface with standard/restart conditions
- Support 100kHz and 400kHz modes
- Test with I2C sensor (e.g., BME280, MPU6050)

Default pin assignments:
- I2C0: SDA=GPIO7, SCL=GPIO8

**P2-3: GDMA Driver (Week 6)**

DMA is critical for high-throughput peripherals (SPI, I2S, USB, display, camera).

Tasks:
- Implement AHB-GDMA and AXI-GDMA controller drivers
- Create DMA channel allocation API
- Support memory-to-peripheral and peripheral-to-memory transfers
- Handle DMA descriptor chain configuration
- Validate DMA access to both HP SRAM and PSRAM regions

**P2-4: ADC Driver (Week 7)**

Tasks:
- Implement ADC1 (8 channels, GPIO16-23) and ADC2 (6 channels, GPIO49-54)
- Support 12-bit resolution
- Implement calibration using ESP32-P4 eFuse calibration data
- Expose via `/dev/adcN` character device interface
- Support one-shot and continuous (DMA) modes

**P2-5: USB OTG HS Device Controller Driver (Weeks 7-8)**

Tasks:
- Implement USB OTG HS DWC v4.30a device controller driver (DCD)
- Initialize PHY before Core (per hardware requirement)
- Handle SET_ADDRESS in driver (not USB stack)
- Allocate DMA buffers with 4096-byte page alignment
- Start with USB Serial/JTAG CDC-ACM class for debug console
- Progress to USB MSC (Mass Storage Class) for exposing flash filesystem to host
- Implement FS mode fallback if HS enumeration fails

Success criteria: Device enumerates on host USB port. Can open serial terminal via USB CDC-ACM.

**P2-6: Watchdog Timer (Week 8)**

Tasks:
- Implement MWDT (Main Watchdog Timer) driver
- Support configurable timeout and reset behavior
- Integrate with NuttX watchdog framework (`/dev/watchdogN`)
- Add watchdog feed in idle task for production builds

### Phase 3: Display, Camera, and Multimedia (Weeks 9-13)

**P3-1: MIPI-DSI Display Driver (Weeks 9-10)**

Tasks:
- Implement MIPI-DSI host controller driver
- Configure D-PHY for video mode
- Implement framebuffer interface (`fb_vtable_s`)
- Register as `/dev/fb0`
- Support RGB565 and RGB888 pixel formats
- Implement double-buffering with VSYNC interrupt
- Integrate with PSRAM framebuffer allocation (typical 1024x600x4 = 2.4MB)
- Test with EVB-connected DSI display panel

Success criteria: Display shows solid color fill, gradient, and bitmap image from NSH.

**P3-2: JPEG Hardware Codec (Week 11)**

Tasks:
- Implement hardware JPEG encoder and decoder driver
- Expose via `/dev/jpeg` or integrate with NuttX video subsystem
- Support baseline JPEG (DCT-based)
- Validate encode: raw RGB -> JPEG file
- Validate decode: JPEG file -> raw RGB -> display on framebuffer

**P3-3: H.264 Hardware Encoder (Week 12)**

Tasks:
- Implement H.264 encoder driver (up to 1080p)
- Interface with ISP (Image Signal Processor) for camera input
- Support configurable bitrate, resolution, and framerate
- Expose via V4L2-like or custom video device interface

**P3-4: MIPI-CSI Camera Driver (Weeks 12-13)**

Tasks:
- Implement MIPI-CSI host controller driver
- Configure D-PHY for camera capture
- Integrate with ISP pipeline (demosaic, AWB, AE, denoise)
- Implement V4L2-like capture interface
- Support YUV422 and RGB output formats
- Pipeline: Camera -> ISP -> H.264 Encoder (for recording)
- Pipeline: Camera -> ISP -> Display (for viewfinder)

**P3-5: I2S Audio Driver (Week 13)**

Tasks:
- Implement I2S controller drivers (3 HP + 1 LP I2S)
- Support master TX and RX modes
- Implement NuttX audio subsystem interface (`/dev/audio/pcmC0D0p`, `/dev/audio/pcmC0D0c`)
- Integrate with external audio codec via I2C control
- Test playback and recording with EVB audio hardware

Pin assignments:
- I2S: MCK=GPIO13, BCK=GPIO12, WS=GPIO10, DO=GPIO9, DI=GPIO11
- PA Control: GPIO53

### Phase 4: Connectivity and Production (Weeks 14-16)

**P4-1: SDIO/MMC Driver (Week 14)**

Tasks:
- Implement SDMMC host controller driver (2 slots, 8-bit width, UHS-I)
- Support SD card and eMMC devices
- Register as block device (`/dev/mmcsd0`)
- Integrate with NuttX FAT/exFAT filesystem
- Test with SD card insert/remove and filesystem mount

**P4-2: Ethernet MAC Driver (Week 14-15)**

Tasks:
- Implement EMAC driver with IEEE 1588v2 PTP support
- Integrate with NuttX network stack (`netdev_lowerhalf_s` interface)
- Support 10/100 Mbps MII/RMII interface
- Configure MDIO for PHY management
- Test: ping, TCP/IP, DHCP client

**P4-3: WiFi via ESP-Hosted (Week 15-16)**

Tasks:
- Implement SDIO host driver for ESP32-C6 communication
- Port ESP-Hosted firmware protocol to NuttX (replace FreeRTOS primitives)
- Map FreeRTOS calls: `xTaskCreate` -> `nxtask_create`, `xSemaphoreTake` -> `nxsem_wait`, etc.
- Integrate with NuttX `netdev` network device interface
- Configure ESP32-C6 WiFi firmware (station + softAP modes)
- Test: scan, connect, ping, TCP/IP throughput

This is the most complex connectivity task. The ESP32-C6 companion chip communicates over SDIO, requiring both SDIO host and a protocol translation layer.

**P4-4: LP Core via OpenAMP/RPMsg (Week 16)**

Tasks:
- Implement LP Core firmware loading mechanism
- Set up OpenAMP framework for HP-LP communication
- Implement RPMsg channel for command/data exchange
- Use LP Core for always-on sensor monitoring during deep sleep

---

## 4. Driver Development Roadmap

### Driver Architecture Pattern

All drivers follow the NuttX upper-half/lower-half pattern:

```
Application (NSH / user code)
    |
    v
VFS Layer (/dev/xxx)
    |
    v
Upper-Half Driver (NuttX generic, e.g., spi_dev_s)
    |
    v
Lower-Half Driver (ESP32-P4 specific, e.g., esp32p4_spi.c)
    |
    v
Hardware Registers (memory-mapped I/O at 0x50000000-0x50130000)
```

### Driver Registration Summary

| Driver | Device Path | Interface | Priority |
|--------|-------------|-----------|----------|
| UART console | `/dev/console` | `uart_register()` | P0 (done) |
| GPIO | `/dev/gpioN` | `gpio_irq_attach()` | P1 (done) |
| Timer | system tick | timer ISR | P1 (done) |
| SPI Flash MTD | `/dev/flash` | `register_blockdriver()` | P1 |
| LittleFS | `/data` | `mount()` | P1 |
| SPI bus | `/dev/spiN` | `spi_dev_s` | P2 |
| I2C bus | `/dev/i2cN` | `i2c_transfer` | P2 |
| ADC | `/dev/adcN` | `adc_ops_s` | P2 |
| Watchdog | `/dev/watchdogN` | `watchdog_ops_s` | P2 |
| USB CDC-ACM | `/dev/ttyACM0` | USB DCD | P2 |
| Framebuffer | `/dev/fb0` | `fb_vtable_s` | P3 |
| JPEG codec | `/dev/jpeg` | custom | P3 |
| H.264 encoder | `/dev/video0` | custom/V4L2-like | P3 |
| Camera | `/dev/video1` | V4L2-like capture | P3 |
| Audio PCM | `/dev/audio/pcm*` | NuttX audio | P3 |
| SD/MMC block | `/dev/mmcsd0` | `register_blockdriver()` | P4 |
| Ethernet | `eth0` | `netdev_lowerhalf_s` | P4 |
| WiFi (ESP-Hosted) | `wlan0` | `netdev_lowerhalf_s` | P4 |

### Key Implementation Notes

**Memory-Mapped Peripherals**: All HP peripheral registers are in the 0x50000000-0x50100000 range. LP peripherals are at 0x50110000-0x50130000. Use `putreg32()`/`getreg32()` for register access.

**Interrupt Handling**: ESP32-P4 uses CLIC (not PLIC as previously named in M2 code -- this needs correction). 32 external interrupts per core. Configure via `irq_attach()` and `up_enable_irq()`.

**DMA Buffers**: All DMA buffers must be in DMA-capable memory (HP SRAM at 0x4FF00000 or PSRAM at 0x48000000). USB buffers require 4096-byte page alignment.

**GPIO Matrix**: Most peripheral signals can be routed to any GPIO pin via the GPIO matrix. This provides flexibility but requires careful pin assignment to avoid conflicts.

**Clock Dependencies**: Before using any peripheral, its clock gate must be enabled via the clock system. SPLL (480MHz) feeds most peripheral clocks through dividers.

---

## 5. Build System Integration Steps

### 5.1 Directory Structure

```
nuttx/
  arch/risc-v/src/
    esp32p4/                    # Chip-layer (architecture-level stubs)
    vendor_esp32p4/
      chips/esp32p4/            # SoC-specific implementation (16 files, 3078 lines)
        esp32p4_start.c
        esp32p4_irq.c
        esp32p4_clockconfig.c
        esp32p4_serial.c
        esp32p4_gpio.c
        esp32p4_timerisr.c
        esp32p4_allocateheap.c
        esp32p4_spi.c           # (P2-1)
        esp32p4_i2c.c           # (P2-2)
        esp32p4_gdma.c          # (P2-3)
        esp32p4_adc.c           # (P2-4)
        esp32p4_usb.c           # (P2-5)
        ...
  boards/risc-v/esp32p4/
    esp32p4-evb/
      configs/default/defconfig # Board configuration
      src/
        esp32p4_bringup.c       # Board init (12 files, 1008 lines)
        esp32p4_boot.c
        ...
```

### 5.2 defconfig Key Settings

The base defconfig for the EVB board must include:

```ini
# Architecture
CONFIG_ARCH="risc-v"
CONFIG_ARCH_CHIP="esp32p4"
CONFIG_ARCH_BOARD="esp32p4-evb"

# Vendor code location (outside nuttx tree)
CONFIG_ARCH_CHIP_VENDOR_DIR="../vendor_esp32p4/chips/esp32p4"
CONFIG_ARCH_BOARD_DIR="../vendor_esp32p4/boards/risc-v/esp32p4/esp32p4-evb"

# Memory
CONFIG_RAM_START=0x4FF00000
CONFIG_RAM_SIZE=786432          # 768KB HP SRAM
CONFIG_MM_REGIONS=2             # Increase to 3 when PSRAM driver is ready

# Console
CONFIG_UART0_SERIAL_CONSOLE=y
CONFIG_UART0_RXBUFSIZE=256
CONFIG_UART0_TXBUFSIZE=256

# NSH
CONFIG_SYSTEM_NSH=y
CONFIG_INIT_ENTRYPOINT="nsh_main"

# SMP (enable after P1-3)
# CONFIG_SMP=y
# CONFIG_SMP_NCPUS=2

# Power Management (enable in Phase 4)
# CONFIG_PM_ENABLE=y
```

### 5.3 Build Commands

```bash
# Configure
./tools/configure.sh esp32p4-evb:default

# Build
make -j$(nproc)

# Or with CMake-based build
./build.sh esp32p4-evb --cmake -j$(nproc)
```

### 5.4 Flash Commands

```bash
# Enter download mode: hold BOOT button, press RST, release BOOT

# Flash via serial (bootloader at offset 0x0 for ESP32-P4)
esptool.py --chip esp32p4 --port /dev/ttyACM0 --baud 460800 \
  write_flash 0x0 nuttx.bin

# Flash via USB DFU (alternative, when USB is configured)
idf.py dfu-flash
```

Note: ESP32-P4 bootloader offset is 0x0 (not 0x1000 like ESP32). Maximum bootloader size is 64KB (0x10000).

### 5.5 Adding a New Driver to the Build

For each new chip-layer driver file, add to the Makefile or CMakeLists.txt:

```makefile
# In arch/risc-v/src/esp32p4/Make.defs
CSRCS += esp32p4_spi.c
```

For board-layer integration, modify `esp32p4_bringup.c` to call the driver registration:

```c
#ifdef CONFIG_ESP32P4_SPI
  ret = esp32p4_spibus_initialize(2);  /* Initialize SPI2 */
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: Failed to initialize SPI2: %d\n", ret);
    }
#endif
```

### 5.6 Kconfig Integration

Each driver needs a Kconfig entry:

```kconfig
config ESP32P4_SPI
    bool "ESP32-P4 SPI driver"
    default n
    depends on ARCH_CHIP_ESP32P4
    ---help---
        Enable SPI bus driver for ESP32-P4.

config ESP32P4_SPI2
    bool "SPI2 controller"
    default y
    depends on ESP32P4_SPI
```

This mirrors the ESP-IDF Kconfig approach, which is conceptually identical to NuttX's system.

---

## 6. Testing and Validation Plan

### 6.1 Unit Tests (per driver)

| Driver | Test Case | Method |
|--------|-----------|--------|
| PSRAM | Write/read pattern across full 32MB | Custom memory test in NSH |
| SMP | Parallel task creation on both cores | Stress test script |
| SPI Flash | Erase sector, write, readback, compare | `dd` equivalent in NSH |
| LittleFS | Create, write, read, delete files; power cycle | File operations + reboot |
| SPI | Loopback test (MOSI->MISO) | SPI transfer utility |
| I2C | Read device ID from known sensor | I2C scan + register read |
| GDMA | Memory-to-memory copy with verification | DMA test utility |
| ADC | Read channel values, compare with expected voltage | Multimeter correlation |
| USB | Host enumeration, serial echo test | minicom/picocom on host |
| Framebuffer | Color fill, gradient, bitmap display | Visual inspection |
| JPEG | Encode/decode round-trip with PSNR check | Image comparison |
| SD/MMC | Mount card, file I/O, remove and reinsert | Filesystem stress test |
| Ethernet | Ping, iperf TCP/UDP throughput | Network test tools |
| WiFi | Scan, connect, ping, throughput | Network test tools |

### 6.2 Integration Tests

| Test | Description | Pass Criteria |
|------|-------------|---------------|
| Boot reliability | Cold boot 100 times consecutively | 100% successful NSH prompt |
| Memory stress | Allocate/free in loop across all regions | Zero memory leaks over 1 hour |
| Multi-peripheral | SPI + I2C + UART active simultaneously | No bus contention or data corruption |
| Display + Camera | Live camera preview on DSI display | Stable 30fps with no frame drops |
| Filesystem endurance | Repeated write/verify cycles | Zero data corruption over 10,000 cycles |
| Network stability | Continuous TCP stream for 24 hours | Zero disconnections |
| Power cycle resilience | Pull power during write operation | Filesystem recovers on reboot |

### 6.3 Performance Benchmarks

| Metric | Target | Tool |
|--------|--------|------|
| CPU CoreMark | >2000 (single core) | CoreMark benchmark |
| Memory bandwidth | >500 MB/s (PSRAM) | Custom memcpy benchmark |
| SPI throughput | >20 MHz effective (SPI2) | SPI loopback throughput test |
| USB HS throughput | >300 Mbps | USB bulk transfer benchmark |
| Display refresh | 60 fps at 1024x600 | Framebuffer swap timing |
| JPEG encode | >30 fps at 720p | Hardware codec benchmark |
| Ethernet throughput | >90 Mbps (100M link) | iperf3 |
| WiFi throughput | >10 Mbps (depends on AP) | iperf3 |
| Boot time | <2 seconds to NSH | Timer measurement |

### 6.4 Stress Testing Protocol

Run the following combinations for minimum 24 hours each before declaring production-ready:

1. **Idle stress**: NSH prompt, no user input, watchdog enabled
2. **Memory stress**: Continuous allocation/deallocation across all heap regions
3. **I/O stress**: Simultaneous SPI, I2C, UART, GPIO toggling
4. **Network stress**: Continuous TCP/UDP traffic with packet loss monitoring
5. **Filesystem stress**: Continuous file create/write/delete cycle
6. **Multimedia stress**: Camera capture -> display pipeline with recording

---

## 7. Timeline Estimation

### Phase 1: Core Platform Stabilization (Weeks 1-4)

| Week | Tasks | Deliverables |
|------|-------|--------------|
| 1 | M2 hardware validation; PSRAM driver start | Validated NSH on hardware; PSRAM init code |
| 2 | PSRAM driver completion; SMP bring-up start | PSRAM self-test passing; Core 1 booting |
| 3 | SMP completion and stress testing | Dual-core NSH stable |
| 4 | SPI Flash MTD; LittleFS | Persistent filesystem working |

**Phase 1 Exit Criteria**: Stable dual-core NSH with 32MB RAM and persistent filesystem.

### Phase 2: Bus and Peripheral Drivers (Weeks 5-8)

| Week | Tasks | Deliverables |
|------|-------|--------------|
| 5 | SPI bus driver; I2C bus driver | `/dev/spiN`, `/dev/i2cN` working |
| 6 | GDMA driver; I2C validation with sensor | DMA transfers verified |
| 7 | ADC driver; USB OTG DCD start | `/dev/adcN` working; USB init code |
| 8 | USB DCD completion; Watchdog | USB CDC-ACM enumerating on host |

**Phase 2 Exit Criteria**: All basic peripheral buses operational. USB device mode functional.

### Phase 3: Display, Camera, and Multimedia (Weeks 9-13)

| Week | Tasks | Deliverables |
|------|-------|--------------|
| 9 | MIPI-DSI display driver start | DSI PHY initialization |
| 10 | MIPI-DSI completion; Framebuffer | `/dev/fb0` showing image |
| 11 | JPEG hardware codec | Encode/decode working |
| 12 | H.264 encoder; MIPI-CSI camera start | Video encoding pipeline |
| 13 | Camera driver completion; I2S audio | Full camera->display pipeline; audio playback |

**Phase 3 Exit Criteria**: Multimedia platform -- display, camera, video encoding, audio all functional.

### Phase 4: Connectivity and Production Hardening (Weeks 14-16)

| Week | Tasks | Deliverables |
|------|-------|--------------|
| 14 | SDIO/MMC driver; Ethernet MAC start | SD card mountable; EMAC init |
| 15 | Ethernet completion; WiFi/ESP-Hosted start | TCP/IP over Ethernet; SDIO to ESP32-C6 |
| 16 | WiFi completion; LP Core; Stability testing | WiFi connected; 24-hour stress tests |

**Phase 4 Exit Criteria**: Full connectivity (Ethernet + WiFi). All 24-hour stress tests passing.

### Milestone Summary

| Milestone | Target Date | Key Result |
|-----------|-------------|------------|
| M2 Validated | Week 1 | NSH on hardware |
| M3: Core Platform | Week 4 | Dual-core, PSRAM, filesystem |
| M4: Peripherals | Week 8 | All buses + USB |
| M5: Multimedia | Week 13 | Display, camera, codecs, audio |
| M6: Production Ready | Week 16 | Full connectivity, stress-tested |

---

## 8. Risk Register

| Risk | Severity | Likelihood | Mitigation |
|------|----------|------------|------------|
| SMP dual-core instability | HIGH | MEDIUM | Complete single-core bring-up first. Use spinlock audit. Run deadlock detection. |
| PSRAM initialization failure | MEDIUM | MEDIUM | Reference ESP-IDF `esp_psram` driver. Add comprehensive self-test. |
| USB OTG HS enumeration failure | MEDIUM | MEDIUM | Implement FS mode fallback first. Validate PHY init sequence. |
| Code size exceeds SRAM | MEDIUM | LOW | Enable LTO (`CONFIG_LTO=y`) and section GC (`-ffunction-sections -fdata-sections`). Move code to flash XIP. |
| MIPI-DSI display not working | MEDIUM | MEDIUM | Start with known-good EVB display panel. Reference ESP-IDF MIPI-DSI example. |
| WiFi ESP-Hosted port complexity | HIGH | HIGH | This is the most complex integration task. Budget extra time. Start with minimal SDIO host. |
| Clock configuration conflicts | LOW | LOW | Document all clock dependencies. Use clock tree validation. |
| DMA buffer alignment issues | MEDIUM | LOW | Use dedicated DMA buffer pool with explicit alignment. |

---

## 9. Appendix: Key References

### Source Code Paths

| Component | Path |
|-----------|------|
| Chip layer (arch stubs) | `nuttx/arch/risc-v/src/esp32p4/` |
| SoC implementation | `vendor_esp32p4/chips/esp32p4/` |
| Board layer | `vendor_esp32p4/boards/risc-v/esp32p4/esp32p4-evb/` |
| defconfig | `vendor_esp32p4/boards/risc-v/esp32p4/esp32p4-evb/configs/default/defconfig` |

### Documentation Paths

| Document | Path |
|----------|------|
| Hardware Reference | `/home/geo/openvela/docs/esp32p4-hardware-reference.md` |
| Porting Guide | `/home/geo/openvela/docs/porting-guide.md` |
| Driver Development Guide | `/home/geo/openvela/docs/driver-development.md` |
| Examples Guide | `/home/geo/openvela/docs/esp32p4-examples-guide.md` |
| ESP32-P4 Adaptation Notes | `/home/geo/openvela/docs/esp32p4-adaptation.md` |
| NSH Baseline defconfig | `/home/geo/openvela/docs/nsh-baseline-defconfig.md` |
| Hardware Porting Track | `/home/geo/openvela/docs/hardware-porting-track.md` |

### External References

| Resource | Purpose |
|----------|---------|
| ESP-IDF `esp_psram` component | PSRAM initialization reference |
| ESP-IDF `esp_lcd_panel_io_dbi` + `esp_lcd_panel_ops` | MIPI-DSI driver reference |
| ESP-IDF `esp_video` + ISP pipeline | Camera driver reference |
| ESP-IDF `esp_hosted` | WiFi companion chip integration reference |
| ESP32-P4 Technical Reference Manual | Register-level documentation |
| NuttX SPI/I2C/GPIO/Framework docs | Upper-half driver API reference |

### Pin Assignment Quick Reference (ESP32-P4 EVB)

| Function | GPIO | Notes |
|----------|------|-------|
| UART0 TX | 37 | Default console |
| UART0 RX | 38 | Default console |
| I2C0 SDA | 7 | |
| I2C0 SCL | 8 | |
| I2S MCK | 13 | |
| I2S BCK | 12 | |
| I2S WS | 10 | |
| I2S DO | 9 | |
| I2S DI | 11 | |
| PA Control | 53 | Audio power amplifier |
| LP UART TX | 24 | LP domain (deep sleep capable) |
| LP UART RX | 25 | LP domain |
| GPIO0-15 | LP domain | Deep sleep wakeup capable |
| GPIO16-23 | ADC1 | 8 channels |
| GPIO49-54 | ADC2 | 6 channels |
