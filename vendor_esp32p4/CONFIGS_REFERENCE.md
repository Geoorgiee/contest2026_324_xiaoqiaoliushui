# ESP32-P4 EVB 配置参考手册

## 概述

本文档描述了 ESP32-P4 EVB 开发板的所有可用 defconfig 配置。

**基础信息:**
- 架构: RISC-V (RV32IMAFC)
- 芯片: ESP32-P4
- 板级: ESP32-P4 EVB
- 工具链: riscv-none-elf-gcc (xPack 14.2.0+)
- ABI: ilp32f (32位整数，硬件浮点)

---

## 配置列表

### 1. nsh - NSH 最小系统

**描述:** 最小化的 NuttX Shell 系统，适合入门学习和基础开发。

**关键特性:**
- UART0 串口控制台 (115200 baud)
- GPIO 和 Timer 驱动
- 768KB 内部 SRAM + 32MB 外部 PSRAM
- SD 卡支持 (SDMMC 接口)
- FAT 文件系统 (支持长文件名)
- ProcFS 和 ROMFS
- 自动挂载 SD 卡到 `/mnt/sdcard`

**内存配置:**
- RAM 起始: 0x4FF00000
- RAM 大小: 768KB
- 内存区域: 2 (SRAM + PSRAM)
- 默认对齐: 16 字节

**栈配置:**
- IDLE 线程栈: 2048 字节
- INIT 栈: 4096 字节
- 默认任务栈: 2048 字节

**文件路径:** `boards/risc-v/esp32p4/esp32p4-evb/configs/nsh/defconfig`

---

### 2. default - 默认配置

**描述:** 基础默认配置，禁用 PSRAM，适合资源受限场景。

**关键特性:**
- UART0 串口控制台
- GPIO 和 Timer 驱动
- 仅使用 768KB 内部 SRAM (无 PSRAM)
- SD 卡支持
- FAT 文件系统

**与 nsh 的区别:**
- PSRAM: 禁用
- 内存区域: 1 (仅 SRAM)
- IDLE 栈: 3072 字节
- 默认任务栈: 4096 字节

**文件路径:** `boards/risc-v/esp32p4/esp32p4-evb/configs/default/defconfig`

---

### 3. ble - 蓝牙配置

**描述:** 启用 NimBLE BLE 协议栈，需要外部 BLE 协处理器。

**重要提示:** ESP32-P4 没有内置蓝牙射频，需要外部 BLE 协处理器 (如 ESP32-C6 或 ESP32-H2) 通过 HCI UART 连接。

**关键特性:**
- NimBLE BLE 协议栈
- GATT 服务器 (自定义服务 + 心率服务)
- ESP-IDF 集成
- 32MB PSRAM (BLE 需要额外内存)

**BLE 配置:**
- 设备名: ESP32P4_BLE
- 广播间隔: 32-64 (20-40ms)
- MTU: 256 字节

**栈配置:**
- INIT 栈: 8192 字节 (BLE 主机任务需要更大栈)
- 默认任务栈: 4096 字节

**文件路径:** `boards/risc-v/esp32p4/esp32p4-evb/configs/ble/defconfig`

---

### 4. display - 显示配置

**描述:** 启用 MIPI-DSI 显示接口，支持 LCD 面板。

**关键特性:**
- MIPI-DSI 显示接口
- 支持 ILI9881C (800x1280, 5英寸) 或 EK79007 (1024x600, 7英寸) 面板
- 32MB PSRAM (用于帧缓冲)
- CPU 频率: 400MHz

**MIPI-DSI 配置:**
- 通道数: 2 lanes
- 速率: 1000 Mbps
- DPI 时钟: 80MHz

**LCD 面板配置 (ILI9881C):**
- 分辨率: 800x1280
- 色深: 16bpp
- 水平时序: HSYNC=40, HBP=140, HFP=40
- 垂直时序: VSYNC=4, VBP=16, VFP=16

**栈配置:**
- IDLE 栈: 4096 字节
- INIT 栈: 8192 字节
- 默认任务栈: 4096 字节

**文件路径:** `boards/risc-v/esp32p4/esp32p4-evb/configs/display/defconfig`

---

### 5. storage - 存储配置

**描述:** 详细的 SD 卡存储配置，支持 SDMMC 接口。

**关键特性:**
- SDMMC 4-bit 模式
- SD 卡自动挂载
- 设备选项支持
- CPU 频率: 400MHz

**SDMMC 配置:**
- 接口宽度: 4-bit
- 频率: 40MHz
- DMA: 禁用
- 卡检测 GPIO: 无
- 写保护 GPIO: 无

**存储功能:**
- 自动挂载: `/mnt/sdcard`
- 设备选项: 启用
- FAT 文件系统 (支持长文件名)

**内存配置:**
- 内存区域: 1 (仅 SRAM，无 PSRAM)

**文件路径:** `boards/risc-v/esp32p4/esp32p4-evb/configs/storage/defconfig`

---

### 6. tinyml - TinyML 配置

**描述:** 启用 TensorFlow Lite Micro，支持神经网络推理。

**关键特性:**
- TensorFlow Lite Micro
- 128KB 推理 Arena
- 9 种神经网络算子
- 32MB PSRAM (模型和数据需要大内存)

