# openvela Porting and Driver Development Guide

> Comprehensive reference for porting openvela to new hardware platforms and developing device drivers.
>
> Last updated: 2026-08-26

---

## Table of Contents

1. [Architecture Overview](#1-architecture-overview)
2. [New Platform Porting Guide](#2-new-platform-porting-guide)
3. [Driver Development Framework](#3-driver-development-framework)
4. [Build System Integration](#4-build-system-integration)
5. [Configuration System (Kconfig)](#5-configuration-system-kconfig)
6. [ESP32-P4 Adaptation Status and Tasks](#6-esp32-p4-adaptation-status-and-tasks)
7. [Development Environment Setup](#7-development-environment-setup)
8. [Testing and Debugging](#8-testing-and-debugging)
9. [References](#9-references)

---

## 1. Architecture Overview

### 1.1 What is openvela

openvela is an open-source RTOS based on Apache NuttX, designed for IoT and embedded devices. It provides a POSIX-compatible real-time operating system with a rich set of device drivers and middleware components.

### 1.2 Three-Layer Architecture

openvela follows a three-layer hardware abstraction architecture:

```
+-------------------------------------------------------------+
|                    Application Layer                         |
|          (NuttShell, apps, user programs)                    |
+-------------------------------------------------------------+
|                    OS Kernel Layer                           |
|     (scheduler, memory management, IPC, file system)         |
+-------------------------------------------------------------+
|              Hardware Abstraction Layer                      |
|  +-------------------------------------------------------+  |
|  |  Architecture Layer (arch)                             |  |
|  |  CPU architecture definitions (ARMv7-M, RISC-V, etc.) |  |
|  |  Usually requires NO modification                      |  |
|  +-------------------------------------------------------+  |
|  |  Chip/SoC Layer (chip)                                 |  |
|  |  Extensions based on architecture                      |  |
|  |  Interrupt controller, clock, GPIO, peripherals        |  |
|  +-------------------------------------------------------+  |
|  |  Board Layer (board)                                   |  |
|  |  Connects peripherals to form a development board      |  |
|  |  Pin definitions, board drivers, hardware init          |  |
|  +-------------------------------------------------------+  |
+-------------------------------------------------------------+
```

**Layer Responsibilities:**

| Layer | Directory | Responsibility |
|-------|-----------|----------------|
| Architecture | `nuttx/arch/<arch>/` | CPU architecture (ARM, RISC-V, Xtensa). Provides exception handling, context switching, cache management. Typically requires no modification. |
| Chip/SoC | `vendor/<vendor>/chips/<chip>/` | SoC-specific code. Interrupt controller, clock tree, UART, GPIO, timer, DMA, peripheral drivers. |
| Board | `vendor/<vendor>/boards/<arch>/<chip>/<board>/` | Board-level integration. Pin mux, board init, defconfig, linker script, device registration. |

### 1.3 Supported Architectures

| Architecture | Examples | Toolchain |
|--------------|----------|-----------|
| ARM Cortex-M | STM32F4/F7/H7, nRF52, RP2040 | `gcc-arm-none-eabi` |
| ARM Cortex-A | QEMU ARM64, QEMU ARM32 | `gcc-aarch64-linux-gnu` |
| RISC-V 32 | ESP32-C3, ESP32-C6, ESP32-P4 | `gcc-riscv32-esp-elf` |
| RISC-V 64 | QEMU RISC-V 64 | `gcc-riscv64-unknown-elf` |
| Xtensa | ESP32, ESP32-S2, ESP32-S3 | `gcc-xtensa-esp32-elf` |

### 1.4 Build Artifacts

The build system produces three key artifacts:

| Artifact | Description |
|----------|-------------|
| `libarch.a` | Architecture-layer static library |
| `libboards.a` | Board-layer code library |
| `vela_nuttx.bin` (or `nuttx.bin`) | Final binary image for flashing |

---

## 2. New Platform Porting Guide

### 2.1 Porting Flow Overview

```
Step 1: Familiarize with code structure (vendor directory layout)
    |
Step 2: Configure Kconfig files (define build options and module dependencies)
    |
Step 3: Write Makefiles (toolchain compilation rules)
    |
Step 4: Implement chip-layer and board-layer code (reference vendor_template)
    |
Step 5: Compile and test (generate libarch.a, libboards.a, vela_nuttx.bin)
```

### 2.2 Required Files for Chip Layer

The chip layer code resides in `vendor/<vendor_name>/chips/<chip_name>/`. The following files must be implemented:

| File | Purpose | Priority |
|------|---------|----------|
| `<chip>_start.c` | Boot entry point. Clear BSS, copy .data, init clock/UART/stack, call `nx_start()` | P0 |
| `<chip>_irq.c` | Interrupt controller driver (PLIC/CLIC for RISC-V, NVIC for ARM) | P0 |
| `<chip>_clockconfig.c` | PLL and clock tree configuration | P0 |
| `<chip>_serial.c` | UART driver (NuttX serial lower-half) | P0 |
| `<chip>_timerisr.c` | System tick timer for OS scheduling | P0 |
| `<chip>_allocateheap.c` | Dynamic memory allocation (heap regions) | P0 |
| `<chip>_gpio.c` | GPIO driver | P0 |
| `hardware/<chip>_soc.h` | SoC base addresses, memory map, register access macros | P0 |
| `hardware/<chip>_uart.h` | UART register definitions | P0 |
| `hardware/<chip>_plic.h` | Interrupt controller register definitions | P0 |
| `Kconfig` | Chip-level configuration options | P0 |
| `Make.defs` | Make build rules | P0 |
| `CMakeLists.txt` | CMake build rules | P0 |

### 2.3 Required Files for Board Layer

The board layer resides in `vendor/<vendor>/boards/<arch>/<chip>/<board>/`:

| File | Purpose |
|------|---------|
| `configs/default/defconfig` | Default configuration (pointers to custom chip/board dirs) |
| `src/<board>_bringup.c` | Board-level initialization (register GPIO/SPI/I2C drivers) |
| `src/<board>_boot.c` | Early board init (IO mux, power domain, console verification) |
| `src/<board>.h` | Board-private header (LED/button macros, function declarations) |
| `src/Make.defs` | Source file build rules |
| `include/board.h` | Board hardware definitions (LED GPIO, button GPIO, clock frequencies) |
| `scripts/ld.script` | Linker script (memory layout: Flash, SRAM, PSRAM) |
| `scripts/Make.defs` | Script rules (linker script path, CFLAGS) |
| `Kconfig` | Board-level configuration options |

### 2.4 Boot Sequence

The standard NuttX boot sequence is:

```
__start (assembly, arch-specific)
  -> <chip>_start() (C entry point)
    -> clock_init()           # Configure PLL, CPU frequency
    -> cache_init()           # Enable L1 I-Cache/D-Cache
    -> lowsetup()             # Early UART for debug output
    -> clear BSS segment
    -> copy .data segment
    -> nx_start()             # NuttX kernel entry (does not return)
      -> board_early_initialize()
        -> <board>_board_initialize()
          -> board_periph_init()    # IO mux, power domains
          -> board_console_init()   # Console verification
          -> board_gpio_init()      # LED/button GPIO setup
      -> board_late_initialize()
        -> <board>_bringup()        # Register device drivers
          -> gpio_init()
          -> spi_init()
          -> i2c_init()
      -> start user tasks (NSH shell)
```

### 2.5 Chip-Layer Implementation Details

#### 2.5.1 SoC Register Header

Define all peripheral base addresses and register access macros:

```c
/* Peripheral base addresses (from Technical Reference Manual) */
#define CHIP_PERIP_BASE       0x50000000
#define CHIP_UART0_BASE       (CHIP_PERIP_BASE + 0x0000)
#define CHIP_SPI0_BASE        (CHIP_PERIP_BASE + 0x3000)
#define CHIP_I2C0_BASE        (CHIP_PERIP_BASE + 0x6000)
#define CHIP_GPIO_BASE        (CHIP_PERIP_BASE + 0x8000)
#define CHIP_TIMER0_BASE      (CHIP_PERIP_BASE + 0x9000)

/* Interrupt numbers */
#define CHIP_IRQ_UART0        10
#define CHIP_IRQ_GPIO         25
#define CHIP_IRQ_TIMER0       35

/* Memory regions */
#define CHIP_SRAM_BASE        0x4FF00000
#define CHIP_SRAM_SIZE        0x000C0000  /* 768 KB */

/* Register access functions */
static inline uint32_t chip_getreg(uintptr_t addr)
{
  return *(volatile uint32_t *)addr;
}

static inline void chip_putreg(uint32_t val, uintptr_t addr)
{
  *(volatile uint32_t *)addr = val;
}
```

#### 2.5.2 Interrupt Controller Driver

For RISC-V platforms using PLIC:

```c
void up_irqinitialize(void)
{
  /* Disable all interrupts */
  for (i = 0; i < NIRQS / 32; i++)
    chip_putreg(0, PLIC_ENABLE + i * 4);

  /* Set default priority for all sources */
  for (i = 1; i < NIRQS; i++)
    chip_putreg(1, PLIC_PRIORITY(i));

  /* Set threshold to 0 (allow all priorities) */
  chip_putreg(0, PLIC_THRESHOLD);

  /* Attach common RISC-V interrupt handler */
  riscv_exception_attach();
}

void up_enable_irq(int irq)
{
  /* Set priority and enable in PLIC */
}

void up_disable_irq(int irq)
{
  /* Disable in PLIC */
}
```

#### 2.5.3 UART Driver (Serial Lower-Half)

Follow the NuttX serial upper-half/lower-half pattern. Implement `uart_ops_s` callbacks:

```c
static const struct uart_ops_s g_uart_ops =
{
  .setup       = chip_uart_setup,       /* Configure baud rate, data format */
  .shutdown    = chip_uart_shutdown,     /* Disable UART peripheral */
  .attach      = chip_uart_attach,       /* Attach interrupt handler */
  .detach      = chip_uart_detach,       /* Detach interrupt handler */
  .ioctl       = chip_uart_ioctl,        /* Additional control commands */
  .receive     = chip_uart_receive,      /* Receive data from FIFO */
  .rxint       = chip_uart_rxint,        /* Enable/disable RX interrupt */
  .rxavailable = chip_uart_rxavailable,  /* Check if RX data available */
  .send        = chip_uart_send,         /* Send data to FIFO */
  .txint       = chip_uart_txint,        /* Enable/disable TX interrupt */
  .txready     = chip_uart_txready,      /* Check if TX FIFO not full */
  .txempty     = chip_uart_txempty,      /* Check if TX FIFO empty */
};

/* Register UART device */
int chip_uart_register(int id)
{
  snprintf(devpath, sizeof(devpath), "/dev/ttyS%d", id);
  return uart_register(devpath, dev);
}
```

#### 2.5.4 System Tick Timer

Configure a hardware timer to generate periodic interrupts for OS scheduling:

```c
void up_timer_initialize(void)
{
  /* Calculate load value: timer_clock / ticks_per_second */
  load_value = APB_CLOCK_FREQ / TICK_PER_SEC;

  /* Configure timer: load value, auto-reload, enable */
  putreg32(load_value, TIMER_LOAD_REG(base));
  putreg32(TIMER_CTRL_AUTO_RELOAD | TIMER_CTRL_ENABLE,
           TIMER_CTRL_REG(base));

  /* Enable timer interrupt */
  up_enable_irq(CHIP_IRQ_TIMER0);
}

static int chip_timer_interrupt(int irq, void *context, void *arg)
{
  putreg32(1, TIMER_INT_CLR_REG(base));  /* Clear interrupt */
  nxsched_process_timer();                /* Notify NuttX of one tick */
  return OK;
}
```

#### 2.5.5 Heap Memory Allocation

Configure memory regions for dynamic allocation:

```c
void up_allocate_heap(void **heap_start, size_t *heap_size)
{
  /* Primary heap: from end of BSS to end of SRAM */
  *heap_start = (void *)&_eheap;
  *heap_size  = (SRAM_BASE + SRAM_SIZE) - (uintptr_t)&_eheap;
}

#ifdef CONFIG_CHIP_PSRAM
void up_addregion(void)
{
  /* Secondary heap: PSRAM for large allocations */
  mm_addregion(g_kmmheap, (void *)PSRAM_BASE, PSRAM_SIZE);
}
#endif
```

### 2.6 Board-Layer Implementation Details

#### 2.6.1 defconfig with Custom Directories

When using vendor directories outside the NuttX tree, use custom directory pointers:

```
CONFIG_ARCH="risc-v"
CONFIG_ARCH_CHIP="esp32p4"
CONFIG_ARCH_CHIP_ESP32P4=y
CONFIG_ARCH_BOARD_CUSTOM_DIR="../vendor_esp32p4/boards/risc-v/esp32p4/esp32p4-evb"
CONFIG_ARCH_CHIP_CUSTOM_DIR="../vendor_esp32p4/chips/esp32p4"
```

#### 2.6.2 Linker Script

Define memory layout matching the target hardware:

```
MEMORY
{
    flash (rx)  : ORIGIN = 0x42000000, LENGTH = 16M
    sram  (rwx) : ORIGIN = 0x4FF00000, LENGTH = 768K
}

SECTIONS
{
    .text : { *(.text .text*) } > flash
    .rodata : { *(.rodata .rodata*) } > flash
    .data : { *(.data .data*) } > sram AT > flash
    .bss : { *(.bss .bss*) } > sram
}
```

- **Flash** (XIP): .text and .rodata execute in place from Flash
- **SRAM**: .data and .bss reside in SRAM
- **PSRAM**: Managed dynamically by chip-layer heap allocator

---

## 3. Driver Development Framework

### 3.1 openvela Driver Model

openvela uses a simplified driver model compared to Linux:

| Feature | openvela | Linux |
|---------|----------|-------|
| Registration | `register_driver()` / `register_blockdriver()` | match/probe mechanism |
| Device numbers | None (path-based) | Major/minor numbers |
| Module init | Explicit call in board code | `module_init` |
| VFS interface | `file_operations` struct | `file_operations` struct |
| User access | Standard system calls (open/read/write/ioctl) | Standard system calls |

### 3.2 Driver Types

| Type | Examples | Registration |
|------|----------|--------------|
| Character devices | zero, null, sensor, ADC, GPIO | `register_driver()` |
| Block devices | eMMC, SD card, BCH | `register_blockdriver()` |
| Special devices | MTD, PTP, timer, netdev | Framework-specific |

### 3.3 Driver Architecture

```
+------------------------------------------+
|          User Application                |
|    (open/read/write/ioctl/close)         |
+------------------------------------------+
|            VFS Layer                      |
|    (path lookup, inode management)        |
+------------------------------------------+
|        Upper Half (openvela provided)     |
|   (driver registration, syscall dispatch, |
|    operation set routing)                 |
+------------------------------------------+
|        Lower Half (driver developer)      |
|   (hardware interaction, bus operations,  |
|    core logic implementation)             |
+------------------------------------------+
|          Hardware                         |
+------------------------------------------+
```

### 3.4 Key Data Structures

#### inode Structure

```c
struct inode
{
  uint8_t          i_flags;      /* File type flags (driver file, etc.) */
  union inode_ops_u inode_ops_u; /* Operation function set */
  void            *i_private;    /* Driver private data */
};

/* Key macros */
INODE_IS_DRIVER(i)   /* Check if inode is a driver file */
INODE_SET_DRIVER(i)  /* Mark inode as a driver file */
```

#### file_operations Structure

```c
struct file_operations
{
  int     (*open)(struct file *filep);
  int     (*close)(struct file *filep);
  ssize_t (*read)(struct file *filep, char *buffer, size_t buflen);
  ssize_t (*write)(struct file *filep, const char *buffer, size_t buflen);
  off_t   (*seek)(struct file *filep, off_t offset, int whence);
  int     (*ioctl)(struct file *filep, int cmd, unsigned long arg);
  int     (*poll)(struct file *filep, struct pollfds *fds, bool setup);
};
```

### 3.5 Driver Directory Structure

| Location | Type | Description |
|----------|------|-------------|
| `nuttx/drivers/` | Shareable | Generic device drivers (common across platforms) |
| `nuttx/boards/<arch>/<chip>/<board>/src/` | Custom | Board-specific drivers |

### 3.6 Driver Registration

#### Character Device

```c
static const struct file_operations g_mydriver_ops =
{
  .open  = mydriver_open,
  .close = mydriver_close,
  .read  = mydriver_read,
  .write = mydriver_write,
  .ioctl = mydriver_ioctl,
};

int mydriver_register(void)
{
  return register_driver("/dev/mydevice", &g_mydriver_ops, 0666, NULL);
}
```

#### Block Device

```c
int myblock_register(void)
{
  return register_blockdriver("/dev/myblock", &g_myblock_ops, 0666, NULL);
}
```

### 3.7 Non-Standard Interface: boardctl

Used for application-level board control via ioctl:

| Function | Purpose |
|----------|---------|
| `board_app_init` | Board initialization at app start |
| `board_app_finalinit` | Final board initialization |
| `board_poweroff` | System power off |
| `board_pmctl` | Power management control |
| `board_reset` | System reset |

### 3.8 Common Driver Patterns

#### SPI Bus Driver

Follow the NuttX SPI bus framework (`struct spi_dev_s`):

```c
static const struct spi_ops_s g_spi_ops =
{
  .lock          = spi_lock,
  .select        = spi_select,
  .setfrequency  = spi_setfrequency,
  .setmode       = spi_setmode,
  .setbits       = spi_setbits,
  .exchange      = spi_exchange,
  .send          = spi_send,
  .recvblock     = spi_recvblock,
  .status        = spi_status,
};

FAR struct spi_dev_s *chip_spibus_initialize(int bus)
{
  /* Allocate SPI device, configure GPIO, enable clock */
  return spi_dev;
}
```

#### GPIO Driver

```c
static int gpio_read(struct gpio_dev_s *dev, bool *value)
{
  int pin = (int)(intptr_t)dev->priv;
  uint32_t regval = getreg32(GPIO_IN_REG);
  *value = (regval >> pin) & 1;
  return OK;
}

static int gpio_write(struct gpio_dev_s *dev, bool value)
{
  int pin = (int)(intptr_t)dev->priv;
  if (value)
    modifyreg32(GPIO_OUT_REG, 0, 1 << pin);
  else
    modifyreg32(GPIO_OUT_REG, 1 << pin, 0);
  return OK;
}
```

#### Framebuffer Driver

Use NuttX fb framework (`fb_vtable_s`), register with `fb_register_device`:

```c
priv->vtable.getvideoinfo = dsi_getvideoinfo;
priv->vtable.getplaneinfo = dsi_getplaneinfo;
priv->vtable.pandisplay   = dsi_pandisplay;
priv->vtable.setpower     = dsi_setpower;

return fb_register_device(display, 0, &priv->vtable);
```

#### Ethernet Driver

Use NuttX netdev framework (`netdev_lowerhalf_s`):

```c
static const struct netdev_ops_s g_emac_ops =
{
  .ifup     = emac_ifup,
  .ifdown   = emac_ifdown,
  .transmit = emac_transmit,
  .receive  = emac_receive,
};
```

Interrupt handling uses ISR + Work Queue pattern: ISR disables hardware interrupt and schedules work queue, work queue processes TX completion and RX reception, then re-enables hardware interrupt.

#### USB Device Controller Driver (DCD)

Key constraints for USB DCD implementation:

1. PHY must be initialized before Core init
2. SET_ADDRESS must be handled directly by the driver (not dispatched to class driver)
3. Must distinguish 2-stage and 3-stage control transfers
4. DMA buffers must be 4096-byte page-aligned
5. Must enable `CONFIG_USBDEV_DUALSPEED` (HS mode bulk maxpacket=512)

---

## 4. Build System Integration

### 4.1 Build System Overview

openvela supports both Make and CMake build systems. The build is configured through Kconfig and driven by Makefiles/CMakeLists.txt at each layer.

### 4.2 Chip-Layer Make.defs

```makefile
include $(TOPDIR)/Make.defs

# Always-compiled sources
CHIP_CSRCS  = chip_start.c
CHIP_CSRCS += chip_irq.c
CHIP_CSRCS += chip_clockconfig.c
CHIP_CSRCS += chip_allocateheap.c
CHIP_CSRCS += chip_serial.c
CHIP_CSRCS += chip_timerisr.c

# Conditionally compiled sources
ifeq ($(CONFIG_CHIP_GPIO),y)
CHIP_CSRCS += chip_gpio.c
endif

ifeq ($(CONFIG_CHIP_SPI0),y)
CHIP_CSRCS += chip_spi.c
endif

ifeq ($(CONFIG_CHIP_I2C0),y)
CHIP_CSRCS += chip_i2c.c
endif

ifeq ($(CONFIG_CHIP_PSRAM),y)
CHIP_CSRCS += chip_psram.c
endif

ifdef CONFIG_SMP
CHIP_CSRCS += chip_smp.c
endif

DEPPATH += --dep-path chip
VPATH += :chip
```

### 4.3 Chip-Layer CMakeLists.txt

```cmake
set(CHIP_CSRCS
    chip_start.c
    chip_irq.c
    chip_clockconfig.c
    chip_allocateheap.c
    chip_serial.c
    chip_timerisr.c
)

if(CONFIG_CHIP_GPIO)
  list(APPEND CHIP_CSRCS chip_gpio.c)
endif()

set(HW_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/hardware)
target_include_directories(nuttx PRIVATE ${HW_INCLUDE_DIR})
```

### 4.4 Board-Layer scripts/Make.defs

```makefile
include $(TOPDIR)/Make.defs

# Toolchain configuration
include $(CHIPDIR)/Toolchain.defs

# Linker script
ARCHSCRIPT = $(BOARD_DIR)/scripts/ld.script

# Compiler flags
CFLAGS += -pipe -Werror
CXXFLAGS += -pipe -Werror
AFLAGS += -pipe -Werror
```

### 4.5 Board-Layer src/Make.defs

```makefile
include $(TOPDIR)/Make.defs

CSRCS  = board_bringup.c
CSRCS += board_boot.c

ifeq ($(CONFIG_BOARD_SDCARD),y)
CSRCS += board_sdcard.c
endif

DEPPATH += --dep-path $(BOARD_SRCDIR)
VPATH += :$(BOARD_SRCDIR)
```

### 4.6 Build Commands

```bash
# Configure for a target
./tools/configure.sh <board>:<config>

# Build
make -j$(nproc)

# Or using build.sh
./build.sh vendor/<vendor>/boards/<arch>/<chip>/<board>/configs/<config> --cmake -j$(nproc)

# Clean build
./build.sh <config> --cmake --clean -j$(nproc)

# Run menuconfig
./build.sh <config> --cmake --menuconfig

# Code style check
nuttx/tools/checkpatch.sh -f <source-file>
```

### 4.7 Repo Manifest Integration

Board adaptation code is placed in a dedicated vendor repository, mapped into the openvela project via manifest `<linkfile>`:

```xml
<manifest>
  <project name="vendor_esp32p4" path="vendor_esp32p4">
    <linkfile src="chips/esp32p4" dest="nuttx/arch/risc-v/src/esp32p4" />
    <linkfile src="boards/risc-v/esp32p4" dest="nuttx/boards/risc-v/esp32p4" />
  </project>
</manifest>
```

During the contest, fork the dedicated repository, submit PR, self-review and merge. After winning, submit PR to the upstream openvela vendor repository.

---

## 5. Configuration System (Kconfig)

### 5.1 Kconfig Overview

openvela uses the Linux Kconfig system for build configuration. Configuration options are defined in `Kconfig` files at each layer and selected through `defconfig` files or `menuconfig`.

### 5.2 Configuration Hierarchy

```
nuttx/Kconfig                    # Top-level (includes arch and board Kconfigs)
  arch/risc-v/Kconfig            # Architecture-level
    arch/risc-v/src/<chip>/Kconfig  # Chip-level
  boards/<arch>/<chip>/<board>/Kconfig  # Board-level
```

### 5.3 defconfig Structure

A defconfig file contains the minimal set of configuration overrides:

```
# Architecture identification
CONFIG_ARCH="risc-v"
CONFIG_ARCH_CHIP="<chip>"
CONFIG_ARCH_CHIP_<UPPERCASE>=y
CONFIG_ARCH_BOARD="<board>"
CONFIG_ARCH_BOARD_<UPPERCASE>=y

# Custom directory pointers (for vendor code outside nuttx tree)
CONFIG_ARCH_BOARD_CUSTOM_DIR="../vendor_<name>/boards/..."
CONFIG_ARCH_CHIP_CUSTOM_DIR="../vendor_<name>/chips/..."

# Memory layout
CONFIG_RAM_START=0x4FF00000
CONFIG_RAM_SIZE=786432
CONFIG_MM_REGIONS=2

# Console
CONFIG_<CHIP>_UART0=y
CONFIG_UART0_SERIAL_CONSOLE=y

# NuttShell
CONFIG_SYSTEM_NSH=y
CONFIG_INIT_ENTRYPOINT="nsh_main"

# Board control
CONFIG_BOARDCTL=y
CONFIG_BOARD_LATE_INITIALIZE=y
```

### 5.4 Minimum NSH Baseline defconfig

For new hardware bring-up, start with the minimum configuration to reach the NSH prompt:

```
CONFIG_ARCH="risc-v"                    # Target CPU architecture
CONFIG_ARCH_CHIP="esp32p4"             # Target SoC family
CONFIG_ARCH_CHIP_ESP32P4=y            # Specific model
CONFIG_ARCH_BOARD="esp32p4-ev-board"   # Board directory name
CONFIG_ARCH_BOARD_ESP32P4_EV_BOARD=y

CONFIG_RAM_START=0x...                 # Main SRAM physical address
CONFIG_RAM_SIZE=...                    # Main SRAM size (bytes)
CONFIG_MM_REGIONS=2                    # Number of heap regions
CONFIG_BOARD_LOOPSPERMSEC=...          # Busy-wait delay calibration

CONFIG_ESP32P4_UART0=y                # Enable UART peripheral
CONFIG_UART0_SERIAL_CONSOLE=y         # Designate as system console

CONFIG_SYSTEM_NSH=y                    # Enable NSH shell
CONFIG_INIT_ENTRYPOINT="nsh_main"     # Shell entry point
```

### 5.5 Recommended Bring-up Workflow

1. **Step 0**: Hardware ready, UART TX/RX pins connected to host
2. **Step 1**: Apply minimum config, compile, flash, verify NSH prompt on serial
3. **Step 2**: Incrementally enable subsystems by hardware capability (filesystem, network, graphics, sensors)
4. **Step 3**: After porting is stable, do release trimming

### 5.6 Common Kconfig Patterns

#### Conditional Compilation

```kconfig
config CHIP_SPI1
    bool "Enable SPI1 (General Purpose)"
    default n
    ---help---
        Enable SPI1 bus driver for general purpose peripherals.
```

#### Dependencies

```kconfig
config CHIP_GPIO_IRQ
    bool "Enable GPIO interrupt support"
    default n
    depends on CHIP_GPIO
```

#### Select (Auto-enable)

```kconfig
config CHIP_USBDEV
    bool "Enable USB Device Controller"
    default n
    select USBDEV
    select USBDEV_DUALSPEED
```

#### Range Constraints

```kconfig
config CHIP_PSRAM_SIZE
    int "PSRAM Size (MB)"
    default 32
    range 2 128
```

---

## 6. ESP32-P4 Adaptation Status and Tasks

### 6.1 Hardware Platform

| Item | Specification |
|------|---------------|
| Chip | Espressif ESP32-P4 |
| Board | ESP32-P4 Function EV Board |
| CPU | Dual-core RISC-V Hazard3 (HP Core 400MHz + LP Core 40MHz) |
| Instruction Set | RV32IMAFCXcheri (HP), RV32IMAC (LP) |
| SRAM | 768 KB HP SRAM + 16 KB LP SRAM |
| PSRAM | Up to 32 MB OPI PSRAM |
| Flash | 16 MB SPI Flash (OPI/QPI) |
| Key Peripherals | USB OTG 2.0 HS, MIPI-DSI, MIPI-CSI, I2S, SPI, I2C, UART, ADC/DAC, GPIO, GDMA, Ethernet MAC, SDIO/MMC |

Note: ESP32-P4 does not integrate WiFi/BLE; these require an external companion chip (e.g., ESP32-C6 on the EVB).

### 6.2 Memory Layout

| Region | Address | Size | Description |
|--------|---------|------|-------------|
| Internal SRAM | 0x4FF00000 | 768 KB | HP core main memory |
| LP SRAM | 0x50108000 | 16 KB | LP core memory |
| PSRAM | 0x48000000 | Up to 32 MB | External OPI PSRAM |
| Flash (Cache) | 0x42000000 | 16 MB | SPI Flash cache-mapped |

### 6.3 Adaptation Phases and Status

| Phase | Content | Priority | Status |
|-------|---------|----------|--------|
| M1: Environment Setup | Dev environment, toolchain, shared repo | P0 | **DONE** (2026-08-09) |
| M2: Single-core Bring-up | UART, GPIO, Timer, basic memory, NSH shell | P0 | **Code DONE** (2026-08-09), pending HW flash test |
| M3: SMP + PSRAM + Flash | Dual-core SMP, PSRAM driver, Flash filesystem | P1 | Pending |
| M4: Core Peripherals | SPI, I2C, GDMA, USB OTG HS, ADC/DAC, WDT | P1 | Pending |
| M5: Advanced Features | MIPI-DSI display, MIPI-CSI camera, I2S audio, Ethernet, SDIO, LP core | P2 | Pending |
| M6: Stability and Power | Power management, long-run stability, code size optimization | -- | Pending |

### 6.4 Completed Work (M1 + M2)

#### Chip Layer: 16 files, 3,078 lines

| File | Lines | Function |
|------|-------|----------|
| `esp32p4_start.c` | 207 | C entry point. Initializes PSRAM, PLL clock, then calls `nx_start()` |
| `esp32p4_irq.c` | 315 | PLIC interrupt controller. Implements claim/complete protocol, `g_irqvector[]` |
| `esp32p4_clockconfig.c` | 241 | PLL and clock tree. CPU 400MHz, APB 80MHz, UART/Timer/GPIO clock enables |
| `esp32p4_serial.c` | 652 | Full NuttX UART lower-half driver. UART0/UART1, interrupt-driven TX/RX |
| `esp32p4_gpio.c` | 344 | GPIO driver. 54 pins, config/read/write, interrupt dispatch (edge/level) |
| `esp32p4_timerisr.c` | 219 | System tick using HP Timer Group 0, Timer 0. Prescaler=80 (1MHz), auto-reload |
| `esp32p4_allocateheap.c` | 171 | Dual-region heap: PSRAM (up to 32MB) primary, HP SRAM (768KB) secondary |
| `Kconfig` | 206 | Config options: UART, Timer, GPIO, PSRAM, Flash, CPU freq, SPI, I2C, LTO |
| `Make.defs` | 45 | Build rules, lists 7 CHIP_CSRCS |
| `CMakeLists.txt` | 27 | CMake build rules |
| 6 hardware headers | ~544 | SoC, UART, PLIC, Timer, GPIO, Clock register definitions |

#### Board Layer: 12 files, 1,008 lines

| File | Lines | Function |
|------|-------|----------|
| `configs/default/defconfig` | 55 | Custom dir pointers, UART0 console, GPIO, Timer, PSRAM 32MB, 16MB Flash, LTO, NSH |
| `src/esp32p4_bringup.c` | 245 | Board init: register GPIO/SPI/I2C drivers, `board_early/late_initialize()` |
| `src/esp32p4_boot.c` | 194 | Early init: IO mux, power domain, console verification, LED/button GPIO |
| `src/esp32p4-evb.h` | 104 | Board private header, LED/button macros |
| `include/board.h` | 66 | Hardware definitions: LED GPIO, button GPIO, clock frequencies, UART defaults |
| `scripts/ld.script` | 119 | Flash at 0x42000000 (16MB), SRAM at 0x4FF00000 (768KB) |
| `Kconfig` | 63 | Board config: LED, button, LCD, touchscreen, USB |
| Build files | ~162 | Make.defs, CMakeLists.txt, Makefile |

### 6.5 Remaining Tasks

#### Phase 3: SMP + PSRAM + Flash

- [ ] Implement SMP multi-core management (`esp32p4_smp.c`)
- [ ] Implement PSRAM initialization and driver (`esp32p4_psram.c`)
- [ ] Implement SPI Flash driver (`esp32p4_flash.c`)
- [ ] Configure MTD partitions and LittleFS filesystem
- [ ] Verify SMP scheduling
- [ ] Verify filesystem read/write

#### Phase 4: Core Peripherals

- [ ] Implement SPI bus driver (`esp32p4_spi.c`)
- [ ] Implement I2C bus driver (`esp32p4_i2c.c`)
- [ ] Implement GDMA driver (`esp32p4_dma.c`)
- [ ] Implement USB OTG HS DCD driver (`esp32p4_usbdev.c`)
- [ ] Implement ADC/DAC drivers
- [ ] Implement watchdog driver

#### Phase 5: Advanced Features

- [ ] Implement MIPI-DSI framebuffer driver (`esp32p4_mipidsi.c`)
- [ ] Implement MIPI-CSI camera driver
- [ ] Implement I2S audio driver (`esp32p4_i2s.c`)
- [ ] Implement SDIO/MMC driver
- [ ] Implement Ethernet MAC driver (`esp32p4_emac.c`)
- [ ] Implement LP core communication via OpenAMP/RPMsg (`esp32p4_lpcore.c`)

#### Phase 6: Stability and Power

- [ ] Power management implementation
- [ ] 24-hour stability testing
- [ ] Code size optimization
- [ ] Performance benchmarking

### 6.6 Risk Assessment

| Risk | Level | Mitigation |
|------|-------|------------|
| SMP dual-core stability | High | Complete single-core bring-up first; reference QEMU RISC-V 64 SMP implementation |
| PSRAM initialization failure | Medium | Reference ESP-IDF code; add self-test; configure reasonable cache policy |
| USB OTG HS enumeration failure | Medium | Implement FS mode first; use usbtrace for debugging |
| Code size exceeds SRAM | Medium | Enable LTO and section GC; trim unnecessary features |
| Register definition errors | Low | Extract from ESP-IDF source; cross-validate with datasheet |
| Toolchain compatibility | Low | Use ESP-IDF-provided `gcc-riscv32-esp-elf` or xPack `riscv-none-elf-gcc` |

### 6.7 Required File Checklist (by priority)

| Priority | File | Function | Est. Lines |
|----------|------|----------|------------|
| P0 | `esp32p4_start.c` | Boot entry | ~200 |
| P0 | `esp32p4_irq.c` | Interrupt controller | ~300 |
| P0 | `esp32p4_clockconfig.c` | Clock configuration | ~200 |
| P0 | `esp32p4_serial.c` | UART driver | ~600 |
| P0 | `esp32p4_gpio.c` | GPIO driver | ~400 |
| P0 | `esp32p4_timerisr.c` | System tick | ~200 |
| P0 | `esp32p4_allocateheap.c` | Heap allocation | ~150 |
| P1 | `esp32p4_smp.c` | SMP management | ~300 |
| P1 | `esp32p4_psram.c` | PSRAM driver | ~400 |
| P1 | `esp32p4_flash.c` | Flash/MTD | ~500 |
| P1 | `esp32p4_spi.c` | SPI bus | ~600 |
| P1 | `esp32p4_i2c.c` | I2C bus | ~500 |
| P1 | `esp32p4_dma.c` | GDMA | ~400 |
| P1 | `esp32p4_usbdev.c` | USB DCD | ~2500 |
| P2 | `esp32p4_mipidsi.c` | MIPI-DSI FB | ~1500 |
| P2 | `esp32p4_emac.c` | Ethernet MAC | ~1500 |
| P2 | `esp32p4_i2s.c` | I2S audio | ~800 |
| P2 | `esp32p4_lpcore.c` | LP core comm | ~600 |

---

## 7. Development Environment Setup

### 7.1 Requirements

| Item | Minimum | Recommended |
|------|---------|-------------|
| OS | Ubuntu 20.04+ | Ubuntu 22.04 LTS |
| RAM | 8 GB | 16 GB |
| Disk | 30 GB | 50 GB |
| CPU | 4 cores | 8+ cores |

### 7.2 Toolchain Installation

```bash
# RISC-V toolchain (ESP32-P4)
# Option A: xPack (no sudo required)
wget https://github.com/xpack-dev-tools/riscv-none-elf-gcc-xpack/releases/download/v14.2.0-3/xpack-riscv-none-elf-gcc-14.2.0-3-linux-x64.tar.gz
tar -xzf xpack-riscv-none-elf-gcc-*.tar.gz -C ~/tools/xpack-riscv-none-elf-gcc --strip-components=1
echo 'export PATH=~/tools/xpack-riscv-none-elf-gcc/bin:$PATH' >> ~/.bashrc

# Option B: ESP-IDF toolchain
git clone --recursive https://github.com/espressif/esp-idf.git ~/esp/esp-idf
cd ~/esp/esp-idf && ./install.sh esp32p4 && source export.sh

# ARM toolchain
sudo apt-get install -y gcc-arm-none-eabi

# Build dependencies
sudo apt-get install -y build-essential cmake ninja-build ccache kconfig-frontends
pip3 install pyelftools esptool
```

### 7.3 Repository Setup

```bash
# Traditional approach (~8 hours for first sync)
cd ~
repo init -u <manifest-url>
repo sync -c -j8

# Optimized approach with git --reference (~15-30 minutes)
# Uses NFS shared bare repository as object store
cd ~
repo init -u <manifest-url>
repo sync --reference=/nfs/openvela-bare.git -c -j8
```

### 7.4 Hardware Connection

```
ESP32-P4 EVB                    Host Machine
+----------------+               +----------------+
|   USB-UART     |---------------| /dev/ttyUSB0   |  Debug serial (NSH)
|   (UART0)      |               |                |
+----------------+               +----------------+
|   USB OTG HS   |---------------| /dev/ttyACM0   |  USB Device
|   (Type-C)     |               |                |
+----------------+               +----------------+
|   JTAG (opt)   |---------------| OpenOCD        |  Debug interface
+----------------+               +----------------+
```

Serial terminal configuration:
```bash
minicom -D /dev/ttyUSB0 -b 115200
# or
screen /dev/ttyUSB0 115200
```

---

## 8. Testing and Debugging

### 8.1 Build and Flash

```bash
# Configure
./tools/configure.sh esp32p4-evb:default

# Build
make -j$(nproc)

# Flash (enter download mode: hold BOOT, press RST, release BOOT)
esptool.py --chip esp32p4 --port /dev/ttyACM0 --baud 460800 \
    write_flash 0x0 nuttx.bin

# Serial monitor
minicom -D /dev/ttyACM0 -b 115200
```

### 8.2 Phase-by-Phase Test Matrix

#### Phase 1: Single-core Bring-up

| Test | Method | Pass Criteria |
|------|--------|---------------|
| Compile | `make -j$(nproc)` | Zero errors |
| Flash | `esptool.py write_flash` | Flash succeeds |
| UART console | Connect serial terminal | See boot log |
| NSH Shell | Type commands | `nsh>` prompt available |
| GPIO LED | `echo 1 > /dev/gpio26` | LED on/off |
| Timer Tick | `date` command | System time increments |
| Memory | `free` command | Shows correct SRAM heap size |

#### Phase 2: SMP + PSRAM + Flash

| Test | Method | Pass Criteria |
|------|--------|---------------|
| SMP startup | `cat /proc/cpuinfo` | Shows 2 CPUs |
| PSRAM detection | `free` | Shows SRAM + PSRAM regions |
| Filesystem | `mount -t lfs /dev/mtdblock1 /mnt` | Mount succeeds |
| File I/O | `echo "hello" > /mnt/test.txt && cat /mnt/test.txt` | Content matches |

### 8.3 Debugging Methods

#### JTAG Debugging

```bash
# Start OpenOCD
openocd -f interface/esp_usb_jtag.cfg -f target/esp32p4.cfg

# Connect GDB
riscv-none-elf-gdb nuttx
(gdb) target remote :3333
(gdb) break chip_start
(gdb) continue
```

#### Backtrace Analysis

```bash
# Auto-parse with esp-idf monitor
idf.py -p /dev/ttyACM0 monitor

# Manual parse
riscv-none-elf-addr2line -pfiaC -e nuttx 0x4ff01234 0x4ff01230
```

#### NSH Diagnostic Commands

```bash
nsh> help              # List all available commands
nsh> free              # Memory usage
nsh> ps                # Process list
nsh> ls /dev/          # List all devices
nsh> cat /proc/mm      # Memory details
nsh> cat /proc/irqs    # Interrupt statistics
nsh> reboot            # System restart
nsh> uname -a          # System information
```

---

## 9. References

### Official Documentation

| Resource | URL |
|----------|-----|
| openvela Porting Guide | https://doc.openvela.com/document?id=215&version=trunk&language=cn |
| openvela Driver Development Guide | https://github.com/open-vela/docs/blob/dev-ai-contest-2026/zh-cn/device_dev_guide/driver/driver_development.md |
| Hardware Porting Track Guide | https://github.com/open-vela/docs/blob/dev-ai-contest-2026/zh-cn/contest_2026/hardware_porting/hardware_porting_track_guide.md |
| NSH Baseline defconfig Reference | https://github.com/open-vela/docs/blob/dev-ai-contest-2026/zh-cn/contest_2026/hardware_porting/defconfig_reference/minimum_nsh_baseline.md |
| vendor_template | https://github.com/open-vela/vendor_template |
| ESP32-S3 Porting Example | https://github.com/open-vela/vendor_espressif/blob/dev-ai-contest-2026/boards/esp32s3/esp32s3-eye/README_zh-cn.md |

### ESP32-P4 Hardware Documentation

| Resource | URL |
|----------|-----|
| ESP32-P4 EVB User Guide | https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/esp32p4/esp32-p4x-function-ev-board/user_guide.html |
| ESP32-P4 Technical Reference Manual | https://www.espressif.com/zh-hans/products/socs/esp32-p4 |

### Local Documentation

| File | Description |
|------|-------------|
| `/home/geo/openvela/docs/esp32p4-adaptation-guide.md` | Complete ESP32-P4 adaptation guide with code examples |
| `/home/geo/openvela/docs/esp32p4/移植过程记录.md` | Porting process record with file inventory |
| `/home/geo/openvela/docs/esp32p4/里程碑.md` | Milestone definitions and status tracking |
| `/home/geo/openvela/vendor_esp32p4/docs/environment-setup-guide.md` | Development environment setup (Ubuntu VM) |
| `/home/geo/openvela/vendor_esp32p4/docs/flash-and-test-guide.md` | Flash and module testing guide |
| `/home/geo/openvela/shared-repo-solution/docs/chip-specific-notes.md` | Chip-specific configuration notes for all supported platforms |

### AI Development Skills

| Skill | Purpose |
|-------|---------|
| `nuttx-driver-development` | Create/update/review NuttX device drivers |
| `driver-code-reviewer` | Driver code quality review |
| `driver-workflow` | End-to-end driver development workflow |
