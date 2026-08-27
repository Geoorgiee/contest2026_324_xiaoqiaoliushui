# openvela 芯片特定说明

## 概述

本文档提供 openvela 支持的各种芯片架构的特定配置和注意事项。

## 乐鑫 (Espressif) 芯片

### ESP32

**架构:** Xtensa LX6 双核

**工具链:**
```bash
# 安装 ESP-IDF 工具链
sudo apt-get install -y gcc-xtensa-esp32-elf

# 或使用 ESP-IDF
source ~/esp/esp-idf/export.sh
```

**配置路径:**
```
vendor/espressif/boards/esp32/configs/default
```

**构建命令:**
```bash
./build.sh vendor/espressif/boards/esp32/configs/default --cmake -j$(nproc)
```

**注意事项:**
- ESP32 有 520KB SRAM，需要合理配置堆大小
- WiFi 和蓝牙共用射频，需要注意资源冲突
- 使用分区表管理 flash 存储

---

### ESP32-S2

**架构:** Xtensa LX7 单核

**工具链:**
```bash
sudo apt-get install -y gcc-xtensa-esp32s2-elf
```

**配置路径:**
```
vendor/espressif/boards/esp32s2/configs/default
```

**注意事项:**
- 单核设计，不支持 SMP
- USB OTG 功能需要额外配置
- 低功耗模式需要特殊处理

---

### ESP32-S3

**架构:** Xtensa LX7 双核

**工具链:**
```bash
sudo apt-get install -y gcc-xtensa-esp32s3-elf
```

**配置路径:**
```
vendor/espressif/boards/esp32s3/configs/default
```

**注意事项:**
- 支持 AI 加速指令
- 需要配置 PSRAM（如果使用）
- LCD 接口需要特殊驱动

---

### ESP32-C3

**架构:** RISC-V 单核

**工具链:**
```bash
sudo apt-get install -y gcc-riscv32-esp-elf
```

**配置路径:**
```
vendor/espressif/boards/esp32c3/configs/default
```

**注意事项:**
- RISC-V 架构，与 Xtensa 工具链不同
- 资源有限，需要精简配置
- 支持 BLE 5.0

---

### ESP32-C6

**架构:** RISC-V 单核

**工具链:**
```bash
sudo apt-get install -y gcc-riscv32-esp-elf
```

**配置路径:**
```
vendor/espressif/boards/esp32c6/configs/default
```

**注意事项:**
- 支持 WiFi 6
- 支持 Thread/Zigbee
- 需要配置 IEEE 802.15.4

---

## ARM Cortex-M 系列

### STM32F4

**架构:** ARM Cortex-M4 (带 FPU)

**工具链:**
```bash
sudo apt-get install -y gcc-arm-none-eabi
```

**配置路径:**
```
vendor/stm32/boards/stm32f4/configs/nsh
```

**构建命令:**
```bash
./build.sh vendor/stm32/boards/stm32f4/configs/nsh --cmake -j$(nproc)
```

**注意事项:**
- 支持 DSP 指令
- FPU 需要在编译时启用
- 使用 HAL 库或 LL 库
- 时钟配置需要仔细设置

---

### STM32F7

**架构:** ARM Cortex-M7 (带 FPU)

**工具链:**
```bash
sudo apt-get install -y gcc-arm-none-eabi
```

**配置路径:**
```
vendor/stm32/boards/stm32f7/configs/nsh
```

**注意事项:**
- 高性能 Cortex-M7 核心
- 支持 L1 缓存
- 需要配置 MPU（如果使用）
- DMA 传输需要特殊处理

---

### STM32H7

**架构:** ARM Cortex-M7 (带双精度 FPU)

**工具链:**
```bash
sudo apt-get install -y gcc-arm-none-eabi
```

**配置路径:**
```
vendor/stm32/boards/stm32h7/configs/nsh
```

**注意事项:**
- 高性能，支持 480MHz 主频
- 多种低功耗模式
- 需要配置电源管理
- 支持 TrustZone 安全扩展

---

### Nordic nRF52

**架构:** ARM Cortex-M4 (带 FPU)

**工具链:**
```bash
sudo apt-get install -y gcc-arm-none-eabi
```

**配置路径:**
```
vendor/nordic/boards/nrf52/configs/nsh
```

**注意事项:**
- 低功耗蓝牙 (BLE) 5.0
- 需要配置 SoftDevice（如果使用 BLE）
- 使用 Nordic SDK
- 支持 NFC

---

### Raspberry Pi RP2040

**架构:** ARM Cortex-M0+ 双核

**工具链:**
```bash
sudo apt-get install -y gcc-arm-none-eabi
```

**配置路径:**
```
vendor/raspberrypi/boards/rp2040/configs/nsh
```

**注意事项:**
- 双核 Cortex-M0+
- 使用 Pico SDK
- 需要配置 PIO（可编程 I/O）
- USB 需要特殊驱动

---

## RISC-V 系列

### QEMU RISC-V 64

**架构:** RISC-V 64 位

**工具链:**
```bash
sudo apt-get install -y gcc-riscv64-linux-gnu
# 或
sudo apt-get install -y gcc-riscv64-unknown-elf
```

