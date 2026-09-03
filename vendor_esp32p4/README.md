# ESP32-P4 EVB openvela 适配

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)]()
[![License](https://img.shields.io/badge/license-Apache%202.0-blue)]()
[![NuttX](https://img.shields.io/badge/NuttX-12.4-orange)]()

## 概述

本项目将乐鑫 **ESP32-P4 Function EV Board** 适配到 [openvela](https://openvela.com/) (NuttX) 实时操作系统，实现从底层 BSP 移植、驱动开发到系统构建的全链路适配工作。

### 硬件特性

| 特性 | 规格 |
|------|------|
| **CPU** | 双核 RISC-V (HP Core 400MHz + LP Core 40MHz) |
| **内存** | 768 KB SRAM + 32 MB PSRAM |
| **存储** | 16 MB Flash |
| **显示** | MIPI-DSI 接口 |
| **摄像头** | MIPI-CSI 接口 |
| **无线** | BLE (需外部协处理器) |
| **接口** | USB-OTG 2.0 HS, SPI, I2C, UART, ADC, DAC, GPIO |

### 适配完成情况

| 模块 | 状态 | 说明 |
|------|------|------|
| UART 串口 | ✅ | 串口控制台 |
| GPIO | ✅ | LED/按键控制 |
| Timer | ✅ | 系统定时器 |
| PSRAM | ✅ | 32MB 外部内存 |
| SDMMC | ⚠️ | 驱动待更新 |
| MIPI-DSI | ✅ | LCD 显示 |
| BLE | ✅ | 蓝牙低功耗 |
| I2C | ✅ | I2C 总线 |
| SPI | ✅ | SPI 总线 |

---

## 快速开始

### 1. 环境搭建

#### 前置条件

- Linux 系统 (推荐 Ubuntu 20.04+)
- 16GB+ RAM
- USB 数据线 x2

#### 安装工具链

```bash
# 安装 RISC-V 工具链
wget https://github.com/xpack-dev-tools/riscv-none-elf-gcc-xpack/releases/download/v14.2.0-3/xpack-riscv-none-elf-gcc-14.2.0-3-linux-x64.tar.gz
tar -xzf xpack-riscv-none-elf-gcc-14.2.0-3-linux-x64.tar.gz
export PATH=$PWD/xpack-riscv-none-elf-gcc-14.2.0-3/bin:$PATH

# 验证安装
riscv-none-elf-gcc --version
```

#### 安装烧录工具

```bash
# 安装 esptool
pip install esptool>=4.7

# 验证安装
esptool.py version
```

详细步骤请参考 [环境搭建指南](docs/environment-setup-guide.md)。

---

### 2. 编译固件

```bash
# 进入 NuttX 目录
cd /home/geo/openvela/nuttx

# 配置 (以 NSH 为例)
./tools/configure.sh -e ../vendor_esp32p4/boards/risc-v/esp32p4/esp32p4-evb/configs/nsh

# 编译
make -j$(nproc)

# 验证编译产物
ls -la nuttx.bin
```

**其他配置：**

```bash
# 默认配置 (无 PSRAM)
./tools/configure.sh -e ../vendor_esp32p4/boards/risc-v/esp32p4/esp32p4-evb/configs/default

# BLE 蓝牙配置
./tools/configure.sh -e ../vendor_esp32p4/boards/risc-v/esp32p4/esp32p4-evb/configs/ble

# 显示配置
./tools/configure.sh -e ../vendor_esp32p4/boards/risc-v/esp32p4/esp32p4-evb/configs/display

# 存储配置
./tools/configure.sh -e ../vendor_esp32p4/boards/risc-v/esp32p4/esp32p4-evb/configs/storage

# TinyML 配置
./tools/configure.sh -e ../vendor_esp32p4/boards/risc-v/esp32p4/esp32p4-evb/configs/tinyml
```

---

### 3. 烧录固件

#### 连接硬件

1. 连接 **USB-JTAG** 接口 (标记为 USB) - 用于调试
2. 连接 **USB-UART** 接口 (标记为 UART) - 用于串口监控

#### 进入下载模式

1. 按住 **BOOT** 按钮 (GPIO 21)
2. 短按 **RST** 按钮 (复位)
3. 松开 **BOOT** 按钮

#### 执行烧录

```bash
# 烧录固件
esptool.py --chip esp32p4 --port /dev/ttyACM0 --baud 460800 \
    write_flash 0x0 nuttx.bin

# 或指定 Flash 大小
esptool.py --chip esp32p4 --port /dev/ttyACM0 --baud 460800 \
    --flash_size 16MB \
    write_flash 0x0 nuttx.bin
```

详细步骤请参考 [烧录测试指南](docs/flash-and-test-guide.md)。

---

### 4. 串口监控

```bash
# 使用 minicom
minicom -D /dev/ttyACM0 -b 115200

# 或使用 screen
screen /dev/ttyACM0 115200

# 或使用 picocom
picocom -b 115200 /dev/ttyACM0

# 或使用 ESP-IDF monitor
idf.py -p /dev/ttyACM0 monitor
```

**预期输出：**

```
NuttShell (NSH) NuttX-12.4
nsh>
```

---

## 目录结构

```
vendor_esp32p4/
├── boards/                          # 板级代码
│   └── risc-v/esp32p4/esp32p4-evb/
│       ├── configs/                 # 配置文件
│       │   ├── nsh/                 # NSH 最小系统
│       │   ├── default/             # 默认配置
│       │   ├── ble/                 # BLE 蓝牙
│       │   ├── display/             # 显示
│       │   ├── storage/             # 存储
│       │   └── tinyml/              # TinyML
│       ├── scripts/                 # 链接脚本
│       ├── src/                     # 板级源码
│       └── include/                 # 头文件
├── chips/                           # 芯片层代码
│   └── esp32p4/
│       ├── esp32p4_serial.c         # UART 驱动
│       ├── esp32p4_gpio.c           # GPIO 驱动
│       ├── esp32p4_psram.c          # PSRAM 驱动
│       ├── esp32p4_sdmmc.c          # SDMMC 驱动
│       ├── esp32p4_mipi_dsi.c       # MIPI-DSI 驱动
│       ├── esp32p4_ble.c            # BLE 驱动
│       ├── esp32p4_i2c.c            # I2C 驱动
│       ├── esp32p4_spi.c            # SPI 驱动
│       ├── include/                 # 头文件
│       └── hardware/                # 硬件寄存器定义
├── docs/                            # 文档
│   ├── environment-setup-guide.md   # 环境搭建指南
│   ├── flash-and-test-guide.md      # 烧录测试指南
│   ├── esp32p4_ble_integration.md   # BLE 集成指南
│   ├── HARDWARE.md                  # 硬件连接文档
│   └── ISSUES_TRACKING.md           # 问题溯源文档
├── .gitignore                       # Git 忽略文件
├── CONFIGS_REFERENCE.md             # 配置参考手册
├── CHANGELOG.md                     # 修改记录
└── README.md                        # 本文件
```

---

## 配置说明

本项目提供 6 种预设配置，适用于不同场景：

| 配置 | 描述 | 适用场景 |
|------|------|----------|
| **nsh** | NSH 最小系统 | 入门学习、基础开发 |
| **default** | 默认配置 (无 PSRAM) | 资源受限场景 |
| **ble** | BLE 蓝牙 | 蓝牙应用开发 |
| **display** | MIPI-DSI 显示 | 显示应用开发 |
| **storage** | SD 卡存储 | 存储应用开发 |
| **tinyml** | TinyML 推理 | 边缘 AI 应用 |

详细配置说明请参考 [配置参考手册](CONFIGS_REFERENCE.md)。

---

## 开发指南

### 添加自定义应用

1. 在 `apps/examples/` 目录创建应用目录
2. 编写 `Kconfig`、`Makefile` 和源代码
3. 在 `menuconfig` 中启用应用
4. 重新编译固件

### 添加新驱动

1. 在 `chips/esp32p4/` 目录创建驱动文件
2. 在 `Kconfig` 中添加配置选项
3. 在 `Make.defs` 中添加编译规则
4. 在 `esp32p4_bringup.c` 中初始化驱动

详细开发指南请参考：
- [openvela 移植指南](https://doc.openvela.com/document?id=215&version=trunk&language=cn)
- [NuttX 驱动开发](https://nuttx.apache.org/docs/latest/reference/os/drivers.html)

---

## 故障排查

### 常见问题

#### 1. 编译失败

```bash
# 清理并重新编译
make distclean
./tools/configure.sh -e ../vendor_esp32p4/boards/risc-v/esp32p4/esp32p4-evb/configs/nsh
make -j$(nproc)
```

#### 2. 烧录失败

```bash
# 检查串口权限
ls -la /dev/ttyACM0
sudo chmod 666 /dev/ttyACM0

# 手动进入下载模式
# 按住 BOOT -> 按 RST -> 松开 BOOT
```

#### 3. 串口无输出

```bash
# 检查波特率 (应为 115200)
minicom -D /dev/ttyACM0 -b 115200

# 复位芯片
# 按 RST 按钮
```

更多问题请参考 [问题溯源文档](docs/ISSUES_TRACKING.md)。

---

## 相关文档

| 文档 | 描述 |
|------|------|
| [环境搭建指南](docs/environment-setup-guide.md) | 开发环境配置 |
| [烧录测试指南](docs/flash-and-test-guide.md) | 烧录和测试方法 |
| [配置参考手册](CONFIGS_REFERENCE.md) | 所有配置选项说明 |
| [BLE 集成指南](docs/esp32p4_ble_integration.md) | 蓝牙功能开发 |
| [硬件连接文档](docs/HARDWARE.md) | 硬件引脚和连接 |
| [问题溯源文档](docs/ISSUES_TRACKING.md) | 问题记录和解决方案 |
| [修改记录](CHANGELOG.md) | 版本更新历史 |

---

## 参考资源

- [openvela 官方文档](https://doc.openvela.com/)
- [NuttX 官方文档](https://nuttx.apache.org/docs/)
- [ESP32-P4 技术参考手册](https://www.espressif.com/zh-hans/products/socs/esp32-p4)
- [ESP32-P4 开发板文档](https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/esp32p4/esp32-p4x-function-ev-board/user_guide.html)
- [ESP-IDF 编程指南](https://docs.espressif.com/projects/esp-idf/)

---

## 许可证

本项目基于 [Apache License 2.0](http://www.apache.org/licenses/LICENSE-2.0) 开源。

---

## 贡献

欢迎贡献代码！请遵循以下步骤：

1. Fork 本仓库
2. 创建特性分支 (`git checkout -b feature/your-feature`)
3. 提交修改 (`git commit -m 'feat: add your feature'`)
4. 推送到分支 (`git push origin feature/your-feature`)
5. 创建 Pull Request

---

## 联系方式

- **维护者：** geo@openvela
- **问题反馈：** [GitHub Issues](https://github.com/Geoorgiee/contest2026_324_xiaoqiaoliushui/issues)

---

*最后更新：2026-09-03*
