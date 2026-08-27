# ESP32-P4 openvela 适配指南

## 文档信息

| 项目 | 内容 |
|------|------|
| 目标芯片 | ESP32-P4 (Espressif) |
| 目标平台 | openvela (基于 NuttX) |
| 架构 | RISC-V 32 位 (RV32IMAFCXcheri) |
| 版本 | v1.0.0 |
| 日期 | 2026-08-09 |

---

## 目录

1. [硬件概述](#一硬件概述)
2. [环境准备](#二环境准备)
3. [适配步骤详解](#三适配步骤详解)
4. [代码示例](#四代码示例)
5. [测试验证](#五测试验证)
6. [常见问题](#六常见问题)
7. [附录](#七附录)

---

## 一、硬件概述

### 1.1 ESP32-P4 架构特点

ESP32-P4 是乐鑫首款采用纯 RISC-V 架构的高性能 SoC，与之前的 Xtensa 架构 ESP32/S2/S3 有本质区别。

**CPU 核心**

| 核心 | 架构 | 主频 | 指令集 | 用途 |
|------|------|------|--------|------|
| HP Core 0 | RISC-V Hazard3 | 最高 400MHz | RV32IMAFCXcheri | 主应用核心 |
| HP Core 1 | RISC-V Hazard3 | 最高 400MHz | RV32IMAFCXcheri | SMP 辅助核心 |
| LP Core | RISC-V | 最高 40MHz | RV32IMAC | 低功耗 always-on |

HP 核心支持浮点运算 (FPU)、原子指令 (A)、压缩指令 (C)、CHERI 硬件安全扩展。LP 核心运行独立固件，用于超低功耗场景。

**内存布局**

| 区域 | 地址 | 大小 | 说明 |
|------|------|------|------|
| Internal SRAM | 0x4FF00000 | 768 KB | HP 核心主内存 |
| LP SRAM | 0x50108000 | 16 KB | LP 核心内存 |
| RTC FAST Memory | 0x50100000 | 8 KB | RTC 域 |
| External Flash (Cache) | 0x40000000 | 64 MB 窗口 | SPI Flash 缓存映射 |
| External PSRAM (Cache) | 0x44000000 | 最大 128 MB | OPI PSRAM |
| L1 Cache | 每核 32 KB | 指令/数据缓存 |

**外设接口**

| 外设 | 关键特性 |
|------|----------|
| USB OTG 2.0 HS | Host/Device，480Mbps |
| MIPI-DSI | 高分辨率显示输出 |
| MIPI-CSI | 摄像头输入 |
| I2S (多实例) | 多通道音频 |
| SPI (多实例) | Flash、PSRAM、通用外设 |
| I2C (多实例) | 传感器、外设通信 |
| UART (多实例) | 调试、通信 |
| ADC/DAC | 模拟信号采集/输出 |
| GDMA | 通用 DMA 传输 |
| JPEG Codec | 硬件图像编解码 |
| SDIO/MMC | SD 卡、eMMC |
| Ethernet MAC | 有线以太网 |
| MCPWM | 电机/PWM 控制 |

**连接性**

ESP32-P4 本身不集成 WiFi/蓝牙，需通过外部伴侣芯片 (如 ESP32-C6) 提供无线连接能力。EVB 板上通常集成此伴侣芯片。

### 1.2 EVB 板资源

ESP32-P4 EVB 评估板通常包含:

- ESP32-P4 主芯片
- 16MB SPI Flash (OPI/QPI)
- 32MB OPI PSRAM
- USB Type-C 接口 (USB OTG HS)
- USB-to-UART 桥接芯片 (调试串口)
- ESP32-C6 伴侣芯片 (WiFi/BLE)
- MIPI-DSI 显示接口
- SD 卡槽
- 以太网 PHY (可选)

### 1.3 与已支持芯片的对比

| 特性 | ESP32-C3 | ESP32-C6 | ESP32-P4 |
|------|----------|----------|----------|
| 架构 | RISC-V 单核 | RISC-V 单核 | RISC-V 双核 HP + LP |
| 主频 | 160MHz | 160MHz | 400MHz |
| SRAM | 400KB | 512KB | 768KB + 16KB LP |
| PSRAM | 无 | 无 | 最大 128MB |
| USB | USB 2.0 FS | USB 2.0 FS | USB 2.0 HS |
| WiFi/BLE | 内置 | 内置 | 外部伴侣芯片 |
| 显示 | 无 | 无 | MIPI-DSI |
| 工具链 | gcc-riscv32-esp-elf | gcc-riscv32-esp-elf | gcc-riscv32-esp-elf |

---

## 二、环境准备

### 2.1 开发机要求

| 项目 | 最低要求 | 推荐配置 |
|------|----------|----------|
| 操作系统 | Ubuntu 20.04+ | Ubuntu 22.04 LTS |
| 内存 | 8 GB | 16 GB |
| 磁盘空间 | 30 GB | 50 GB |
| CPU | 4 核 | 8 核+ |

### 2.2 工具链安装

ESP32-P4 使用乐鑫提供的 RISC-V 工具链，与 ESP32-C3/C6 共用:

```bash
# 方式一: 通过 apt 安装 (推荐)
sudo apt-get update
sudo apt-get install -y gcc-riscv32-esp-elf

# 方式二: 通过 ESP-IDF 安装
cd ~
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32p4
source export.sh

# 验证安装
riscv32-esp-elf-gcc --version
```

### 2.3 基础依赖安装

```bash
# 构建工具
sudo apt-get install -y build-essential cmake ninja-build ccache

# NuttX 依赖
sudo apt-get install -y kconfig-frontends python3-pip
pip3 install pyelftools

# 调试工具
sudo apt-get install -y gdb-multiarch openocd minicom

# Flash 烧录工具
pip3 install esptool
```

### 2.4 仓库获取

```bash
# 克隆 openvela 仓库 (使用 NFS 共享方案可加速)
cd ~
git clone --reference /nfs/openvela-bare.git <openvela-repo-url> openvela
cd openvela

# 如果使用共享仓库方案
bash shared-repo-solution/dev-machine-setup.sh
```

### 2.5 硬件连接

```
ESP32-P4 EVB                    开发机
┌──────────────┐               ┌──────────────┐
│   USB-UART   │───────────────│ /dev/ttyUSB0 │  调试串口 (NSH)
│   (UART0)    │               │              │
├──────────────┤               ├──────────────┤
│   USB OTG HS │───────────────│ /dev/ttyACM0 │  USB Device
│   (Type-C)   │               │              │
├──────────────┤               ├──────────────┤
│   JTAG       │───────────────│ OpenOCD      │  调试接口
│   (可选)     │               │              │
└──────────────┘               └──────────────┘
```

串口终端配置:
```bash
# 使用 minicom 连接调试串口
minicom -D /dev/ttyUSB0 -b 115200

# 或使用 screen
screen /dev/ttyUSB0 115200
```

---

## 三、适配步骤详解

### 3.1 适配阶段总览

| 阶段 | 内容 | 优先级 | 预估工时 |
|------|------|--------|----------|
| Phase 1 | 单核 Bring-up (UART + GPIO + Timer + 基础内存) | P0 | 2-3 周 |
| Phase 2 | SMP 支持 + PSRAM + Flash 文件系统 | P1 | 2-3 周 |
| Phase 3 | 核心外设 (SPI, I2C, DMA, USB) | P1 | 3-4 周 |
| Phase 4 | 高级功能 (MIPI-DSI, I2S, Ethernet, LP 核心) | P2 | 4-6 周 |
| Phase 5 | 稳定性优化 + 功耗管理 + 测试 | -- | 2-3 周 |

### 3.2 Phase 1: 单核 Bring-up

#### 3.2.1 创建目录结构

```bash
# 芯片架构层目录
mkdir -p nuttx/arch/risc-v/src/esp32p4/hardware

# 板级目录
mkdir -p nuttx/boards/risc-v/esp32p4/esp32p4-evb/configs/default
mkdir -p nuttx/boards/risc-v/esp32p4/esp32p4-evb/configs/smp
mkdir -p nuttx/boards/risc-v/esp32p4/esp32p4-evb/configs/nsh
mkdir -p nuttx/boards/risc-v/esp32p4/esp32p4-evb/src
```

完整目录布局:

```
nuttx/
├── arch/risc-v/src/esp32p4/
│   ├── Kconfig                          # 芯片级配置项
│   ├── Make.defs                        # 构建规则
│   ├── CMakeLists.txt                   # CMake 构建规则
│   ├── esp32p4_start.c                  # 启动入口
│   ├── esp32p4_irq.c                    # 中断控制器驱动
│   ├── esp32p4_clockconfig.c            # 时钟树配置
│   ├── esp32p4_allocateheap.c           # 堆内存分配
│   ├── esp32p4_serial.c                 # UART 驱动
│   ├── esp32p4_lowputs.c                # 低级串口输出
│   ├── esp32p4_gpio.c                   # GPIO 驱动
│   ├── esp32p4_timerisr.c               # 系统 tick 定时器
│   ├── esp32p4_wdt.c                    # 看门狗驱动
│   ├── esp32p4_spi.c                    # SPI 总线驱动
│   ├── esp32p4_i2c.c                    # I2C 总线驱动
│   ├── esp32p4_dma.c                    # GDMA 驱动
│   ├── esp32p4_flash.c                  # SPI Flash 驱动
│   ├── esp32p4_psram.c                  # PSRAM 初始化
│   ├── esp32p4_smp.c                    # SMP 双核管理
│   ├── esp32p4_usbdev.c                 # USB OTG HS DCD 驱动
│   ├── esp32p4_adc.c                    # ADC 驱动
│   ├── esp32p4_dac.c                    # DAC 驱动
│   ├── esp32p4_mipidsi.c                # MIPI-DSI 驱动
│   ├── esp32p4_i2s.c                    # I2S 音频驱动
│   ├── esp32p4_emac.c                   # 以太网 MAC 驱动
│   ├── esp32p4_lpcore.c                 # LP 核心通信
│   └── hardware/
│       ├── esp32p4_soc.h                # SoC 寄存器基地址总表
│       ├── esp32p4_uart.h               # UART 寄存器定义
│       ├── esp32p4_gpio.h               # GPIO 寄存器定义
│       ├── esp32p4_timer.h              # Timer 寄存器定义
│       ├── esp32p4_spi.h                # SPI 寄存器定义
│       ├── esp32p4_i2c.h                # I2C 寄存器定义
│       ├── esp32p4_dma.h                # GDMA 寄存器定义
│       ├── esp32p4_usb.h                # USB 寄存器定义
│       ├── esp32p4_flash.h              # Flash 控制器寄存器定义
│       ├── esp32p4_psram.h              # PSRAM 控制器寄存器定义
│       ├── esp32p4_plic.h               # PLIC/CLIC 寄存器定义
│       └── esp32p4_clock.h              # 时钟/PLL 寄存器定义
│
└── boards/risc-v/esp32p4/esp32p4-evb/
    ├── Kconfig                          # 板级配置项
    ├── Make.defs                        # 板级构建规则
    ├── CMakeLists.txt
    ├── configs/
    │   ├── default/
    │   │   └── defconfig                # 默认配置 (单核)
    │   ├── smp/
    │   │   └── defconfig                # SMP 双核配置
    │   ├── nsh/
    │   │   └── defconfig                # NSH 最小系统
    │   └── full/
    │       └── defconfig                # 全功能配置
    └── src/
        ├── esp32p4_bringup.c            # 板级初始化
        ├── esp32p4_boot.c               # 启动辅助
        ├── esp32p4_clockconfig.c        # 板级时钟配置
        ├── esp32p4_spidev.c             # SPI 设备注册
        ├── esp32p4_i2cbus.c             # I2C 设备注册
        ├── esp32p4_usbdev.c             # USB 设备注册
        └── Make.defs
```

#### 3.2.2 实现 SoC 寄存器定义

首先创建 `hardware/esp32p4_soc.h`，定义所有外设寄存器基地址:

```c
/****************************************************************************
 * arch/risc-v/src/esp32p4/hardware/esp32p4_soc.h
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#ifndef __ARCH_RISC_V_SRC_ESP32P4_HARDWARE_ESP32P4_SOC_H
#define __ARCH_RISC_V_SRC_ESP32P4_HARDWARE_ESP32P4_SOC_H

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* 外设寄存器基地址 (从 ESP32-P4 Technical Reference Manual 提取) */

#define ESP32P4_PERIP_BASE        0x50000000

#define ESP32P4_UART0_BASE        (ESP32P4_PERIP_BASE + 0x0000)
#define ESP32P4_UART1_BASE        (ESP32P4_PERIP_BASE + 0x1000)
#define ESP32P4_UART2_BASE        (ESP32P4_PERIP_BASE + 0x2000)

#define ESP32P4_SPI0_BASE         (ESP32P4_PERIP_BASE + 0x3000)
#define ESP32P4_SPI1_BASE         (ESP32P4_PERIP_BASE + 0x4000)
#define ESP32P4_SPI2_BASE         (ESP32P4_PERIP_BASE + 0x5000)

#define ESP32P4_I2C0_BASE         (ESP32P4_PERIP_BASE + 0x6000)
#define ESP32P4_I2C1_BASE         (ESP32P4_PERIP_BASE + 0x7000)

#define ESP32P4_GPIO_BASE         (ESP32P4_PERIP_BASE + 0x8000)

#define ESP32P4_TIMER0_BASE       (ESP32P4_PERIP_BASE + 0x9000)
#define ESP32P4_TIMER1_BASE       (ESP32P4_PERIP_BASE + 0x9100)

#define ESP32P4_WDT0_BASE         (ESP32P4_PERIP_BASE + 0xA000)

#define ESP32P4_USB_BASE          (ESP32P4_PERIP_BASE + 0xB000)

#define ESP32P4_GDMA_BASE         (ESP32P4_PERIP_BASE + 0xC000)

#define ESP32P4_MIPI_DSI_BASE     (ESP32P4_PERIP_BASE + 0xD000)

#define ESP32P4_EMAC_BASE         (ESP32P4_PERIP_BASE + 0xE000)

#define ESP32P4_I2S0_BASE         (ESP32P4_PERIP_BASE + 0xF000)

/* 中断号 (从 ESP32-P4 Technical Reference Manual 提取) */

#define ESP32P4_IRQ_UART0         10
#define ESP32P4_IRQ_UART1         11
#define ESP32P4_IRQ_UART2         12
#define ESP32P4_IRQ_SPI0          15
#define ESP32P4_IRQ_SPI1          16
#define ESP32P4_IRQ_SPI2          17
#define ESP32P4_IRQ_I2C0          20
#define ESP32P4_IRQ_I2C1          21
#define ESP32P4_IRQ_GPIO          25
#define ESP32P4_IRQ_TIMER0        35
#define ESP32P4_IRQ_WDT           40
#define ESP32P4_IRQ_USB           50
#define ESP32P4_IRQ_DMA_CH0       55
#define ESP32P4_IRQ_ETH           60
#define ESP32P4_IRQ_MIPI_DSI      65
#define ESP32P4_IRQ_I2S0          70

#define ESP32P4_NIRQS             96

/* 内存区域 */

#define ESP32P4_SRAM_BASE         0x4ff00000
#define ESP32P4_SRAM_SIZE         0x000c0000  /* 768 KB */
#define ESP32P4_PSRAM_BASE        0x44000000
#define ESP32P4_LP_SRAM_BASE      0x50108000
#define ESP32P4_LP_SRAM_SIZE      0x00004000  /* 16 KB */

/* PLIC 基地址 */

#define ESP32P4_PLIC_BASE         0x10000000
#define ESP32P4_PLIC_PRIORITY     (ESP32P4_PLIC_BASE + 0x0000)
#define ESP32P4_PLIC_PENDING      (ESP32P4_PLIC_BASE + 0x1000)
#define ESP32P4_PLIC_ENABLE       (ESP32P4_PLIC_BASE + 0x2000)
#define ESP32P4_PLIC_THRESHOLD    (ESP32P4_PLIC_BASE + 0x200000)

/* 寄存器访问宏 */

#define ESP32P4_REG(addr)         (*(volatile uint32_t *)(addr))

/****************************************************************************
 * Inline Functions
 ****************************************************************************/

static inline uint32_t esp32p4_getreg(uintptr_t addr)
{
  return *(volatile uint32_t *)addr;
}

static inline void esp32p4_putreg(uint32_t val, uintptr_t addr)
{
  *(volatile uint32_t *)addr = val;
}

static inline void esp32p4_modifyreg(uintptr_t addr, uint32_t clearbits,
                                     uint32_t setbits)
{
  irqstate_t flags;
  uint32_t regval;

  flags = enter_critical_section();
  regval = esp32p4_getreg(addr);
  regval &= ~clearbits;
  regval |= setbits;
  esp32p4_putreg(regval, addr);
  leave_critical_section(flags);
}

#endif /* __ARCH_RISC_V_SRC_ESP32P4_HARDWARE_ESP32P4_SOC_H */
```

#### 3.2.3 实现启动入口

创建 `esp32p4_start.c`:

```c
/****************************************************************************
 * arch/risc-v/src/esp32p4/esp32p4_start.c
 *
 * ESP32-P4 HP Core 0 启动入口
 *
 * 启动序列:
 * 1. 配置系统时钟 (PLL, CPU 频率)
 * 2. 初始化 L1 Cache
 * 3. 初始化低级串口 (早期调试)
 * 4. 清零 BSS 段
 * 5. 复制初始化数据段
 * 6. 跳转到 nx_start() (NuttX 内核入口)
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/compiler.h>
#include <stdint.h>
#include <string.h>
#include <nuttx/arch.h>
#include <nuttx/board.h>

#include "riscv_internal.h"
#include "hardware/esp32p4_soc.h"
#include "hardware/esp32p4_clock.h"
#include "esp32p4_lowputs.h"

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void esp32p4_clock_init(void)
{
  /* TODO: 配置 PLL 到目标 HP 核心频率
   * 必须在任何外设访问之前完成
   * 参考 CONFIG_ESP32P4_HP_CORE_FREQ
   */
}

static void esp32p4_cache_init(void)
{
  /* TODO: 启用 L1 指令缓存和数据缓存
   * 每核 32KB (I-Cache + D-Cache)
   * 配置 cache line 大小 (通常 32 字节)
   * 推荐 write-back 策略以获得最佳性能
   */
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

void esp32p4_start(void)
{
  /* 第一步: 配置系统时钟 */

  esp32p4_clock_init();

  /* 第二步: 启用 L1 Cache */

  esp32p4_cache_init();

  /* 第三步: 初始化低级串口用于早期调试输出 */

  esp32p4_lowsetup();

  /* 第四步: 清零 BSS 段 */

  memset(&_sbss, 0, (&_ebss - &_sbss));

  /* 第五步: 从 Flash 复制初始化数据到 SRAM */

  if (_eronly != _sdata)
    {
      memcpy(_sdata, _eronly, _edata - _sdata);
    }

  /* 第六步: 初始化 NuttX 内核 (此函数不会返回) */

  nx_start();

  /* 不应到达此处 */

  for (; ; );
}
```

#### 3.2.4 实现中断控制器驱动

创建 `esp32p4_irq.c`。ESP32-P4 支持 PLIC (Platform-Level Interrupt Controller) 或 CLIC (Core-Local Interrupt Controller):

```c
/****************************************************************************
 * arch/risc-v/src/esp32p4/esp32p4_irq.c
 *
 * ESP32-P4 中断控制器驱动
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/irq.h>
#include <nuttx/arch.h>
#include <nuttx/spinlock.h>

#include "riscv_internal.h"
#include "hardware/esp32p4_soc.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define PLIC_PRIORITY(irq)   (ESP32P4_PLIC_PRIORITY + ((irq) * 4))
#define PLIC_ENABLE(irq)     (ESP32P4_PLIC_ENABLE + (((irq) / 32) * 4))
#define PLIC_THRESHOLD       ESP32P4_PLIC_THRESHOLD
#define PLIC_CLAIM           (ESP32P4_PLIC_THRESHOLD + 4)

/****************************************************************************
 * Public Functions
 ****************************************************************************/

void up_irqinitialize(void)
{
  int i;

  /* 禁用所有中断 */

  for (i = 0; i < ESP32P4_NIRQS / 32; i++)
    {
      esp32p4_putreg(0, ESP32P4_PLIC_ENABLE + i * 4);
    }

  /* 设置所有中断源的默认优先级 */

  for (i = 1; i < ESP32P4_NIRQS; i++)
    {
      esp32p4_putreg(1, PLIC_PRIORITY(i));
    }

  /* 设置阈值为 0 (允许所有优先级) */

  esp32p4_putreg(0, PLIC_THRESHOLD);

  /* 挂载公共 RISC-V 中断处理入口 */

  riscv_exception_attach();
}

void up_disable_irq(int irq)
{
  irqstate_t flags;
  uint32_t regval;

  if (irq >= 0 && irq < ESP32P4_NIRQS)
    {
      flags = spin_lock_irqsave(NULL);
      regval = esp32p4_getreg(PLIC_ENABLE(irq));
      regval &= ~(1 << (irq % 32));
      esp32p4_putreg(regval, PLIC_ENABLE(irq));
      spin_unlock_irqrestore(NULL, flags);
    }
}

void up_enable_irq(int irq)
{
  irqstate_t flags;
  uint32_t regval;

  if (irq >= 0 && irq < ESP32P4_NIRQS)
    {
      flags = spin_lock_irqsave(NULL);
      regval = esp32p4_getreg(PLIC_ENABLE(irq));
      regval |= (1 << (irq % 32));
      esp32p4_putreg(regval, PLIC_ENABLE(irq));
      spin_unlock_irqrestore(NULL, flags);
    }
}

int up_ack_irq(int irq)
{
  /* 返回中断号，NuttX 框架使用此值 */

  return irq;
}
```

#### 3.2.5 实现 UART 驱动

创建 `esp32p4_serial.c`，遵循 NuttX serial upper-half/lower-half 模式。参考 NuttX 已有的 ESP32-C3 UART 驱动结构:

```c
/****************************************************************************
 * arch/risc-v/src/esp32p4/esp32p4_serial.c
 *
 * ESP32-P4 UART lower-half 驱动
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/irq.h>
#include <nuttx/serial/serial.h>
#include <nuttx/clock.h>

#include "hardware/esp32p4_soc.h"
#include "hardware/esp32p4_uart.h"

/****************************************************************************
 * UART 寄存器偏移 (需根据 datasheet 确认)
 ****************************************************************************/

#define UART_FIFO_REG       0x00
#define UART_INT_RAW_REG    0x04
#define UART_INT_ST_REG     0x08
#define UART_INT_ENA_REG    0x0C
#define UART_INT_CLR_REG    0x10
#define UART_CLKDIV_REG     0x14
#define UART_RXFIFO_CNT_REG 0x18
#define UART_TXFIFO_CNT_REG 0x1C
#define UART_CONF0_REG      0x20
#define UART_CONF1_REG      0x24

/****************************************************************************
 * Private Data
 ****************************************************************************/

#ifdef CONFIG_ESP32P4_UART0
static char g_uart0rxbuffer[CONFIG_UART0_RXBUFSIZE];
static char g_uart0txbuffer[CONFIG_UART0_TXBUFSIZE];
#endif

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int  esp32p4_uart_setup(struct uart_dev_s *dev);
static void esp32p4_uart_shutdown(struct uart_dev_s *dev);
static int  esp32p4_uart_attach(struct uart_dev_s *dev);
static void esp32p4_uart_detach(struct uart_dev_s *dev);
static int  esp32p4_uart_ioctl(struct uart_dev_s *dev, int cmd,
                                unsigned long arg);
static void esp32p4_uart_rxint(struct uart_dev_s *dev, bool enable);
static bool esp32p4_uart_rxavailable(struct uart_dev_s *dev);
static void esp32p4_uart_txint(struct uart_dev_s *dev, bool enable);
static bool esp32p4_uart_txready(struct uart_dev_s *dev);
static bool esp32p4_uart_txempty(struct uart_dev_s *dev);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct uart_ops_s g_uart_ops =
{
  .setup       = esp32p4_uart_setup,
  .shutdown    = esp32p4_uart_shutdown,
  .attach      = esp32p4_uart_attach,
  .detach      = esp32p4_uart_detach,
  .ioctl       = esp32p4_uart_ioctl,
  .receive     = esp32p4_uart_receive,
  .rxint       = esp32p4_uart_rxint,
  .rxavailable = esp32p4_uart_rxavailable,
  .send        = esp32p4_uart_send,
  .txint       = esp32p4_uart_txint,
  .txready     = esp32p4_uart_txready,
  .txempty     = esp32p4_uart_txempty,
};

#ifdef CONFIG_ESP32P4_UART0
static struct uart_dev_s g_uart0priv =
{
  .recv =
    {
      .size   = CONFIG_UART0_RXBUFSIZE,
      .buffer = g_uart0rxbuffer,
    },
  .xmit =
    {
      .size   = CONFIG_UART0_TXBUFSIZE,
      .buffer = g_uart0txbuffer,
    },
  .ops  = &g_uart_ops,
  .priv = (void *)ESP32P4_UART0_BASE,
};
#endif

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int esp32p4_uart_setup(struct uart_dev_s *dev)
{
  uintptr_t base = (uintptr_t)dev->priv;

  /* 配置波特率 */
  /* TODO: 根据 APB 时钟频率计算分频值 */

  /* 配置数据格式: 8N1 */
  /* TODO: 写入 CONF0 寄存器 */

  /* 使能 FIFO */
  /* TODO: 写入 CONF1 寄存器 */

  return OK;
}

static void esp32p4_uart_shutdown(struct uart_dev_s *dev)
{
  /* TODO: 禁用 UART 外设 */
}

static int esp32p4_uart_attach(struct uart_dev_s *dev)
{
  int irq;

  /* 根据 UART 实例确定中断号 */

  if ((uintptr_t)dev->priv == ESP32P4_UART0_BASE)
    {
      irq = ESP32P4_IRQ_UART0;
    }
  else
    {
      return -EINVAL;
    }

  return irq_attach(irq, esp32p4_uart_interrupt, dev);
}

static void esp32p4_uart_detach(struct uart_dev_s *dev)
{
  /* TODO: 禁用 UART 中断 */
}

static void esp32p4_uart_rxint(struct uart_dev_s *dev, bool enable)
{
  uintptr_t base = (uintptr_t)dev->priv;
  uint32_t regval;

  regval = esp32p4_getreg(base + UART_INT_ENA_REG);
  if (enable)
    {
      regval |= UART_INT_RXFIFO;
    }
  else
    {
      regval &= ~UART_INT_RXFIFO;
    }

  esp32p4_putreg(regval, base + UART_INT_ENA_REG);
}

static bool esp32p4_uart_rxavailable(struct uart_dev_s *dev)
{
  uintptr_t base = (uintptr_t)dev->priv;
  uint32_t cnt;

  cnt = esp32p4_getreg(base + UART_RXFIFO_CNT_REG);
  return cnt > 0;
}

static void esp32p4_uart_txint(struct uart_dev_s *dev, bool enable)
{
  uintptr_t base = (uintptr_t)dev->priv;
  uint32_t regval;

  regval = esp32p4_getreg(base + UART_INT_ENA_REG);
  if (enable)
    {
      regval |= UART_INT_TXFIFO;
    }
  else
    {
      regval &= ~UART_INT_TXFIFO;
    }

  esp32p4_putreg(regval, base + UART_INT_ENA_REG);
}

static bool esp32p4_uart_txready(struct uart_dev_s *dev)
{
  uintptr_t base = (uintptr_t)dev->priv;
  uint32_t cnt;

  cnt = esp32p4_getreg(base + UART_TXFIFO_CNT_REG);
  return cnt < 128;  /* FIFO 深度 */
}

static bool esp32p4_uart_txempty(struct uart_dev_s *dev)
{
  uintptr_t base = (uintptr_t)dev->priv;
  uint32_t cnt;

  cnt = esp32p4_getreg(base + UART_TXFIFO_CNT_REG);
  return cnt == 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int esp32p4_uart_register(int id)
{
  struct uart_dev_s *dev;
  char devpath[16];

  switch (id)
    {
#ifdef CONFIG_ESP32P4_UART0
      case 0:
        dev = &g_uart0priv;
        break;
#endif
      default:
        return -ENODEV;
    }

  snprintf(devpath, sizeof(devpath), "/dev/ttyS%d", id);
  return uart_register(devpath, dev);
}
```

#### 3.2.6 实现堆内存分配

创建 `esp32p4_allocateheap.c`:

```c
/****************************************************************************
 * arch/risc-v/src/esp32p4/esp32p4_allocateheap.c
 *
 * ESP32-P4 堆内存分配
 *
 * Phase 1: 仅使用 SRAM (768KB)
 * Phase 2: 添加 PSRAM 作为第二内存区域
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/mm/mm.h>

#include "riscv_internal.h"
#include "hardware/esp32p4_soc.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

void up_allocate_heap(void **heap_start, size_t *heap_size)
{
  /* SRAM 堆: 从 BSS 段结束到 SRAM 末尾 */

  *heap_start = (void *)&_eheap;
  *heap_size  = (ESP32P4_SRAM_BASE + ESP32P4_SRAM_SIZE) -
                (uintptr_t)&_eheap;
}

#ifdef CONFIG_ESP32P4_PSRAM
void up_addregion(void)
{
  /* PSRAM 作为第二内存区域 (用于用户态堆、大 buffer、文件系统缓存) */

  mm_addregion(g_kmmheap,
               (void *)ESP32P4_PSRAM_BASE,
               (size_t)CONFIG_ESP32P4_PSRAM_SIZE * 1024 * 1024);
}
#endif
```

#### 3.2.7 创建 Kconfig 配置

`arch/risc-v/src/esp32p4/Kconfig`:

```kconfig
if ARCH_CHIP_ESP32P4

# ── CPU 配置 ──

config ESP32P4_HP_CORE_FREQ
	int "HP Core Maximum Frequency (MHz)"
	default 400
	---help---
		HP 核心最大频率。
		有效值: 160, 240, 320, 400。

config ESP32P4_LP_CORE
	bool "Enable LP (Low-Power) Core support"
	default n
	depends on OPENAMP
	---help---
		启用 LP RISC-V 核心支持。
		需要 OpenAMP/RPMsg 进行核间通信。

# ── 内存配置 ──

config ESP32P4_PSRAM
	bool "Enable PSRAM support"
	default y
	---help---
		启用外部 PSRAM (OPI) 作为第二内存区域。

if ESP32P4_PSRAM

config ESP32P4_PSRAM_SIZE
	int "PSRAM Size (MB)"
	default 32
	range 2 128

config ESP32P4_PSRAM_MODE_OPI
	bool "Use OPI mode for PSRAM"
	default y
	---help---
		使用 OPI 模式获得更高带宽。

endif # ESP32P4_PSRAM

# ── Flash 配置 ──

config ESP32P4_FLASH_SIZE
	int "Flash Size (MB)"
	default 16

config ESP32P4_FLASH_MODE_OPI
	bool "Use OPI mode for Flash"
	default n

# ── UART 配置 ──

config ESP32P4_UART0
	bool "Enable UART0"
	default y

config ESP32P4_UART1
	bool "Enable UART1"
	default n

config ESP32P4_UART2
	bool "Enable UART2"
	default n

# ── GPIO 配置 ──

config ESP32P4_GPIO
	bool "Enable GPIO driver"
	default y

config ESP32P4_GPIO_IRQ
	bool "Enable GPIO interrupt support"
	default n
	depends on ESP32P4_GPIO

# ── SPI 配置 ──

config ESP32P4_SPI0
	bool "Enable SPI0 (Flash/PSRAM)"
	default y

config ESP32P4_SPI1
	bool "Enable SPI1 (General Purpose)"
	default n

config ESP32P4_SPI2
	bool "Enable SPI2 (General Purpose)"
	default n

# ── I2C 配置 ──

config ESP32P4_I2C0
	bool "Enable I2C0"
	default n

config ESP32P4_I2C1
	bool "Enable I2C1"
	default n

# ── DMA 配置 ──

config ESP32P4_GDMA
	bool "Enable GDMA (General DMA)"
	default n

# ── USB 配置 ──

config ESP32P4_USBDEV
	bool "Enable USB Device Controller"
	default n
	select USBDEV
	select USBDEV_DUALSPEED
	---help---
		启用 USB 2.0 High-Speed 设备控制器。

# ── 定时器/看门狗 ──

config ESP32P4_TIMER
	bool "Enable Timer driver"
	default y

config ESP32P4_WDT
	bool "Enable Watchdog Timer"
	default n

# ── ADC/DAC ──

config ESP32P4_ADC
	bool "Enable ADC driver"
	default n

config ESP32P4_DAC
	bool "Enable DAC driver"
	default n

# ── 显示 ──

config ESP32P4_MIPI_DSI
	bool "Enable MIPI-DSI display controller"
	default n
	select VIDEO_FB if FRAMEBUFFER

# ── 以太网 ──

config ESP32P4_EMAC
	bool "Enable Ethernet MAC"
	default n
	select NETDEVICES
	depends on NET

# ── I2S 音频 ──

config ESP32P4_I2S
	bool "Enable I2S audio interface"
	default n

# ── 代码优化 ──

config ESP32P4_LTO
	bool "Enable Link-Time Optimization"
	default y
	select LTO
	---help---
		启用 LTO 减小代码体积。
		ESP32-P4 的 768KB SRAM 有限，LTO 能显著帮助。

endif # ARCH_CHIP_ESP32P4
```

#### 3.2.8 创建默认 defconfig

`boards/risc-v/esp32p4/esp32p4-evb/configs/default/defconfig`:

```
# Architecture
CONFIG_ARCH="risc-v"
CONFIG_ARCH_CHIP="esp32p4"
CONFIG_ARCH_CHIP_ESP32P4=y
CONFIG_ARCH_RV32=y
CONFIG_ARCH_RV32IMAFC=y
CONFIG_RISCV_TOOLCHAIN="gnu"

# Board
CONFIG_ARCH_BOARD="esp32p4-evb"
CONFIG_ARCH_BOARD_COMMON=y

# Scheduler
CONFIG_SCHED_HPWORK=y
CONFIG_SCHED_LPWORK=y

# Memory
CONFIG_RAM_START=0x4FF00000
CONFIG_RAM_SIZE=786432
CONFIG_MM_DEFAULT_ALIGNMENT=16

# Console
CONFIG_UART0_SERIAL_CONSOLE=y
CONFIG_ESP32P4_UART0=y

# GPIO
CONFIG_ESP32P4_GPIO=y

# Timer
CONFIG_ESP32P4_TIMER=y

# NuttX Shell
CONFIG_NSH_READLINE=y
CONFIG_NSH_ARCHINIT=y

# Board control
CONFIG_BOARDCTL=y
CONFIG_BOARD_LATE_INITIALIZE=y

# Debug
CONFIG_DEBUG_ASSERTIONS=y
CONFIG_DEBUG_FEATURES=y

# Code size optimization
CONFIG_ESP32P4_LTO=y
CONFIG_DEBUG_OPT_UNUSED_SECTIONS=y
```

#### 3.2.9 板级初始化

`boards/risc-v/esp32p4/esp32p4-evb/src/esp32p4_bringup.c`:

```c
/****************************************************************************
 * boards/risc-v/esp32p4/esp32p4-evb/src/esp32p4_bringup.c
 *
 * ESP32-P4 EVB 板级初始化
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/board.h>
#include <syslog.h>

#ifdef CONFIG_ESP32P4_GPIO
#  include "esp32p4_gpio.h"
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int esp32p4_bringup(void)
{
  int ret = OK;

#ifdef CONFIG_ESP32P4_GPIO
  /* 注册 GPIO 设备 */

  ret = esp32p4_gpio_init();
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: GPIO init failed: %d\n", ret);
    }
#endif

#ifdef CONFIG_ESP32P4_SPI1
  /* 注册 SPI1 总线 */

  ret = esp32p4_spidev_register(1);
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: SPI1 init failed: %d\n", ret);
    }
#endif

#ifdef CONFIG_ESP32P4_I2C0
  /* 注册 I2C0 总线 */

  ret = esp32p4_i2cbus_register(0);
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: I2C0 init failed: %d\n", ret);
    }
#endif

  return ret;
}
```

#### 3.2.10 构建系统集成

`arch/risc-v/src/esp32p4/Make.defs`:

```makefile
include $(TOPDIR)/Make.defs

# 芯片架构源文件

CHIP_CSRCS  = esp32p4_start.c
CHIP_CSRCS += esp32p4_irq.c
CHIP_CSRCS += esp32p4_clockconfig.c
CHIP_CSRCS += esp32p4_allocateheap.c
CHIP_CSRCS += esp32p4_serial.c
CHIP_CSRCS += esp32p4_lowputs.c
CHIP_CSRCS += esp32p4_timerisr.c

ifeq ($(CONFIG_ESP32P4_GPIO),y)
CHIP_CSRCS += esp32p4_gpio.c
endif

ifeq ($(CONFIG_ESP32P4_WDT),y)
CHIP_CSRCS += esp32p4_wdt.c
endif

ifeq ($(CONFIG_ESP32P4_SPI0),y)
CHIP_CSRCS += esp32p4_spi.c
else ifeq ($(CONFIG_ESP32P4_SPI1),y)
CHIP_CSRCS += esp32p4_spi.c
else ifeq ($(CONFIG_ESP32P4_SPI2),y)
CHIP_CSRCS += esp32p4_spi.c
endif

ifeq ($(CONFIG_ESP32P4_I2C0),y)
CHIP_CSRCS += esp32p4_i2c.c
else ifeq ($(CONFIG_ESP32P4_I2C1),y)
CHIP_CSRCS += esp32p4_i2c.c
endif

ifeq ($(CONFIG_ESP32P4_GDMA),y)
CHIP_CSRCS += esp32p4_dma.c
endif

ifeq ($(CONFIG_ESP32P4_USBDEV),y)
CHIP_CSRCS += esp32p4_usbdev.c
endif

ifeq ($(CONFIG_ESP32P4_PSRAM),y)
CHIP_CSRCS += esp32p4_psram.c
endif

ifeq ($(CONFIG_ESP32P4_MIPI_DSI),y)
CHIP_CSRCS += esp32p4_mipidsi.c
endif

ifeq ($(CONFIG_ESP32P4_EMAC),y)
CHIP_CSRCS += esp32p4_emac.c
endif

ifeq ($(CONFIG_ESP32P4_I2S),y)
CHIP_CSRCS += esp32p4_i2s.c
endif

ifdef CONFIG_SMP
CHIP_CSRCS += esp32p4_smp.c
endif

DEPPATH += --dep-path chip
VPATH += :chip
CFLAGS += ${INCDIR_PREFIX}$(TOPDIR)$(DELIM)arch$(DELIM)$(CONFIG_ARCH)$(DELIM)src$(DELIM)chip$(DELIM)include
```

### 3.3 Phase 2: SMP + PSRAM + Flash

#### 3.3.1 SMP 多核管理

创建 `esp32p4_smp.c`。ESP32-P4 双核启动序列:

1. Mask ROM 固定从 Core 0 开始执行
2. Core 0 在 `board_initialize()` 中通过 IPI 唤醒 Core 1
3. Core 1 进入 `esp32p4_cpu_boot()` 完成 SMP 初始化

```c
/****************************************************************************
 * arch/risc-v/src/esp32p4/esp32p4_smp.c
 *
 * ESP32-P4 SMP 双核管理
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/arch.h>
#include <nuttx/spinlock.h>

#include "riscv_internal.h"
#include "hardware/esp32p4_soc.h"

#ifdef CONFIG_SMP

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int up_cpu_start(int cpu)
{
  if (cpu != 1)
    {
      return -EINVAL;
    }

  /* TODO: 释放 Core 1 复位
   * 典型序列:
   * 1. 在 RTC_CNTL 寄存器中设置 Core 1 入口点
   * 2. 断言 Core 1 复位释放
   * 3. Core 1 将跳转到 esp32p4_cpu_boot(1)
   */

  return OK;
}

int up_cpu_pause(int cpu)
{
  /* TODO: 发送 IPI 暂停指定核心
   * 目标核心将进入 WFI 循环直到 up_cpu_resume() 被调用
   */

  return OK;
}

int up_cpu_resume(int cpu)
{
  /* TODO: 发送 IPI 唤醒指定核心 */

  return OK;
}

void esp32p4_cpu_boot(int cpu)
{
  /* Core 1 在此被 Core 0 释放后到达
   * SMP 框架处理后续初始化
   */

  /* 启用 Core 1 的 L1 Cache */

  esp32p4_cache_init();

  /* 进入 NuttX SMP 启动路径 */

  nx_smp_start(cpu);
}

#endif /* CONFIG_SMP */
```

SMP defconfig (`boards/risc-v/esp32p4/esp32p4-evb/configs/smp/defconfig`):

在默认配置基础上增加:

```
# SMP
CONFIG_SMP=y
CONFIG_SMP_NCPUS=2
CONFIG_SPINLOCK=y

# PSRAM
CONFIG_ESP32P4_PSRAM=y
CONFIG_ESP32P4_PSRAM_SIZE=32
CONFIG_MM_REGIONS=2

# Flash
CONFIG_ESP32P4_FLASH_SIZE=16
```

#### 3.3.2 PSRAM 初始化

创建 `esp32p4_psram.c`。PSRAM 通过 SPI/OPI 接口访问，初始化时序复杂:

```c
/****************************************************************************
 * arch/risc-v/src/esp32p4/esp32p4_psram.c
 *
 * ESP32-P4 PSRAM 初始化和管理
 ****************************************************************************/

#include <nuttx/config.h>
#include <syslog.h>

#include "hardware/esp32p4_soc.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int esp32p4_psram_init(void)
{
  /* TODO: 参考 ESP-IDF 的 PSRAM 初始化代码
   *
   * 步骤:
   * 1. 配置 SPI/OPI 控制器
   * 2. 发送 PSRAM 识别命令 (Read ID)
   * 3. 配置 PSRAM 工作模式 (OPI/QPI)
   * 4. 配置 cache 映射
   * 5. 执行 PSRAM 自检 (可选)
   *
   * 注意:
   * - PSRAM 初始化必须在 bootloader 阶段完成
   * - 高速运行时需要注意时序稳定性
   * - 建议配置 write-back cache 策略
   */

  syslog(LOG_INFO, "ESP32-P4: Initializing PSRAM (%d MB, %s mode)\n",
         CONFIG_ESP32P4_PSRAM_SIZE,
         CONFIG_ESP32P4_PSRAM_MODE_OPI ? "OPI" : "QPI");

  return OK;
}
```

#### 3.3.3 Flash 文件系统

Flash 分区方案 (16MB):

| 分区 | 大小 | 用途 |
|------|------|------|
| Bootloader | 32KB | 一级引导 |
| App (nuttx.bin) | 2MB | openvela 固件 |
| LittleFS | 4MB | 用户文件系统 |
| NVS | 256KB | 非易失存储 |
| LP Firmware | 256KB | LP 核心固件 |
| OTA Backup | 2MB | OTA 备份分区 |
| Reserved | ~9.5MB | 预留空间 |

### 3.4 Phase 3: 核心外设

#### 3.4.1 SPI 总线驱动

遵循 NuttX SPI bus framework (`struct spi_dev_s`):

```c
/****************************************************************************
 * arch/risc-v/src/esp32p4/esp32p4_spi.c
 *
 * ESP32-P4 SPI 总线驱动
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/spi/spi.h>

static const struct spi_ops_s g_spi_ops =
{
  .lock          = esp32p4_spi_lock,
  .select        = esp32p4_spi_select,
  .setfrequency  = esp32p4_spi_setfrequency,
  .setmode       = esp32p4_spi_setmode,
  .setbits       = esp32p4_spi_setbits,
  .exchange      = esp32p4_spi_exchange,
  .send          = esp32p4_spi_send,
  .recvblock     = esp32p4_spi_recvblock,
  .status        = esp32p4_spi_status,
};

FAR struct spi_dev_s *esp32p4_spibus_initialize(int bus)
{
  /* TODO: 分配 SPI 设备结构
   * 配置 GPIO 引脚
   * 使能 SPI 外设时钟
   * 返回 spi_dev_s 指针
   */

  return NULL;
}
```

#### 3.4.2 USB OTG HS DCD 驱动

USB DCD 驱动位于架构层 (`arch/risc-v/src/esp32p4/esp32p4_usbdev.c`)，不在 `drivers/` 目录。参考 `usbdev_dcd_pattern.md` 的关键约束:

1. PHY 必须在 Core init 之前初始化
2. SET_ADDRESS 必须由驱动直接处理，禁止分发给 class driver
3. 必须区分 2-stage 和 3-stage 控制传输
4. DMA 缓冲区必须满足 4096 字节页对齐
5. 必须开启 `CONFIG_USBDEV_DUALSPEED` (HS 模式 bulk maxpacket=512)

核心数据结构:

```c
struct esp32p4_usb_ep_s
{
  struct usbdev_ep_s ep;          /* 标准端点结构 */
  struct esp32p4_usb_s *priv;     /* 反向引用 */
  struct esp32p4_usb_req_s *head; /* 请求队列头 */
  struct esp32p4_usb_req_s *tail; /* 请求队列尾 */
};

struct esp32p4_usb_s
{
  struct usbdev_s usbdev;               /* NuttX USB device */
  struct usbdevclass_driver_s *driver;  /* 绑定的 class driver */
  uintptr_t base;                       /* USB 控制器基地址 */
  int irq;                              /* IRQ 号 */
};
```

### 3.5 Phase 4: 高级功能

#### 3.5.1 MIPI-DSI Framebuffer 驱动

使用 NuttX fb framework (`fb_vtable_s`)，参考 `fb_pattern.md`。注册函数使用 `fb_register_device` 直接注册 (Path A):

```c
int esp32p4_mipidsi_register(int display, uintptr_t base, int irq)
{
  FAR struct esp32p4_dsi_fb_s *priv;

  priv = kmm_zalloc(sizeof(struct esp32p4_dsi_fb_s));
  if (priv == NULL)
    {
      return -ENOMEM;
    }

  /* 初始化 videoinfo */
  priv->videoinfo.fmt     = FB_FMT_RGB16_565;
  priv->videoinfo.xres    = CONFIG_ESP32P4_DSI_HRES;
  priv->videoinfo.yres    = CONFIG_ESP32P4_DSI_VRES;
  priv->videoinfo.nplanes = 1;

  /* 初始化 planeinfo */
  priv->planeinfo.bpp    = 16;
  priv->planeinfo.stride = priv->videoinfo.xres * 2;
  priv->planeinfo.fblen  = priv->planeinfo.stride *
                           priv->videoinfo.yres * 2; /* 双缓冲 */

  /* 从 PSRAM 分配对齐的 framebuffer */
  priv->planeinfo.fbmem = kmm_memalign(64, priv->planeinfo.fblen);

  /* 填充 vtable 回调 */
  priv->vtable.getvideoinfo = esp32p4_dsi_getvideoinfo;
  priv->vtable.getplaneinfo = esp32p4_dsi_getplaneinfo;
  priv->vtable.pandisplay   = esp32p4_dsi_pandisplay;
  priv->vtable.setpower     = esp32p4_dsi_setpower;

  /* 注册 framebuffer 设备 */
  return fb_register_device(display, 0, &priv->vtable);
}
```

#### 3.5.2 以太网 MAC 驱动

使用 NuttX netdev framework (`netdev_lowerhalf_s`)，参考 `eth_netdev_pattern.md`:

```c
static const struct netdev_ops_s g_esp32p4_emac_ops =
{
  .ifup     = esp32p4_emac_ifup,
  .ifdown   = esp32p4_emac_ifdown,
  .transmit = esp32p4_emac_transmit,
  .receive  = esp32p4_emac_receive,
};
```

中断处理采用 ISR + Work Queue 模式 (方案 B): ISR 中禁用硬件中断并调度 work queue，work queue 中处理 TX 完成和 RX 接收，最后重新使能硬件中断。

#### 3.5.3 LP 核心通信

使用 OpenAMP/RPMsg 框架实现 HP 与 LP 核心间的通信:

```c
/****************************************************************************
 * arch/risc-v/src/esp32p4/esp32p4_lpcore.c
 *
 * LP 核心通过共享内存 + 软件中断与 HP 核心通信
 ****************************************************************************/

#include <openamp/open_amp.h>
#include <metal/device.h>

/* 共享内存区域定义 */
#define LP_SHM_BASE   0x50108000
#define LP_SHM_SIZE   0x4000

/* TODO: 实现 RPMsg virtio 设备初始化 */
```

---

## 四、代码示例

### 4.1 寄存器访问模式

ESP32-P4 外设寄存器位于 MMIO 地址空间，使用 NuttX 标准的 `getreg32`/`putreg32` 宏:

```c
/* 读写寄存器示例 */

#define UART_FIFO_REG(base)     ((base) + 0x00)
#define UART_STATUS_REG(base)   ((base) + 0x04)
#define UART_CLKDIV_REG(base)   ((base) + 0x14)

/* 读取 RX FIFO 数据 */

static inline uint8_t esp32p4_uart_rxbyte(uintptr_t base)
{
  return (uint8_t)(getreg32(UART_FIFO_REG(base)) & 0xff);
}

/* 写入 TX FIFO */

static inline void esp32p4_uart_txbyte(uintptr_t base, uint8_t ch)
{
  putreg32((uint32_t)ch, UART_FIFO_REG(base));
}
```

### 4.2 中断处理模式

```c
/****************************************************************************
 * UART 中断处理函数
 ****************************************************************************/

static int esp32p4_uart_interrupt(int irq, void *context, void *arg)
{
  struct uart_dev_s *dev = (struct uart_dev_s *)arg;
  uintptr_t base = (uintptr_t)dev->priv;
  uint32_t status;

  /* 读取中断状态 */

  status = getreg32(base + UART_INT_ST_REG);

  /* 处理 RX 中断 */

  if (status & UART_INT_RXFIFO)
    {
      uart_recvchars(dev);
    }

  /* 处理 TX 中断 */

  if (status & UART_INT_TXFIFO)
    {
      uart_xmitchars(dev);
    }

  /* 清除中断标志 */

  putreg32(status, base + UART_INT_CLR_REG);

  return OK;
}
```

### 4.3 GPIO 驱动示例

```c
/****************************************************************************
 * arch/risc-v/src/esp32p4/esp32p4_gpio.c
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/ioexpander/gpio.h>

/* GPIO 寄存器偏移 */

#define GPIO_OUT_REG        0x04
#define GPIO_IN_REG         0x08
#define GPIO_ENABLE_REG     0x20
#define GPIO_FUNC_OUT_SEL   0x530

static int esp32p4_gpio_read(struct gpio_dev_s *dev, bool *value)
{
  uintptr_t base = ESP32P4_GPIO_BASE;
  int pin = (int)(intptr_t)dev->priv;
  uint32_t regval;

  regval = getreg32(base + GPIO_IN_REG);
  *value = (regval >> pin) & 1;

  return OK;
}

static int esp32p4_gpio_write(struct gpio_dev_s *dev, bool value)
{
  uintptr_t base = ESP32P4_GPIO_BASE;
  int pin = (int)(intptr_t)dev->priv;

  if (value)
    {
      modifyreg32(base + GPIO_OUT_REG, 0, 1 << pin);
    }
  else
    {
      modifyreg32(base + GPIO_OUT_REG, 1 << pin, 0);
    }

  return OK;
}
```

### 4.4 定时器中断示例

```c
/****************************************************************************
 * arch/risc-v/src/esp32p4/esp32p4_timerisr.c
 *
 * 系统 tick 定时器
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/arch.h>
#include <nuttx/clock.h>

#include "hardware/esp32p4_soc.h"

#define TIMER_LOAD_REG(base)    ((base) + 0x00)
#define TIMER_VALUE_REG(base)   ((base) + 0x04)
#define TIMER_CTRL_REG(base)    ((base) + 0x08)
#define TIMER_INT_CLR_REG(base) ((base) + 0x0C)

void up_timer_initialize(void)
{
  uintptr_t base = ESP32P4_TIMER0_BASE;
  uint32_t load_value;

  /* 计算加载值: APB_CLK / TICK_PER_SEC */

  load_value = 80000000 / TICK_PER_SEC;  /* 假设 APB 80MHz */

  /* 配置定时器 */

  putreg32(load_value, TIMER_LOAD_REG(base));
  putreg32(TIMER_CTRL_AUTO_RELOAD | TIMER_CTRL_ENABLE,
           TIMER_CTRL_REG(base));

  /* 使能定时器中断 */

  up_enable_irq(ESP32P4_IRQ_TIMER0);
}

static int esp32p4_timer_interrupt(int irq, void *context, void *arg)
{
  /* 清除中断 */

  putreg32(1, TIMER_INT_CLR_REG(ESP32P4_TIMER0_BASE));

  /* 通知 NuttX 一个 tick 已过 */

  nxsched_process_timer();

  return OK;
}
```

---

## 五、测试验证

### 5.1 Phase 1 测试 (单核 Bring-up)

**编译测试**

```bash
cd ~/openvela
./build.sh boards/risc-v/esp32p4/esp32p4-evb/configs/default --cmake -j$(nproc)
```

预期结果: 零 error，生成 `nuttx.bin`。

**烧录测试**

```bash
esptool.py --chip esp32p4 --port /dev/ttyUSB0 --baud 921600 \
  write_flash 0x0 nuttx.bin
```

预期结果: 烧录成功，无校验错误。

**UART 控制台测试**

```bash
minicom -D /dev/ttyUSB0 -b 115200
```

预期结果: 看到 NuttX 启动日志，出现 `nsh>` 提示符。

| 测试项 | 验证方法 | 通过标准 |
|--------|----------|----------|
| 编译 | `./build.sh ...` | 零 error |
| 固件烧录 | `esptool.py write_flash` | 烧录成功 |
| UART 控制台 | 连接 UART0 | 看到启动日志 |
| NSH Shell | 输入命令 | `nsh>` 提示符可用 |
| GPIO LED | `nsh> gpio` | LED 点亮/熄灭 |
| Timer Tick | `nsh> date` | 系统时间正常递增 |
| 内存 | `nsh> free` | 显示正确的 SRAM 堆大小 |

### 5.2 Phase 2 测试 (SMP + PSRAM + Flash)

| 测试项 | 验证方法 | 通过标准 |
|--------|----------|----------|
| SMP 启动 | `nsh> cat /proc/cpuinfo` | 显示 2 个 CPU |
| SMP 调度 | `nsh> hello & hello &` | 两个任务交替执行 |
| PSRAM 检测 | `nsh> free` | 显示 SRAM + PSRAM 两个区域 |
| PSRAM 读写 | `nsh> dd if=/dev/zero of=/tmp/test bs=4096 count=1024` | 写入 4MB 成功 |
| Flash 分区 | `nsh> ls /dev/` | 显示 MTD 设备节点 |
| LittleFS | `nsh> mount -t lfs /dev/mtdblock1 /mnt` | 挂载成功 |
| 文件读写 | `nsh> echo "hello" > /mnt/test.txt && cat /mnt/test.txt` | 内容一致 |
| SMP 稳定性 | `nsh> ostest` | SMP 相关测试通过 |

### 5.3 Phase 3 测试 (核心外设)

| 测试项 | 验证方法 | 通过标准 |
|--------|----------|----------|
| SPI Flash | 读取 MTD 分区 | 数据正确 |
| I2C 传感器 | `nsh> cat /dev/sensors/temp0` | 读取温度值 |
| DMA 传输 | 大块数据 SPI 传输 | 传输速度提升 |
| USB 枚举 | `lsusb` | 看到设备 VID:PID |
| USB CDC-ACM | `nsh> echo "test" > /dev/ttyACM0` | PC 端收到数据 |
| 看门狗 | `nsh> wdog -t 5 -a` | 超时后系统重启 |
| ADC | 读取 ADC 通道值 | 值在合理范围 |

### 5.4 Phase 4 测试 (高级功能)

| 测试项 | 验证方法 | 通过标准 |
|--------|----------|----------|
| MIPI-DSI 显示 | `nsh> fb` (fb 示例) | 屏幕显示彩色矩形 |
| 以太网 | `nsh> ifconfig eth0 <ip> && ping <gateway>` | ping 通 |
| iperf 打流 | `iperf -c <server> -t 30` | TCP 吞吐量稳定 |
| I2S 音频 | 播放 WAV 文件 | 音频输出正常 |
| LP 核心通信 | RPMsg 发送消息 | LP 核心响应 |
| SD 卡 | `mount -t vfat /dev/mmcsd0 /mnt` | 挂载成功 |

### 5.5 压力测试

| 测试项 | 方法 | 持续时间 | 通过标准 |
|--------|------|----------|----------|
| 内存泄漏 | 循环分配/释放 + `free` 监控 | 1 小时 | 堆使用量稳定 |
| SMP 竞争 | 多高优先级任务争抢资源 | 30 分钟 | 无死锁/崩溃 |
| USB 热插拔 | 反复插拔 USB 线 | 50 次 | 每次枚举成功 |
| 网络长连接 | iperf 打流 > 10 分钟 | 10 分钟 | 无 ENOMEM |

### 5.6 自动化测试脚本

```bash
#!/bin/bash
# esp32p4_test.sh - ESP32-P4 自动化测试脚本

DEVICE="/dev/ttyUSB0"
BAUD=115200
LOG_FILE="test_results.log"

run_nsh_cmd() {
  local cmd="$1"
  local expected="$2"
  local timeout="${3:-5}"

  # 发送命令并捕获输出
  echo "$cmd" > "$DEVICE"
  sleep "$timeout"
  output=$(timeout 2 cat "$DEVICE" 2>/dev/null || true)

  if echo "$output" | grep -q "$expected"; then
    echo "[PASS] $cmd" | tee -a "$LOG_FILE"
    return 0
  else
    echo "[FAIL] $cmd (expected: $expected)" | tee -a "$LOG_FILE"
    return 1
  fi
}

echo "=== ESP32-P4 openvela 测试 ===" | tee "$LOG_FILE"
echo "时间: $(date)" | tee -a "$LOG_FILE"

# Phase 1 测试
echo "--- Phase 1: 基础功能 ---" | tee -a "$LOG_FILE"
run_nsh_cmd "uname -a" "NuttX"
run_nsh_cmd "free" "Total:"
run_nsh_cmd "date" "20"

# Phase 2 测试 (如果启用 SMP)
echo "--- Phase 2: SMP + PSRAM ---" | tee -a "$LOG_FILE"
run_nsh_cmd "cat /proc/cpuinfo" "cpu1"

echo "=== 测试完成 ===" | tee -a "$LOG_FILE"
```

---

## 六、常见问题

### 6.1 编译问题

**Q: 工具链找不到 `riscv32-esp-elf-gcc`**

```bash
# 检查工具链是否安装
which riscv32-esp-elf-gcc

# 如果使用 ESP-IDF 工具链
source ~/esp/esp-idf/export.sh

# 如果使用 apt 安装
sudo apt-get install -y gcc-riscv32-esp-elf
```

**Q: 编译时报 `unknown architecture` 错误**

确认 defconfig 中设置了正确的架构:

```
CONFIG_ARCH="risc-v"
CONFIG_ARCH_CHIP="esp32p4"
CONFIG_ARCH_RV32=y
CONFIG_ARCH_RV32IMAFC=y
```

**Q: 链接时报 `region 'sram' overflowed` 错误**

768KB SRAM 有限，需要优化代码尺寸:

```
# 启用 LTO
CONFIG_ESP32P4_LTO=y

# 启用未使用段回收
CONFIG_DEBUG_OPT_UNUSED_SECTIONS=y

# 裁剪不必要的功能
# 将不频繁执行的代码放到 Flash (XIP)
```

### 6.2 启动问题

**Q: 烧录后串口无输出**

排查步骤:

1. 确认 UART 引脚连接正确 (TX/RX 不要接反)
2. 确认波特率设置正确 (115200)
3. 检查 boot mode 引脚 (GPIO0/Strapping)
4. 使用 `esptool.py read_flash` 验证固件是否正确烧录
5. 尝试硬复位 (按 RST 按钮)

**Q: 系统启动后卡在 `nx_start()`**

可能原因:

- 时钟配置错误导致外设无法访问
- 中断控制器未正确初始化
- 堆内存配置错误

调试方法:

```c
/* 在 esp32p4_start.c 中添加早期调试输出 */

esp32p4_lowputs("ESP32-P4: clock init...\n");
esp32p4_clock_init();
esp32p4_lowputs("ESP32-P4: cache init...\n");
esp32p4_cache_init();
esp32p4_lowputs("ESP32-P4: entering nx_start...\n");
```

**Q: SMP 模式下 Core 1 无法启动**

排查步骤:

1. 确认 Core 1 复位释放序列正确
2. 确认 Core 1 入口地址正确设置
3. 确认 IPI 中断配置正确
4. 先用单核模式验证基本功能

### 6.3 外设问题

**Q: GPIO 无法控制 LED**

- 确认 GPIO 引脚号正确 (查阅 EVB 原理图)
- 确认 GPIO 方向已设置为输出
- 确认 GPIO 时钟已使能
- 检查是否有复用冲突

**Q: SPI Flash 读取失败**

- 确认 Flash 时钟配置正确
- 确认 Flash 模式 (QPI/OPI/Standard) 与硬件匹配
- 检查 cache 映射配置
- 验证 Flash 分区表

**Q: PSRAM 初始化失败**

- 确认 PSRAM 芯片型号与驱动匹配
- 检查 SPI/OPI 时序配置
- 降低 PSRAM 时钟频率测试
- 参考 ESP-IDF 的 PSRAM 初始化代码

### 6.4 USB 问题

**Q: USB 设备无法枚举**

- 确认 USB PHY 已正确初始化 (必须在 Core init 之前)
- 确认 USB 时钟配置正确 (480MHz HS 需要精确时钟)
- 使用 `usbtrace` 工具抓取 USB 通信日志
- 先测试 FS 模式 (12Mbps)，再启用 HS 模式
- 确认 DCD 驱动正确处理了 SET_ADDRESS 命令

**Q: USB 传输速度不达标**

- 确认 DMA 已启用且缓冲区对齐 (4096 字节)
- 确认 bulk maxpacket 大小正确 (HS: 512, FS: 64)
- 检查中断处理延迟
- 使用 USB 分析仪抓包分析

### 6.5 显示问题

**Q: MIPI-DSI 屏幕无显示**

- 确认 DSI 时钟配置正确
- 确认 DSI data lane 数量与屏幕匹配
- 检查初始化命令序列 (参考屏幕 datasheet)
- 确认 framebuffer 内存已正确分配 (从 PSRAM)
- 检查 TE (Tearing Effect) 信号

### 6.6 网络问题

**Q: 以太网无法获取 IP**

- 确认 PHY 芯片已正确初始化
- 检查 MDC/MDIO 通信是否正常
- 确认 link 状态 (`nsh> ifconfig eth0`)
- 检查 DHCP 客户端配置

**Q: 以太网吞吐量低**

- 确认 DMA 已启用
- 检查中断处理模式 (建议使用 work queue 方案 B)
- 调整 TCP 窗口大小
- 使用 iperf 定位瓶颈

---

## 七、附录

### 7.1 必须实现的文件清单

按优先级排序:

| 优先级 | 文件 | 功能 | 行数估算 |
|--------|------|------|----------|
| P0 | `esp32p4_start.c` | 启动入口 | ~200 |
| P0 | `esp32p4_irq.c` | 中断控制器 | ~300 |
| P0 | `esp32p4_clockconfig.c` | 时钟配置 | ~200 |
| P0 | `esp32p4_serial.c` | UART 驱动 | ~600 |
| P0 | `esp32p4_gpio.c` | GPIO 驱动 | ~400 |
| P0 | `esp32p4_timerisr.c` | 系统 tick | ~200 |
| P0 | `esp32p4_allocateheap.c` | 堆分配 | ~150 |
| P0 | `esp32p4_lowputs.c` | 早期调试输出 | ~100 |
| P0 | `esp32p4_bringup.c` | 板级初始化 | ~200 |
| P1 | `esp32p4_smp.c` | SMP 管理 | ~300 |
| P1 | `esp32p4_psram.c` | PSRAM 驱动 | ~400 |
| P1 | `esp32p4_flash.c` | Flash/MTD | ~500 |
| P1 | `esp32p4_spi.c` | SPI 总线 | ~600 |
| P1 | `esp32p4_i2c.c` | I2C 总线 | ~500 |
| P1 | `esp32p4_dma.c` | GDMA | ~400 |
| P1 | `esp32p4_usbdev.c` | USB DCD | ~2500 |
| P2 | `esp32p4_mipidsi.c` | MIPI-DSI FB | ~1500 |
| P2 | `esp32p4_emac.c` | 以太网 MAC | ~1500 |
| P2 | `esp32p4_i2s.c` | I2S 音频 | ~800 |
| P2 | `esp32p4_lpcore.c` | LP 核心通信 | ~600 |

### 7.2 风险评估

| 风险 | 等级 | 缓解措施 |
|------|------|----------|
| SMP 双核稳定性 | 高 | 先完成单核 bring-up；参考 QEMU RISC-V 64 SMP 实现 |
| PSRAM 初始化失败 | 中 | 参考 ESP-IDF 代码；添加自检；配置合理 cache 策略 |
| USB OTG HS 枚举失败 | 中 | 先实现 FS 模式；使用 usbtrace 调试 |
| 代码尺寸超过 SRAM | 中 | 启用 LTO 和 section GC；裁剪不必要功能 |
| 寄存器定义错误 | 低 | 从 ESP-IDF 源码提取；与 datasheet 交叉验证 |
| 工具链兼容性 | 低 | 使用 ESP-IDF 提供的 gcc-riscv32-esp-elf |

### 7.3 参考文档

| 文档 | 说明 |
|------|------|
| ESP32-P4 Technical Reference Manual | 寄存器定义、外设时序、中断号 |
| ESP32-P4 Datasheet | 电气特性、引脚定义、封装信息 |
| NuttX Documentation | 内核 API、驱动框架、SMP 模型 |
| `chip-specific-notes.md` | openvela 已支持芯片的配置说明 |
| `nuttx-driver-development/SKILL.md` | NuttX 驱动开发完整流程 |
| `usbdev_dcd_pattern.md` | USB DCD 驱动模式参考 |
| `fb_pattern.md` | Framebuffer 驱动模式参考 |
| `eth_netdev_pattern.md` | 以太网驱动模式参考 |
| `coding_rules.md` | NuttX 编码规范 |
| `board_registration.md` | 板级注册模式参考 |

### 7.4 构建命令速查

```bash
# 默认配置 (单核)
./build.sh boards/risc-v/esp32p4/esp32p4-evb/configs/default --cmake -j$(nproc)

# SMP 配置
./build.sh boards/risc-v/esp32p4/esp32p4-evb/configs/smp --cmake -j$(nproc)

# NSH 最小系统
./build.sh boards/risc-v/esp32p4/esp32p4-evb/configs/nsh --cmake -j$(nproc)

# 清理后重新构建
./build.sh boards/risc-v/esp32p4/esp32p4-evb/configs/default --cmake --clean -j$(nproc)

# 运行 menuconfig
./build.sh boards/risc-v/esp32p4/esp32p4-evb/configs/default --cmake --menuconfig

# 烧录固件
esptool.py --chip esp32p4 --port /dev/ttyUSB0 --baud 921600 \
  write_flash 0x0 cmake_out/boards_risc-v_esp32p4_esp32p4-evb_configs_default/nuttx.bin

# 代码规范检查
nuttx/tools/checkpatch.sh -f arch/risc-v/src/esp32p4/esp32p4_serial.c
```

### 7.5 调试方法

**JTAG 调试**

```bash
# 启动 OpenOCD
openocd -f interface/esp_usb_jtag.cfg -f target/esp32p4.cfg

# 连接 GDB
gdb-multiarch cmake_out/.../nuttx
(gdb) target remote :3333
(gdb) break esp32p4_start
(gdb) continue
```

**串口日志调试**

```bash
# 启用详细日志
CONFIG_DEBUG_ASSERTIONS=y
CONFIG_DEBUG_FEATURES=y
CONFIG_DEBUG_ERROR=y
CONFIG_DEBUG_WARN=y
CONFIG_DEBUG_INFO=y
```

**usbtrace (USB 调试)**

```bash
nsh> cat /dev/usbtrace
```

---

本文档涵盖了 ESP32-P4 适配 openvela 的完整流程，从硬件概述到环境准备、分阶段适配步骤、代码示例、测试验证方案和常见问题解答。适配过程建议采用渐进式策略，先完成单核最小系统 (Phase 1)，验证基本功能后再逐步添加 SMP、PSRAM、外设驱动和高级功能。