**配置路径:**
```
vendor/openvela/boards/vela/configs/goldfish-riscv64
```

**构建命令:**
```bash
./build.sh vendor/openvela/boards/vela/configs/goldfish-riscv64 --cmake -j$(nproc)
```

**运行模拟器:**
```bash
./emulator.sh cmake_out/vela_goldfish-riscv64/
```

**注意事项:**
- 使用 QEMU 模拟
- 支持 RV64IMAFDC 指令集
- 需要配置 OpenSBI
- 内存布局需要特殊配置

---

## 模拟器配置

### QEMU ARM64 (推荐)

**配置路径:**
```
vendor/openvela/boards/vela/configs/goldfish-arm64-v8a-ap
```

**构建命令:**
```bash
./build.sh vendor/openvela/boards/vela/configs/goldfish-arm64-v8a-ap --cmake -j$(nproc)
```

**运行模拟器:**
```bash
./emulator.sh cmake_out/vela_goldfish-arm64-v8a-ap/
```

**注意事项:**
- 64 位 ARM 架构
- 支持 SMP（多核）
- 适合开发和测试

---

### QEMU ARM32

**配置路径:**
```
vendor/openvela/boards/vela/configs/goldfish-armeabi-v7a-ap
```

**构建命令:**
```bash
./build.sh vendor/openvela/boards/vela/configs/goldfish-armeabi-v7a-ap --cmake -j$(nproc)
```

**运行模拟器:**
```bash
./emulator.sh cmake_out/vela_goldfish-armeabi-v7a-ap/
```

**注意事项:**
- 32 位 ARM 架构
- 兼容性好
- 适合资源受限场景

---

## 工具链安装汇总

### Ubuntu/Debian

```bash
# ARM 工具链
sudo apt-get install -y gcc-arm-none-eabi

# RISC-V 工具链
sudo apt-get install -y gcc-riscv64-linux-gnu gcc-riscv64-unknown-elf

# ESP32 工具链
sudo apt-get install -y gcc-xtensa-esp32-elf gcc-xtensa-esp32s2-elf gcc-xtensa-esp32s3-elf
sudo apt-get install -y gcc-riscv32-esp-elf

# 模拟器
sudo apt-get install -y qemu-system-arm qemu-system-misc
```

### CentOS/RHEL

```bash
# ARM 工具链
sudo yum install -y arm-none-eabi-gcc-cs

# RISC-V 工具链
sudo yum install -y riscv64-unknown-elf-gcc

# 模拟器
sudo yum install -y qemu-system-arm qemu-system-misc
```

### 使用 xPack 工具链

```bash
# 安装 xPack 工具链管理器
npm install -g xpm

# 安装 ARM 工具链
xpm install --global @xpack-dev-tools/arm-none-eabi-gcc@latest

# 安装 RISC-V 工具链
xpm install --global @xpack-dev-tools/riscv-none-elf-gcc@latest
```

---

## 常见问题

### 1. 工具链找不到

**问题:** `arm-none-eabi-gcc: command not found`

**解决:**
```bash
# 检查工具链是否安装
which arm-none-eabi-gcc

# 添加到 PATH
export PATH=$PATH:/usr/local/bin

# 或安装工具链
sudo apt-get install -y gcc-arm-none-eabi
```

### 2. 编译错误

**问题:** 编译时出现架构相关的错误

**解决:**
- 确保使用正确的工具链
- 检查 Makefile 中的架构设置
- 验证 defconfig 配置

### 3. 模拟器启动失败

**问题:** QEMU 无法启动

**解决:**
```bash
# 检查 QEMU 版本
qemu-system-arm --version

# 检查内核镜像
file cmake_out/vela_*/nuttx

# 尝试无图形模式
./emulator.sh -no-window cmake_out/vela_*/
```

---

## 性能优化建议

### 1. 编译优化

```bash
# 使用 ccache
sudo apt-get install -y ccache
export CC="ccache gcc"

# 使用并行编译
./build.sh <config> --cmake -j$(nproc)

# 使用增量编译
./build.sh <config> --cmake  # 不清理直接编译
```

### 2. 链接优化

```bash
# 使用 LTO（链接时优化）
CONFIG_LTO=y

# 使用 GC（垃圾回收）
CONFIG_DEBUG_OPT_UNUSED_SECTIONS=y
```

### 3. 运行时优化

```bash
# 启用缓存
CONFIG_ARM_DCACHE=y
CONFIG_ARM_ICACHE=y

# 启用 MPU
CONFIG_ARM_MPU=y
```

---

## 调试工具

### 1. GDB 调试

```bash
# 启动 QEMU 调试模式
./emulator.sh -s -S cmake_out/vela_*/

# 连接 GDB
gdb cmake_out/vela_*/nuttx
(gdb) target remote :1234
(gdb) continue
```

### 2. JTAG 调试

```bash
# 使用 OpenOCD
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg

# 连接 GDB
gdb nuttx
(gdb) target remote :3333
```

### 3. 日志调试

```bash
# 启用串口日志
CONFIG_SERIAL_CONSOLE=y
CONFIG_UART1_SERIAL_CONSOLE=y

# 启用系统日志
CONFIG_SYSLOG=y
CONFIG_SYSLOG_SERIAL=y
```
