# ESP32-P4 Function EV Board 烧录与模块测试指南

> 本文档提供 ESP32-P4 Function EV Board 的完整烧录流程和各模块测试方法，适用于 openvela NuttX 系统。
>
> 最后更新：2026-08-25

---

## 目录

1. [烧录准备](#1-烧录准备)
2. [烧录方法](#2-烧录方法)
3. [esp-idf monitor 使用](#3-esp-idf-monitor-使用)
4. [模块测试指南](#4-模块测试指南)
5. [常见问题排查](#5-常见问题排查)

---

## 1. 烧录准备

### 1.1 USB 连接方式

ESP32-P4 Function EV Board 提供两种 USB 接口用于开发调试：

| 接口类型 | 用途 | 连接位置 | 说明 |
|----------|------|----------|------|
| **USB-JTAG** | 调试、烧录 | 板载 USB-C (标记为 USB) | 内置 USB-JTAG 适配器，支持 OpenOCD 调试和 JTAG 烧录 |
| **USB-UART** | 串口通信、烧录 | 板载 USB-C (标记为 UART) | 通过 USB-UART 桥接芯片（如 CP2102/CH340）连接 UART0 |

**连接建议**：

- 日常开发建议同时连接两个 USB 接口：UART 用于串口监控，JTAG 用于调试
- 烧录时使用任一接口均可，USB-UART 更为通用
- 首次连接时，系统会自动识别设备并分配串口设备文件

### 1.2 串口设备识别

连接 USB 线后，Linux 系统会自动识别设备。使用以下命令确认设备分配：

```bash
# 查看新出现的串口设备
ls -la /dev/ttyACM* /dev/ttyUSB* 2>/dev/null

# 查看内核日志确认设备识别
dmesg | tail -20

# 查看所有 USB 设备
lsusb
```

**设备命名规则**：

| 设备文件 | 对应接口 | 说明 |
|----------|----------|------|
| `/dev/ttyACM0` | USB-JTAG | 内置 USB-JTAG 适配器（CDC ACM 类设备） |
| `/dev/ttyUSB0` | USB-UART | USB-UART 桥接芯片（CP2102/CH340 等） |

> **注意**：设备编号取决于插入顺序，可能为 `ttyACM0`、`ttyACM1` 或 `ttyUSB0`、`ttyUSB1`。建议通过 `dmesg` 确认实际分配的设备号。

### 1.3 udev 规则配置（Linux）

为避免每次都需要 `sudo` 权限访问串口设备，建议配置 udev 规则：

```bash
# 创建 udev 规则文件（需要 sudo 权限）
sudo tee /etc/udev/rules.d/99-esp32.rules << 'EOF'
# Espressif USB-JTAG (CDC ACM)
SUBSYSTEM=="tty", ATTRS{idVendor}=="303a", ATTRS{idProduct}=="1001", MODE="0666", GROUP="dialout", TAG+="uaccess"

# CP2102 USB-UART Bridge
SUBSYSTEM=="tty", ATTRS{idVendor}=="10c4", ATTRS{idProduct}=="ea60", MODE="0666", GROUP="dialout", TAG+="uaccess"

# CH340 USB-UART Bridge
SUBSYSTEM=="tty", ATTRS{idVendor}=="1a86", ATTRS{idProduct}=="7523", MODE="0666", GROUP="dialout", TAG+="uaccess"

# FT2232H (JTAG)
SUBSYSTEM=="tty", ATTRS{idVendor}=="0403", ATTRS{idProduct}=="6010", MODE="0666", GROUP="dialout", TAG+="uaccess"
EOF

# 重新加载 udev 规则
sudo udevadm control --reload-rules
sudo udevadm trigger

# 将当前用户添加到 dialout 组
sudo usermod -a -G dialout $USER
```

**验证权限**：

```bash
# 确认设备权限（应显示 crw-rw-rw- 或包含 dialout 组）
ls -la /dev/ttyACM0 /dev/ttyUSB0

# 确认当前用户在 dialout 组中
groups $USER
```

> **注意**：添加用户到 dialout 组后需要重新登录才能生效。如果不想重新登录，可以临时使用 `newgrp dialout`。

---

## 2. 烧录方法

### 2.1 烧录前准备：进入下载模式

ESP32-P4 默认从 SPI Flash 启动。如果芯片从未烧录过固件，或固件损坏无法启动，芯片会自动进入下载模式。

**手动进入下载模式**：

1. 按住 **BOOT** 按钮（GPIO 21）
2. 短按 **RST** 按钮（复位）
3. 松开 **BOOT** 按钮

此时芯片进入 UART 下载模式，等待固件传输。

### 2.2 方法一：esptool.py 直接烧录

`esptool.py` 是 Espressif 官方的 Flash 烧录工具，支持直接通过串口烧录固件。

**安装 esptool**：

```bash
# 通过 pip 安装
pip install esptool

# 或安装指定版本（推荐 4.x 以上）
pip install esptool>=4.7
```

**烧录命令**：

```bash
# 基本烧录命令
esptool.py --chip esp32p4 --port /dev/ttyACM0 --baud 460800 \
    write_flash 0x0 nuttx.bin

# 指定 Flash 大小（如果自动检测失败）
esptool.py --chip esp32p4 --port /dev/ttyACM0 --baud 460800 \
    --flash_size 16MB \
    write_flash 0x0 nuttx.bin

# 烧录多个分区（如果使用分区表）
esptool.py --chip esp32p4 --port /dev/ttyACM0 --baud 460800 \
    write_flash \
    0x0 bootloader.bin \
    0x8000 partition-table.bin \
    0x10000 nuttx.bin
```

**常用参数说明**：

| 参数 | 说明 |
|------|------|
| `--chip esp32p4` | 指定目标芯片型号 |
| `--port /dev/ttyACM0` | 指定串口设备 |
| `--baud 460800` | 波特率（可选 115200/230400/460800/921600） |
| `--flash_size 16MB` | 指定 Flash 容量 |
| `--flash_mode dout` | Flash 读取模式（dio/dout/qio/qout） |
| `--erase-all` | 烧录前擦除整个 Flash |

**擦除 Flash**：

```bash
# 擦除整个 Flash（慎用，会清除所有数据）
esptool.py --chip esp32p4 --port /dev/ttyACM0 erase_flash

# 擦除指定区域
esptool.py --chip esp32p4 --port /dev/ttyACM0 erase_region 0x0 0x10000
```

**读取 Flash 内容**：

```bash
# 读取 Flash 内容到文件（用于备份或分析）
esptool.py --chip esp32p4 --port /dev/ttyACM0 \
    read_flash 0x0 0x10000 flash_dump.bin
```

**验证烧录**：

```bash
# 烧录后验证固件完整性
esptool.py --chip esp32p4 --port /dev/ttyACM0 \
    verify_flash 0x0 nuttx.bin

# 查看芯片信息
esptool.py --chip esp32p4 --port /dev/ttyACM0 chip_id

# 查看 Flash 信息
esptool.py --chip esp32p4 --port /dev/ttyACM0 flash_id
```

### 2.3 方法二：ESP-IDF idf.py 烧录

如果使用 ESP-IDF 构建系统，可以通过 `idf.py` 一键烧录。

**配置串口**：

```bash
# 设置目标芯片
idf.py set-target esp32p4

# 设置串口（首次使用时需要）
idf.py -p /dev/ttyACM0 build
```

**烧录命令**：

```bash
# 编译并烧录
idf.py -p /dev/ttyACM0 flash

# 仅烧录（不重新编译）
idf.py -p /dev/ttyACM0 flash

# 指定波特率烧录
idf.py -p /dev/ttyACM0 -b 460800 flash
```

**openvela 工程中的使用**：

```bash
# 在 openvela 工程中，使用 NuttX 的构建系统生成固件后
# 固件通常位于 nuttx.bin，使用 esptool.py 烧录
cd /home/geo/openvela

# 编译
./tools/configure.sh evb_esp32p4:nsh
make -j$(nproc)

# 烧录（使用 esptool.py）
esptool.py --chip esp32p4 --port /dev/ttyACM0 --baud 460800 \
    write_flash 0x0 nuttx.bin
```

### 2.4 方法三：JTAG 烧录

JTAG 烧录通过 USB-JTAG 接口进行，无需额外的 JTAG 适配器（ESP32-P4 内置 USB-JTAG）。

#### 2.4.1 OpenOCD 配置

**安装 OpenOCD**（支持 ESP32-P4 的版本）：

```bash
# 从 Espressif fork 安装（推荐，支持 ESP32-P4）
# https://github.com/espressif/openocd-esp32
git clone https://github.com/espressif/openocd-esp32.git
cd openocd-esp32
./bootstrap
./configure --enable-ftdi --enable-esp32p4
make -j$(nproc)
sudo make install
```

**创建 OpenOCD 配置文件** (`esp32p4-evb.cfg`)：

```tcl
# ESP32-P4 Function EV Board OpenOCD Configuration

# 使用内置 USB-JTAG 适配器
adapter driver esp_usb_jtag
esp_usb_jtag vid_pid 0x303a 0x1001
esp_usb_jtag speed 20000

# ESP32-P4 目标配置
set ESP32P4_RTOS nuttx

target create esp32p4.riscv esp32p4 -endian little -chain-position esp32p4.riscv

# Flash 配置
flash bank esp32p4.flash0 esp32p4 0x42000000 0 0 0 esp32p4.flash0
flash bank esp32p4.flash1 esp32p4 0x42800000 0 0 0 esp32p4.flash1

# 初始化
init
```

**启动 OpenOCD**：

```bash
# 使用配置文件启动 OpenOCD
openocd -f esp32p4-evb.cfg

# 预期输出：
# Info : esp_usb_jtag: found device with vid:303a pid:1001
# Info : Listening on port 3333 for gdb connections
# Info : Listening on port 6666 for tcl connections
# Info : Listening on port 4444 for telnet connections
```

#### 2.4.2 GDB 连接

**使用 GDB 烧录固件**：

```bash
# 启动 GDB（使用 RISC-V GDB）
riscv-none-elf-gdb nuttx

# 在 GDB 中连接 OpenOCD
(gdb) target remote :3333

# 加载固件到 Flash
(gdb) monitor flash write_image erase nuttx.bin 0x0

# 或加载到 RAM（用于调试）
(gdb) load nuttx

# 复位并运行
(gdb) monitor reset halt
(gdb) continue
```

**GDB 调试常用命令**：

```bash
# 连接目标
(gdb) target remote :3333

# 复位芯片
(gdb) monitor reset halt

# 设置断点
(gdb) break main
(gdb) break esp32p4_bringup

# 单步执行
(gdb) step
(gdb) next

# 查看寄存器
(gdb) info registers

# 查看内存
(gdb) x/16xw 0x4FF00000

# 继续运行
(gdb) continue

# 查看调用栈
(gdb) backtrace
```

---

## 3. esp-idf monitor 使用

### 3.1 启动命令

```bash
# 使用 esp-idf monitor 查看串口输出
idf.py -p /dev/ttyACM0 monitor

# 或使用 openvela 的串口终端工具
minicom -D /dev/ttyACM0 -b 115200

# 或使用 screen
screen /dev/ttyACM0 115200

# 或使用 picocom
picocom -b 115200 /dev/ttyACM0
```

### 3.2 快捷键说明

| 快捷键 | 功能 |
|--------|------|
| `Ctrl-]` | 退出 monitor |
| `Ctrl-T` | 进入菜单模式（再按其他键执行功能） |
| `Ctrl-T Ctrl-R` | 复位芯片（发送 RTS 信号） |
| `Ctrl-T Ctrl-H` | 显示帮助信息 |
| `Ctrl-T Ctrl-G` | 触发 GDB Stub（进入 GDB 调试模式） |
| `Ctrl-T Ctrl-D` | 触发 Core Dump 解析 |
| `Ctrl-T Ctrl-F` | 切换自动滚动 |
| `Ctrl-T Ctrl-E` | 切换转义字符显示 |

### 3.3 Panic Backtrace 解析

当系统发生 panic（内核崩溃）时，串口会输出类似以下信息：

```
assertion failed at file:xxx.c line:xxx function:xxx
PANIC: panic at xxx.c:xxx
Backtrace: 0x4ff01234:0x4ff00100 0x4ff01230:0x4ff000f0 0x4ff0122c:0x4ff000e0
```

**使用 addr2line 解析地址**：

```bash
# 方法一：使用 esp-idf 的 idf.py monitor 自动解析
idf.py -p /dev/ttyACM0 monitor
# monitor 会自动检测 backtrace 并使用 addr2line 解析

# 方法二：手动解析
riscv-none-elf-addr2line -pfiaC -e nuttx 0x4ff01234 0x4ff01230 0x4ff0122c

# 方法三：使用 GDB 解析
riscv-none-elf-gdb nuttx -batch -ex "list *0x4ff01234"
```

**配置 Backtrace 输出**：

确保 defconfig 中包含以下配置以获取完整的 backtrace 信息：

```
CONFIG_ARCH_STACKDUMP=y
CONFIG_DEBUG_ASSERTIONS=y
CONFIG_DEBUG_SYMBOLS=y
```

### 3.4 Core Dump 配置和分析

Core Dump 在系统 panic 时将内存快照保存到 Flash 或 UART，便于事后分析。

**配置 Core Dump**（在 defconfig 中添加）：

```
# 启用 Core Dump
CONFIG_ESP32P4_COREDUMP=y

# Core Dump 保存方式（二选一）
CONFIG_ESP32P4_COREDUMP_TO_FLASH=y    # 保存到 Flash（推荐）
# CONFIG_ESP32P4_COREDUMP_TO_UART=y   # 通过 UART 输出

# 保存到 Flash 时的分区配置
CONFIG_ESP32P4_COREDUMP_PARTITION_LABEL="coredump"
```

**分析 Core Dump**：

```bash
# 方法一：通过 idf.py monitor 自动解析
idf.py -p /dev/ttyACM0 monitor
# 当 panic 发生时，monitor 会提示是否解析 core dump

# 方法二：手动解析
# 从 Flash 读取 core dump
esptool.py --chip esp32p4 --port /dev/ttyACM0 \
    read_flash <coredump_offset> <size> coredump.bin

# 使用 espcoredump 工具分析
espcoredump info_corefile -t raw -c coredump.bin nuttx

# 在 GDB 中加载 core dump
riscv-none-elf-gdb nuttx -ex "target core coredump.bin"
(gdb) bt        # 查看调用栈
(gdb) info threads  # 查看所有线程
```

### 3.5 GDB Stub 调试模式

GDB Stub 在 panic 时自动启动 GDB 调试服务器，允许实时调试崩溃点。

**配置 GDB Stub**：

```
CONFIG_ESP32P4_GDBSTUB=y
```

**使用 GDB Stub**：

```bash
# 1. 烧录包含 GDB Stub 的固件
esptool.py --chip esp32p4 --port /dev/ttyACM0 --baud 460800 \
    write_flash 0x0 nuttx.bin

# 2. 启动串口终端
idf.py -p /dev/ttyACM0 monitor

# 3. 当 panic 发生时，串口会输出：
# Entering GDB Stub...
# 系统暂停在崩溃点

# 4. 在另一个终端启动 GDB 连接
riscv-none-elf-gdb nuttx
(gdb) target remote /dev/ttyACM0
(gdb) bt          # 查看崩溃时的调用栈
(gdb) info locals # 查看局部变量
(gdb) list        # 查看源码
```

---

## 4. 模块测试指南

本节介绍 ESP32-P4 EVB 各主要模块的测试方法。测试基于 NSH Shell 进行交互式验证。

### 4.1 UART 串口测试

ESP32-P4 EVB 的 UART0 作为默认串口控制台，波特率 115200，8N1 格式。

**前提条件**：

```
CONFIG_ESP32P4_UART0=y
CONFIG_ESP32P4_UART0_SERIAL_CONSOLE=y
```

#### 4.1.1 串口基础验证

```bash
# 启动串口终端
minicom -D /dev/ttyACM0 -b 115200

# 按回车键，应看到 NSH 提示符
nsh>
```

#### 4.1.2 串口回环测试

串口回环测试将 TX 和 RX 引脚短接，验证数据收发正确性。

**硬件连接**：将 UART0 的 TX 和 RX 引脚短接（跳线或杜邦线）。

```bash
# 在 NSH 中使用 echo 测试
nsh> echo "Hello ESP32-P4" > /dev/ttyS0
# 应在终端看到 "Hello ESP32-P4"（回环回来的数据）

# 使用 cat 读取
nsh> cat /dev/ttyS0 &
# 在后台持续读取串口数据
```

#### 4.1.3 波特率验证

```bash
# 查看当前串口配置
nsh> stty -F /dev/ttyS0

# 测试不同波特率
# 在 host 侧使用不同波特率连接
# minicom -D /dev/ttyACM0 -b 9600    # 9600 bps
# minicom -D /dev/ttyACM0 -b 115200  # 115200 bps（默认）
# minicom -D /dev/ttyACM0 -b 921600  # 921600 bps
```

### 4.2 GPIO 测试

ESP32-P4 EVB 的 GPIO 配置：

| GPIO | 功能 | 说明 |
|------|------|------|
| GPIO 26 | 板载 LED | 输出，高电平点亮 |
| GPIO 21 | BOOT 按钮 | 输入，内部上拉，低电平有效 |

**前提条件**：

```
CONFIG_ESP32P4_GPIO=y
CONFIG_DEV_GPIO=y
```

#### 4.2.1 LED 闪烁测试

**方法一：使用 NuttX GPIO 工具**

```bash
# 在 NSH 中控制 LED（GPIO 26）
# 设置 GPIO 26 为输出模式
nsh> echo 1 > /dev/gpio26   # LED 亮
nsh> echo 0 > /dev/gpio26   # LED 灭

# LED 闪烁脚本
nsh> while true; do echo 1 > /dev/gpio26; usleep 500000; \
     echo 0 > /dev/gpio26; usleep 500000; done
```

**方法二：使用 NSH builtin 应用**

如果配置了 GPIO 测试应用：

```bash
# 编译时启用 GPIO 测试应用
CONFIG_EXAMPLES_GPIO=y

# 运行 GPIO 测试
nsh> gpio
```

**方法三：编写自定义测试程序**

```c
/* gpio_led_test.c */
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main(void)
{
    int fd;
    int value;

    fd = open("/dev/gpio26", O_WRONLY);
    if (fd < 0)
    {
        perror("Failed to open GPIO 26");
        return 1;
    }

    /* 闪烁 10 次 */
    for (int i = 0; i < 10; i++)
    {
        value = 1;
        write(fd, &value, sizeof(value));
        usleep(500000);

        value = 0;
        write(fd, &value, sizeof(value));
        usleep(500000);
    }

    close(fd);
    return 0;
}
```

#### 4.2.2 按键中断测试

**方法一：使用 NuttX GPIO 工具**

```bash
# 读取 BOOT 按钮状态（GPIO 21）
nsh> cat /dev/gpio21
# 应返回 1（未按下）或 0（按下）

# 持续监控按键状态
nsh> while true; do cat /dev/gpio21; usleep 100000; done
```

**方法二：编写中断测试程序**

```c
/* gpio_button_test.c */
#include <stdio.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

int main(void)
{
    int fd;
    struct pollfd pfd;
    int value;
    int ret;

    fd = open("/dev/gpio21", O_RDONLY);
    if (fd < 0)
    {
        perror("Failed to open GPIO 21");
        return 1;
    }

    pfd.fd = fd;
    pfd.events = POLLPRI;

    printf("Press BOOT button to trigger interrupt...\n");

    while (1)
    {
        ret = poll(&pfd, 1, -1);  /* 等待中断 */
        if (ret > 0)
        {
            lseek(fd, 0, SEEK_SET);
            read(fd, &value, sizeof(value));
            printf("Button %s\n", value ? "released" : "pressed");
        }
    }

    close(fd);
    return 0;
}
```

### 4.3 SDMMC / SD 卡测试

ESP32-P4 EVB 通过 SDMMC 接口连接 SD 卡，默认挂载到 `/mnt/sdcard`。

**SDMMC GPIO 引脚**：

| 信号 | GPIO | 说明 |
|------|------|------|
| CLK | GPIO 43 | SDMMC 时钟 |
| CMD | GPIO 44 | SDMMC 命令线 |
| D0 | GPIO 39 | 数据线 0 |
| D1 | GPIO 40 | 数据线 1（4-bit 模式） |
| D2 | GPIO 41 | 数据线 2（4-bit 模式） |
| D3 | GPIO 42 | 数据线 3（4-bit 模式） |

**前提条件**：

```
CONFIG_ESP32P4_SDMMC=y
CONFIG_ESP32P4_SDMMC_DMA=y
CONFIG_ESP32P4_SDMMC_FREQ=40000000
CONFIG_ESP32P4_EVB_SDCARD=y
CONFIG_ESP32P4_EVB_SDCARD_AUTOMOUNT=y
CONFIG_ESP32P4_EVB_SDCARD_MOUNTPOINT="/mnt/sdcard"
CONFIG_MMCSD=y
CONFIG_MMCSD_SDIO=y
CONFIG_SDMMC=y
CONFIG_FS_FAT=y
CONFIG_FS_FAT_LFN=y
```

#### 4.3.1 SD 卡挂载测试

```bash
# 检查 SD 卡是否自动挂载
nsh> ls /mnt/sdcard/

# 查看 SD 卡信息
nsh> df -h /mnt/sdcard/

# 查看块设备
nsh> ls /dev/mmcsd*

# 如果未自动挂载，手动挂载
nsh> mount -t vfat /dev/mmcsd0 /mnt/sdcard
```

#### 4.3.2 文件读写测试

```bash
# 写入测试文件
nsh> echo "ESP32-P4 SD card test" > /mnt/sdcard/test.txt

# 读取测试文件
nsh> cat /mnt/sdcard/test.txt
# 预期输出：ESP32-P4 SD card test

# 创建目录
nsh> mkdir /mnt/sdcard/testdir

# 列出文件
nsh> ls -la /mnt/sdcard/

# 写入大文件测试（1MB）
nsh> dd if=/dev/zero of=/mnt/sdcard/bigfile.bin bs=1024 count=1024

# 读取大文件并验证
nsh> ls -la /mnt/sdcard/bigfile.bin
# 应显示 1048576 字节（1MB）

# 清理测试文件
nsh> rm /mnt/sdcard/test.txt
nsh> rm /mnt/sdcard/bigfile.bin
nsh> rmdir /mnt/sdcard/testdir
```

#### 4.3.3 SD 卡性能测试

```bash
# 写入速度测试
nsh> time dd if=/dev/zero of=/mnt/sdcard/speed_test.bin bs=4096 count=256

# 读取速度测试
nsh> time dd if=/mnt/sdcard/speed_test.bin of=/dev/null bs=4096

# 清理
nsh> rm /mnt/sdcard/speed_test.bin
```

### 4.4 MIPI-DSI 显示测试

ESP32-P4 EVB 支持 MIPI-DSI 接口的 LCD 显示屏，帧缓冲设备为 `/dev/fb0`。

**前提条件**：

```
CONFIG_ESP32P4_LCD_PANEL=y
CONFIG_ESP32P4_LCD_PANEL_ILI9881C=y  # 或 EK79007
CONFIG_ESP32P4_MIPI_DSI_LANES=2
CONFIG_ESP32P4_MIPI_DSI_LANE_RATE_MBPS=1000
CONFIG_ESP32P4_LCD_HRES=1024
CONFIG_ESP32P4_LCD_VRES=600
CONFIG_ESP32P4_LCD_BPP=16
CONFIG_ESP32P4_LCD_DPI_CLK_MHZ=60
```

#### 4.4.1 屏幕点亮测试

```bash
# 检查帧缓冲设备是否存在
nsh> ls /dev/fb0

# 查看帧缓冲信息
nsh> cat /proc/fb0

# 写入测试图案（白色全屏）
nsh> dd if=/dev/zero of=/dev/fb0 bs=1024 count=1200
# 对于 1024x600 RGB565 (16bpp)：
# 帧大小 = 1024 * 600 * 2 = 1,228,800 字节 ≈ 1200KB
```

#### 4.4.2 显示测试图案

**方法一：使用 fb-test 工具**

```bash
# 编译时启用 framebuffer 测试应用
CONFIG_EXAMPLES_FB=y

# 运行 framebuffer 测试
nsh> fb
# 会显示彩色条纹测试图案
```

**方法二：编写自定义显示测试**

```c
/* lcd_test.c - 显示彩色条纹 */
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#define LCD_WIDTH  1024
#define LCD_HEIGHT 600
#define BPP        2  /* RGB565 */

int main(void)
{
    int fd;
    uint16_t line[LCD_WIDTH];
    int x, y;

    fd = open("/dev/fb0", O_WRONLY);
    if (fd < 0)
    {
        perror("Failed to open /dev/fb0");
        return 1;
    }

    /* 绘制彩色条纹 */
    for (y = 0; y < LCD_HEIGHT; y++)
    {
        for (x = 0; x < LCD_WIDTH; x++)
        {
            /* RGB565: R5G6B5 */
            uint8_t r = (x * 32 / LCD_WIDTH) & 0x1F;
            uint8_t g = (y * 64 / LCD_HEIGHT) & 0x3F;
            uint8_t b = ((x + y) * 32 / (LCD_WIDTH + LCD_HEIGHT)) & 0x1F;
            line[x] = (r << 11) | (g << 5) | b;
        }
        write(fd, line, sizeof(line));
    }

    close(fd);
    printf("Display test pattern written to /dev/fb0\n");
    return 0;
}
```

### 4.5 BLE 蓝牙测试

ESP32-P4 需要外部 BLE 协处理器（如 ESP32-C6）来实现蓝牙功能。BLE 功能通过 SPI 或 UART 接口与协处理器通信。

**前提条件**：

```
CONFIG_ESP32P4_BLE=y
```

#### 4.5.1 BLE 广播扫描测试

```bash
# 检查 BLE 设备是否初始化
nsh> ls /dev/ble*

# 如果 BLE 驱动已注册，使用 BLE 工具进行扫描
# 注意：需要外部 BLE 协处理器硬件支持

# 编译时启用 BLE 测试应用
CONFIG_EXAMPLES_BLE=y

# 运行 BLE 扫描
nsh> ble_scan
# 应列出附近的 BLE 广播设备
```

> **注意**：BLE 功能需要硬件支持外部 BLE 协处理器，并且需要在 Kconfig 中正确配置 SPI/UART 通信接口。

### 4.6 Timer 定时器测试

ESP32-P4 提供 HP Timer Group，基于 80MHz APB 时钟。

**Timer 寄存器信息**：

| 项目 | 值 |
|------|-----|
| 基础时钟 | 80 MHz (APB clock) |
| Timer Group | TIMG0, TIMG1 |
| 分辨率 | 12.5 ns |
| 预分频范围 | 1 - 65536 |

**前提条件**：

```
CONFIG_ESP32P4_TIMER=y
```

#### 4.6.1 定时中断测试

```bash
# 使用 NuttX 的 timer 工具进行测试
# 编译时启用 timer 测试应用
CONFIG_EXAMPLES_TIMERDRIVER=y

# 运行 timer 测试
nsh> timer
# 应显示定时中断触发信息
```

#### 4.6.2 精度测试

```c
/* timer_accuracy.c - 定时器精度测试 */
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <nuttx/timers/timer.h>
#include <time.h>

int main(void)
{
    int fd;
    struct timer_status_s status;
    struct timespec start, end;
    int count = 100;

    fd = open("/dev/timer0", O_RDONLY);
    if (fd < 0)
    {
        perror("Failed to open timer");
        return 1;
    }

    /* 设置定时器周期为 10ms (100Hz) */
    ioctl(fd, TCIOC_SETTIMEOUT, 10000);

    /* 启动定时器 */
    ioctl(fd, TCIOC_START, 0);

    clock_gettime(CLOCK_MONOTONIC, &start);

    /* 等待 100 次中断 */
    for (int i = 0; i < count; i++)
    {
        read(fd, &status, sizeof(status));
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    /* 计算实际时间 */
    long long elapsed_ns = (end.tv_sec - start.tv_sec) * 1000000000LL +
                           (end.tv_nsec - start.tv_nsec);

    printf("Timer test: %d interrupts in %lld ms\n",
           count, elapsed_ns / 1000000);
    printf("Expected: %d ms, Actual: %lld ms\n",
           count * 10, elapsed_ns / 1000000);
    printf("Accuracy: %.2f%%\n",
           100.0 * (count * 10000000LL) / elapsed_ns);

    ioctl(fd, TCIOC_STOP, 0);
    close(fd);
    return 0;
}
```

### 4.7 IRQ 中断测试

ESP32-P4 使用 PLIC (Platform-Level Interrupt Controller) 管理中断。

**前提条件**：

```
CONFIG_ESP32P4_GPIO=y
```

#### 4.7.1 中断注册和触发测试

```bash
# 查看中断统计信息
nsh> cat /proc/irqs

# 使用 GPIO 按键中断进行测试
# 按下 BOOT 按钮，观察中断计数增加
nsh> while true; do cat /proc/irqs | head -20; sleep 1; done
```

**编写中断测试程序**：

```c
/* irq_test.c - GPIO 中断注册测试 */
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <poll.h>
#include <unistd.h>

volatile int irq_count = 0;

void irq_handler(int signo)
{
    irq_count++;
}

int main(void)
{
    int fd;
    struct pollfd pfd;
    int ret;

    fd = open("/dev/gpio21", O_RDONLY);
    if (fd < 0)
    {
        perror("Failed to open GPIO 21");
        return 1;
    }

    pfd.fd = fd;
    pfd.events = POLLPRI | POLLERR;

    printf("Press BOOT button to trigger GPIO interrupt\n");
    printf("Interrupt count: ");

    while (irq_count < 10)
    {
        ret = poll(&pfd, 1, 5000);  /* 5 秒超时 */
        if (ret > 0)
        {
            if (pfd.revents & POLLPRI)
            {
                irq_count++;
                printf("%d ", irq_count);
                fflush(stdout);
            }
        }
        else if (ret == 0)
        {
            printf("\nTimeout waiting for interrupt\n");
            break;
        }
    }

    printf("\nTotal interrupts: %d\n", irq_count);
    close(fd);
    return 0;
}
```

### 4.8 Clock 时钟测试

ESP32-P4 的时钟配置：

| 时钟 | 频率 | 说明 |
|------|------|------|
| XTAL | 40 MHz | 外部晶振 |
| PLL | 480 MHz | PLL 输出 |
| CPU | 400 MHz | CPU 主频 |
| APB | 80 MHz | 外设总线时钟 |

**前提条件**：

```
CONFIG_ESP32P4_CLOCK=y
```

#### 4.8.1 时钟频率验证

```bash
# 查看时钟配置
nsh> cat /proc/clocks

# 使用 timer 验证时钟精度
# 如果 APB 时钟正确（80MHz），1ms 定时器应精确触发
nsh> timer
```

**编写时钟测试程序**：

```c
/* clock_test.c - 时钟频率验证 */
#include <stdio.h>
#include <time.h>
#include <unistd.h>

int main(void)
{
    struct timespec start, end;
    long long elapsed_ns;
    int iterations = 1000;

    printf("Clock frequency test\n");
    printf("Measuring usleep accuracy...\n");

    /* 测试 usleep 精度 */
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < iterations; i++)
    {
        usleep(1000);  /* 1ms */
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    elapsed_ns = (end.tv_sec - start.tv_sec) * 1000000000LL +
                 (end.tv_nsec - start.tv_nsec);

    printf("Expected: %d ms\n", iterations);
    printf("Actual: %lld ms\n", elapsed_ns / 1000000);
    printf("Drift: %.2f%%\n",
           100.0 * (elapsed_ns - iterations * 1000000LL) /
           (iterations * 1000000LL));

    return 0;
}
```

### 4.9 PSRAM 测试

ESP32-P4 EVB 配备 32MB 外部 OPI PSRAM，用于扩展内存。

**内存布局**：

| 区域 | 地址 | 大小 | 说明 |
|------|------|------|------|
| 内部 SRAM | 0x4FF00000 | 768 KB | 高速内部存储 |
| PSRAM | 0x48000000 | 32 MB | 外部 OPI PSRAM |

**前提条件**：

```
CONFIG_ESP32P4_PSRAM=y
CONFIG_ESP32P4_PSRAM_SIZE=32
CONFIG_MM_REGIONS=2
```

#### 4.9.1 内存分配测试

```bash
# 查看内存信息
nsh> free
# 应显示两个内存区域：内部 SRAM 和 PSRAM

# 查看详细内存信息
nsh> cat /proc/mm
```

#### 4.9.2 PSRAM 访问测试

```c
/* psram_test.c - PSRAM 分配和访问测试 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define TEST_SIZE (4 * 1024 * 1024)  /* 4MB */

int main(void)
{
    uint8_t *buffer;
    struct timespec start, end;
    long long elapsed_ns;

    printf("PSRAM test: Allocating %d bytes...\n", TEST_SIZE);

    /* 分配大块内存（应分配到 PSRAM） */
    buffer = malloc(TEST_SIZE);
    if (buffer == NULL)
    {
        printf("ERROR: Failed to allocate %d bytes\n", TEST_SIZE);
        return 1;
    }

    printf("Allocated at %p\n", buffer);

    /* 写入测试 */
    clock_gettime(CLOCK_MONOTONIC, &start);
    memset(buffer, 0xAA, TEST_SIZE);
    clock_gettime(CLOCK_MONOTONIC, &end);

    elapsed_ns = (end.tv_sec - start.tv_sec) * 1000000000LL +
                 (end.tv_nsec - start.tv_nsec);
    printf("Write speed: %lld MB/s\n",
           (long long)TEST_SIZE * 1000 / elapsed_ns);

    /* 读取验证 */
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < TEST_SIZE; i++)
    {
        if (buffer[i] != 0xAA)
        {
            printf("ERROR: Data mismatch at offset %d: "
                   "expected 0xAA, got 0x%02X\n", i, buffer[i]);
            free(buffer);
            return 1;
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &end);

    elapsed_ns = (end.tv_sec - start.tv_sec) * 1000000000LL +
                 (end.tv_nsec - start.tv_nsec);
    printf("Read speed: %lld MB/s\n",
           (long long)TEST_SIZE * 1000 / elapsed_ns);

    /* 图案测试 */
    printf("Pattern test...\n");
    for (int i = 0; i < TEST_SIZE; i++)
    {
        buffer[i] = (uint8_t)(i & 0xFF);
    }
    for (int i = 0; i < TEST_SIZE; i++)
    {
        if (buffer[i] != (uint8_t)(i & 0xFF))
        {
            printf("ERROR: Pattern mismatch at offset %d\n", i);
            free(buffer);
            return 1;
        }
    }

    printf("PSRAM test PASSED\n");
    free(buffer);
    return 0;
}
```

---

## 5. 常见问题排查

### 5.1 烧录失败

#### 问题：串口权限不足

**现象**：

```
serial.serialutil.SerialException: could not open port /dev/ttyACM0: [Errno 13] Permission denied
```

**解决办法**：

```bash
# 方法一：临时解决
sudo chmod 666 /dev/ttyACM0

# 方法二：永久解决（推荐）
sudo usermod -a -G dialout $USER
# 然后重新登录

# 方法三：配置 udev 规则（见 1.3 节）
```

#### 问题：无法进入下载模式

**现象**：

```
A fatal error occurred: Failed to connect to Espressif device: Wrong boot mode detected (0xXX)
```

**解决办法**：

1. 确认已按住 BOOT 按钮后按 RST 进入下载模式
2. 检查 USB 线是否支持数据传输（非仅充电线）
3. 降低波特率重试：
   ```bash
   esptool.py --chip esp32p4 --port /dev/ttyACM0 --baud 115200 \
       write_flash 0x0 nuttx.bin
   ```
4. 检查串口设备是否正确：
   ```bash
   ls -la /dev/ttyACM* /dev/ttyUSB*
   dmesg | tail -20
   ```

#### 问题：Flash 写入失败

**现象**：

```
A fatal error occurred: Failed to write to target Flash
```

**解决办法**：

1. 擦除 Flash 后重试：
   ```bash
   esptool.py --chip esp32p4 --port /dev/ttyACM0 erase_flash
   esptool.py --chip esp32p4 --port /dev/ttyACM0 --baud 115200 \
       write_flash 0x0 nuttx.bin
   ```
2. 检查固件文件是否完整：
   ```bash
   ls -la nuttx.bin
   file nuttx.bin
   ```
3. 尝试指定 Flash 模式：
   ```bash
   esptool.py --chip esp32p4 --port /dev/ttyACM0 --flash_mode dout \
       write_flash 0x0 nuttx.bin
   ```

#### 问题：USB 设备未识别

**现象**：`/dev/ttyACM0` 和 `/dev/ttyUSB0` 均不存在。

**解决办法**：

```bash
# 检查 USB 设备是否被识别
lsusb
# 应看到 Espressif 相关设备

# 检查内核日志
dmesg | grep -i usb
dmesg | grep -i tty

# 检查 USB 线缆（换一根试试）
# 检查 USB 端口（换一个端口试试）

# 如果是 USB-JTAG 设备未识别，可能需要安装驱动
sudo apt install linux-modules-extra-$(uname -r)
```

### 5.2 启动失败

#### 问题：串口无输出

**现象**：烧录成功后，串口终端无任何输出。

**排查步骤**：

1. **确认串口配置正确**：
   ```bash
   # 确认波特率 115200，8N1
   minicom -D /dev/ttyACM0 -b 115200
   ```

2. **确认固件已正确烧录**：
   ```bash
   # 验证 Flash 内容
   esptool.py --chip esp32p4 --port /dev/ttyACM0 \
       verify_flash 0x0 nuttx.bin
   ```

3. **手动复位芯片**：
   - 按 RST 按钮复位
   - 或在串口终端按 `Ctrl-T Ctrl-R`

4. **检查是否为 UART 配置问题**：
   - 确认 defconfig 中启用了 UART0 作为串口控制台：
     ```
     CONFIG_ESP32P4_UART0=y
     CONFIG_ESP32P4_UART0_SERIAL_CONSOLE=y
     ```

5. **尝试其他串口工具**：
   ```bash
   # 尝试不同的串口工具
   screen /dev/ttyACM0 115200
   picocom -b 115200 /dev/ttyACM0
   python3 -m serial.tools.miniterm /dev/ttyACM0 115200
   ```

#### 问题：Panic 循环

**现象**：系统启动后反复输出 panic 信息并重启。

**排查步骤**：

1. **捕获 panic 信息**：
   ```bash
   # 使用 idf.py monitor 自动解析 backtrace
   idf.py -p /dev/ttyACM0 monitor
   ```

2. **手动解析 backtrace**：
   ```bash
   # 从串口输出中复制 backtrace 地址
   riscv-none-elf-addr2line -pfiaC -e nuttx 0x4ff01234 0x4ff01230
   ```

3. **常见原因**：
   - 内存不足（PSRAM 未正确初始化）
   - 时钟配置错误
   - GPIO 冲突（Flash 引脚被占用）
   - 中断处理异常

4. **临时禁用看门狗以获取更多信息**：
   ```
   CONFIG_ESP32P4_WDT=n
   ```

#### 问题：启动后卡死

**现象**：串口输出部分启动信息后卡死。

**排查步骤**：

1. **检查是否为内存问题**：
   ```bash
   # 减小堆栈大小以适应内部 SRAM
   CONFIG_IDLETHREAD_STACKSIZE=1024
   CONFIG_INIT_STACKSIZE=2048
   ```

2. **检查是否为驱动初始化卡死**：
   - 逐个禁用外设驱动，定位问题驱动
   - 从最小 NSH 配置开始：
     ```bash
     ./tools/configure.sh evb_esp32p4:nsh
     make -j$(nproc)
     ```

### 5.3 驱动不工作

#### 问题：GPIO 未配置

**现象**：GPIO 操作无效果，LED 不亮或按键无响应。

**排查步骤**：

1. **确认 GPIO 驱动已启用**：
   ```
   CONFIG_ESP32P4_GPIO=y
   CONFIG_DEV_GPIO=y
   ```

2. **确认 GPIO 引脚未被其他功能占用**：
   - GPIO 0-7 通常用于 SPI Flash，不可用作通用 GPIO
   - 检查 `board.h` 中的 GPIO 定义

3. **检查 GPIO 方向配置**：
   ```bash
   # 确认 LED GPIO 配置为输出
   # 确认 BUTTON GPIO 配置为输入
   ```

4. **使用寄存器直接操作验证**：
   ```bash
   # 读取 GPIO 寄存器状态
   nsh> devmem 0x60091008 32  # GPIO_OUT_W1TS_REG
   ```

#### 问题：时钟未使能

**现象**：外设驱动初始化超时或无响应。

**排查步骤**：

1. **确认时钟配置**：
   ```bash
   nsh> cat /proc/clocks
   ```

2. **检查外设时钟使能**：
   - 确认相关外设的时钟门控已打开
   - 检查 `esp32p4_clockconfig.c` 中的配置

3. **常见时钟问题**：
   - APB 时钟频率不正确（应为 80MHz）
   - PLL 未锁定
   - 外设时钟分频器配置错误

#### 问题：SD 卡无法挂载

**现象**：SD 卡插入后无法识别或挂载失败。

**排查步骤**：

1. **检查 SD 卡硬件**：
   - 确认 SD 卡已正确插入卡槽
   - 尝试更换 SD 卡
   - 确认 SD 卡已格式化为 FAT32

2. **检查驱动配置**：
   ```
   CONFIG_ESP32P4_SDMMC=y
   CONFIG_ESP32P4_SDMMC_DMA=y
   CONFIG_ESP32P4_EVB_SDCARD=y
   CONFIG_MMCSD=y
   CONFIG_MMCSD_SDIO=y
   CONFIG_SDMMC=y
   CONFIG_FS_FAT=y
   ```

3. **查看启动日志**：
   ```bash
   # SD 卡初始化信息应在启动日志中
   # 搜索 "SD card" 相关日志
   nsh> dmesg | grep -i sd
   ```

4. **手动挂载测试**：
   ```bash
   nsh> ls /dev/mmcsd*
   nsh> mount -t vfat /dev/mmcsd0 /mnt/sdcard
   ```

#### 问题：MIPI-DSI 显示异常

**现象**：屏幕不亮或显示异常。

**排查步骤**：

1. **检查 LCD 配置**：
   ```
   CONFIG_ESP32P4_LCD_PANEL=y
   CONFIG_ESP32P4_LCD_PANEL_ILI9881C=y
   CONFIG_ESP32P4_MIPI_DSI_LANES=2
   CONFIG_ESP32P4_MIPI_DSI_LANE_RATE_MBPS=1000
   ```

2. **确认帧缓冲设备**：
   ```bash
   nsh> ls /dev/fb0
   nsh> cat /proc/fb0
   ```

3. **检查视频时序参数**：
   - 确认分辨率、刷新率与屏幕规格匹配
   - 检查 HSYNC/VSYNC/HBP/HFP/VBP/VFP 参数

4. **检查 MIPI PHY 电源**：
   - 确认 LDO 电源已使能
   - 检查 GPIO 配置（复位引脚）

---

## 附录

### A. 快速参考卡片

**编译**：

```bash
cd /home/geo/openvela
./tools/configure.sh evb_esp32p4:nsh
make -j$(nproc)
```

**烧录**：

```bash
esptool.py --chip esp32p4 --port /dev/ttyACM0 --baud 460800 \
    write_flash 0x0 nuttx.bin
```

**串口监控**：

```bash
minicom -D /dev/ttyACM0 -b 115200
# 或
idf.py -p /dev/ttyACM0 monitor
```

**手动进入下载模式**：按住 BOOT -> 按 RST -> 松开 BOOT

### B. ESP32-P4 EVB GPIO 引脚速查表

| GPIO | 功能 | 说明 |
|------|------|------|
| 0 | SPI Flash | 不可用作通用 GPIO |
| 21 | BOOT 按钮 | 输入，内部上拉，低电平有效 |
| 26 | 板载 LED | 输出，高电平点亮 |
| 39-42 | SDMMC D0-D3 | SD 卡数据线 |
| 43 | SDMMC CLK | SD 卡时钟 |
| 44 | SDMMC CMD | SD 卡命令线 |

### C. 常用 NSH 命令

```bash
nsh> help              # 查看所有可用命令
nsh> free              # 查看内存使用情况
nsh> ps                # 查看进程列表
nsh> ls /dev/          # 列出所有设备
nsh> cat /proc/mm      # 查看内存详情
nsh> cat /proc/irqs    # 查看中断统计
nsh> reboot            # 重启系统
nsh> uname -a          # 查看系统信息
```

### D. 相关文档

- [ESP32-P4 开发环境搭建指南](environment-setup-guide.md)
- ESP32-P4 技术参考手册：https://www.espressif.com/zh-hans/products/socs/esp32-p4
- NuttX 官方文档：https://nuttx.apache.org/docs/
- ESP-IDF 编程指南：https://docs.espressif.com/projects/esp-idf/

---

*文档维护者：geo@openvela | 最后更新：2026-08-25*