**支持的算子:**
- Conv2D (卷积)
- Dense/FullConnected (全连接)
- MaxPool (最大池化)
- ReLU (激活函数)
- Softmax (分类)
- Quantize (量化)
- Reshape (重塑)
- Add (加法)
- Mul (乘法)

**栈配置:**
- IDLE 栈: 4096 字节
- INIT 栈: 8192 字节
- 默认任务栈: 8192 字节

**文件路径:** `boards/risc-v/esp32p4/esp32p4-evb/configs/tinyml/defconfig`

---

## 通用配置项

所有配置共享以下基础设置:

**架构配置:**
- `CONFIG_ARCH="risc-v"`
- `CONFIG_ARCH_BOARD="esp32p4-evb"`
- `CONFIG_ARCH_CHIP="esp32p4"`
- `CONFIG_ARCH_RISCV=y`

**芯片配置:**
- Flash 大小: 16MB
- LTO (链接时优化): 启用
- UART0: 串口控制台
- GPIO: 启用
- Timer: 启用

**调试配置:**
- 断言: 启用
- 调试特性: 启用
- 优化: 完全优化
- 调试符号: 启用

**NSH 配置:**
- 架构初始化: 启用
- 内置应用: 启用
- Readline: 启用
- 入口点: `nsh_main`

---

## 编译命令

### 基本编译流程

```bash
# 1. 设置环境变量
export PATH="/home/geo/openvela-build/prebuilts/gcc/linux-x86_64/riscv-none-elf/bin:$PATH"
export PATH="/opt/openvela/prebuilts/build-tools/linux-x86_64/bin:$PATH"

# 2. 进入 NuttX 目录
cd /home/geo/openvela-build/nuttx

# 3. 配置 (以 nsh 为例)
./tools/configure.sh -e vendor/espressif/boards/esp32p4/esp32p4-evb/configs/nsh

# 4. 编译
make -j$(nproc)

# 5. 清理
make distclean
```

### 使用测试脚本

```bash
# 列出所有配置
bash build_test.sh --list

# 测试单个配置
bash build_test.sh --config nsh

# 测试所有配置
bash build_test.sh

# 只验证环境
bash build_test.sh --dry-run

# 清理后重新编译
bash build_test.sh --config nsh --clean
```

---

## 预期编译结果

### 成功标志

编译成功后，会生成以下文件:

1. **nuttx** - ELF 格式的固件文件
2. **nuttx.bin** - 二进制格式固件 (用于烧录)
3. **nuttx.hex** - Intel HEX 格式固件
4. **System.map** - 符号表文件

### 固件大小参考

| 配置 | 预期大小 | 说明 |
|------|---------|------|
| nsh | ~200-300KB | 基础 NSH 系统 |
| default | ~180-250KB | 无 PSRAM 基础配置 |
| ble | ~400-600KB | 包含 BLE 协议栈 |
| display | ~300-500KB | 包含显示驱动 |
| storage | ~200-300KB | 存储配置 |
| tinyml | ~500-800KB | 包含 TFLite Micro |

### 编译时间参考

| 配置 | 预期时间 (8核) | 说明 |
|------|---------------|------|
| nsh | 2-4 分钟 | 基础配置 |
| default | 2-3 分钟 | 最简配置 |
| ble | 5-8 分钟 | BLE 协议栈较大 |
| display | 3-5 分钟 | 显示驱动 |
| storage | 2-4 分钟 | 存储配置 |
| tinyml | 4-6 分钟 | TFLite Micro |

---

## 常见问题

### 1. 工具链未找到

**错误:** `riscv-none-elf-gcc: command not found`

**解决:**
```bash
export PATH="/home/geo/openvela-build/prebuilts/gcc/linux-x86_64/riscv-none-elf/bin:$PATH"
```

### 2. kconfig-conf 未找到

**错误:** `kconfig-conf: command not found`

**解决:**
```bash
export PATH="/opt/openvela/prebuilts/build-tools/linux-x86_64/bin:$PATH"
```

### 3. 内存不足

**错误:** 编译过程中 OOM (Out of Memory)

**解决:**
- 减少并行编译数: `make -j2` 或 `make -j4`
- 增加 swap 空间
- 使用 16GB+ 内存的机器

### 4. 配置验证失败

**错误:** `Configuration validation failed`

**解决:**
- 检查 defconfig 文件语法
- 确保所有必需配置项存在
- 运行 `make menuconfig` 手动调整

---

## 相关文件

- 配置目录: `boards/risc-v/esp32p4/esp32p4-evb/configs/`
- 链接脚本: `boards/risc-v/esp32p4/esp32p4-evb/scripts/ld.script`
- 工具链定义: `boards/risc-v/esp32p4/esp32p4-evb/scripts/Toolchain.defs`
- 构建定义: `boards/risc-v/esp32p4/esp32p4-evb/scripts/Make.defs`
- 测试脚本: `build_test.sh`

---

## 更新日志

**v1.0.0 (2026-08-29)**
- 初始版本
- 支持 6 种配置: nsh, default, ble, display, storage, tinyml
- 完整的编译测试脚本
- 详细的配置说明
