# ESP-IDF Development Guide (v6.0.2)

Based on ESP-IDF v6.0.2 official documentation. Source: `/home/geo/esp/v6.0.2/esp-idf/docs/en/`

---

## Table of Contents

1. [Project Structure and Build System (CMake)](#1-project-structure-and-build-system-cmake)
2. [Configuration System (menuconfig/sdkconfig)](#2-configuration-system-menuconfigsdkconfig)
3. [Flash and Debug Workflow](#3-flash-and-debug-workflow)
4. [Component Management](#4-component-management)
5. [OTA Update Mechanism](#5-ota-update-mechanism)
6. [Power Management APIs](#6-power-management-apis)
7. [Bootloader](#7-bootloader)
8. [Partition Tables](#8-partition-tables)

---

## 1. Project Structure and Build System (CMake)

### 1.1 Core Concepts

An ESP-IDF project is an amalgamation of **components**. Key concepts:

- **Project**: A directory containing all files and configuration to build a single app (executable), plus partition table, data partitions, filesystem partitions, and a bootloader.
- **Project Configuration**: Held in `sdkconfig` in the project root, modified via `idf.py menuconfig`.
- **App**: An executable built by ESP-IDF. A project typically builds two apps -- the "project app" (main firmware) and a "bootloader app" (initial bootloader).
- **Components**: Modular standalone code compiled into static libraries (`.a` files) and linked to an app.
- **Target**: The hardware for which an application is built (e.g., `esp32`, `esp32s3`, `esp32p4`).

**Not part of the project:**
- ESP-IDF itself (linked via `IDF_PATH` environment variable)
- The toolchain (installed in system PATH)

### 1.2 Example Project Directory Structure

```
myProject/
    CMakeLists.txt              # Top-level project CMakeLists
    sdkconfig                   # Project configuration (auto-generated)
    dependencies.lock           # Component Manager lock file
    bootloader_components/      # Optional: custom bootloader components
        boot_component/
            CMakeLists.txt
            Kconfig
            src1.c
    components/                 # Optional: project-specific components
        component1/
            CMakeLists.txt
            Kconfig
            src1.c
        component2/
            CMakeLists.txt
            Kconfig
            src1.c
            include/
                component2.h
    managed_components/         # Auto-managed by IDF Component Manager
        namespace__component-name/
            CMakeLists.txt
            src1.c
            idf_component.yml
            include/
                src1.h
    main/                       # Special "main" component
        CMakeLists.txt
        src1.c
        src2.c
        idf_component.yml
    build/                      # Build output directory
```

### 1.3 Project CMakeLists.txt

Minimal required structure:

```cmake
cmake_minimum_required(VERSION 3.22)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(myProject)
```

**Mandatory parts (in order):**
1. `cmake_minimum_required(VERSION 3.22)` -- must be the first line
2. `include($ENV{IDF_PATH}/tools/cmake/project.cmake)` -- pulls in ESP-IDF build system
3. `project(myProject)` -- creates the project, sets the project name

**Optional project variables** (set before `include()`):

| Variable | Description |
|---|---|
| `COMPONENT_DIRS` | Directories to search for components. Defaults to `IDF_PATH/components`, `PROJECT_DIR/components`, and `EXTRA_COMPONENT_DIRS` |
| `EXTRA_COMPONENT_DIRS` | Additional component directories |
| `COMPONENTS` | List of component names to build. Defaults to all found components |
| `BOOTLOADER_IGNORE_EXTRA_COMPONENT` | Components in `bootloader_components/` to ignore |
| `BOOTLOADER_EXTRA_COMPONENT_DIRS` | Extra directories for bootloader components |

### 1.4 Component CMakeLists.txt

Minimal component registration:

```cmake
idf_component_register(SRCS "foo.c" "bar.c"
                       INCLUDE_DIRS "include"
                       REQUIRES mbedtls)
```

| Argument | Description |
|---|---|
| `SRCS` | Source files (`*.c`, `*.cpp`, `*.cc`, `*.S`) |
| `INCLUDE_DIRS` | Public include directories |
| `PRIV_INCLUDE_DIRS` | Private include directories |
| `REQUIRES` | Public component dependencies |
| `PRIV_REQUIRES` | Private component dependencies |
| `EMBED_FILES` | Binary files to embed in `.rodata` |
| `EMBED_TXTFILES` | Text files to embed as null-terminated strings |
| `REQUIRED_IDF_TARGETS` | Restrict to specific target chips |

**Component search precedence (highest to lowest):**
1. Project components (`PROJECT_DIR/components/`)
2. Components from `EXTRA_COMPONENT_DIRS`
3. Managed components (`managed_components/`)
4. ESP-IDF components (`IDF_PATH/components/`)

**Common automatic dependencies** (always available): `cxx`, `esp_libc`, `freertos`, `esp_hw_support`, `heap`, `log`, `soc`, `hal`, `esp_rom`, `esp_common`, `esp_system`, `xtensa/riscv`.

### 1.5 idf.py Build Commands

| Command | Description |
|---|---|
| `idf.py build` | Full build (compile, generate bootloader, partition table, app binaries) |
| `idf.py clean` | Clean build artifacts |
| `idf.py fullclean` | Remove entire build directory |
| `idf.py reconfigure` | Re-run CMake configuration |
| `idf.py menuconfig` | Open TUI configuration editor |
| `idf.py flash` | Build and flash to device |
| `idf.py monitor` | Open serial monitor |
| `idf.py -p PORT flash monitor` | Combined flash and monitor |
| `idf.py partition-table` | Print partition table summary |
| `idf.py set-target TARGET` | Set build target (e.g., `esp32s3`) |
| `idf.py size` | Show app size breakdown |
| `idf.py size-components` | Show size by component |
| `idf.py size-files` | Show size by file |
| `idf.py dfu` | Build DFU image |
| `idf.py dfu-flash` | Flash via USB DFU |

**Using CMake directly** (idf.py is a wrapper):

```bash
mkdir -p build
cd build
cmake .. -G Ninja
ninja          # Build
ninja flash    # Flash
```

### 1.6 Custom sdkconfig Defaults

Create `sdkconfig.defaults` in the project directory to override ESP-IDF defaults without a full `sdkconfig`. Target-specific files like `sdkconfig.defaults.esp32` are also supported.

Loading order:
1. `sdkconfig.defaults`
2. `sdkconfig.defaults.<TARGET_NAME>`
3. Additional files listed in `SDKCONFIG_DEFAULTS`
4. `sdkconfig` (user-modified values)

Override via CMake:
```cmake
set(SDKCONFIG_DEFAULTS "sdkconfig.defaults;sdkconfig_devkit1")
```

---

## 2. Configuration System (menuconfig/sdkconfig)

### 2.1 Overview

ESP-IDF uses **Kconfig** (same system as the Linux kernel) for project configuration. Configuration options are defined in `Kconfig` files within each component.

**Configuration files generated:**
- `sdkconfig` -- human-readable key=value pairs (do not edit manually)
- `sdkconfig.h` -- C header with `#define CONFIG_*` macros
- `sdkconfig.cmake` -- CMake variables
- `sdkconfig.json` -- JSON format

### 2.2 Editing Configuration

**CLI (most portable):**
```bash
idf.py menuconfig
```
Opens a TUI (Text-based User Interface). Navigation with arrow keys; hotkeys shown at bottom of window.

**IDE plugins:**
- VS Code: ESP-IDF Extension -- Project Configuration Editor
- Eclipse: ESP-IDF Eclipse Plugin -- SDK Configuration Editor

### 2.3 Using Configuration in Code

**In C code:**
```c
#include "sdkconfig.h"

#if CONFIG_USE_WARP
    set_warp_speed(CONFIG_WARP_SPEED);
#else
    set_warp_speed(0);
#endif
```

**In CMake:**
```cmake
if(CONFIG_USE_WARP)
    set(WARP_SPEED ${CONFIG_WARP_SPEED})
else()
    set(WARP_SPEED 0)
endif()
```

### 2.4 Defining Custom Configuration Options

Create `Kconfig` or `Kconfig.projbuild` in the component directory:

- `Kconfig`: Options appear under "Component configuration" in menuconfig
- `Kconfig.projbuild`: Options appear in the top-level menu

Example `Kconfig`:
```kconfig
menu "Motors configuration"
    config SUBLIGHT_DRIVE_ENABLED
        bool "Enable sublight drive"
        default n
        depends on SPACE_SHIP
        help
            This option enables sublight on our spaceship.
endmenu
```

### 2.5 Configuration Loading Order

1. Default values from `Kconfig` files
2. Values from `sdkconfig.defaults` (if found)
3. Values from `sdkconfig` (if found)

Later values override earlier ones.

### 2.6 Backward Compatibility (sdkconfig.rename)

When renaming Kconfig options, create `sdkconfig.rename` in the component root:
```
CONFIG_OLD_NAME CONFIG_NEW_NAME        # Direct replacement
CONFIG_OLD_NAME !CONFIG_NEW_NAME       # Boolean inversion
```

### 2.7 Configuration Report

The configuration report is automatically printed during build or `idf.py menuconfig`. Generate JSON report:
```bash
idf.py config-report
```

Suppress warnings with pragma in Kconfig:
```kconfig
config LED_PIN # ignore: multiple-definition
    int "Pin for LED"
    default 1
```

---

## 3. Flash and Debug Workflow

### 3.1 Building

```bash
idf.py build
```

This compiles the application and all ESP-IDF components, then generates the bootloader, partition table, and application binaries.

### 3.2 Flashing

```bash
idf.py -p /dev/ttyUSB0 flash
```

Replace `/dev/ttyUSB0` with your device's serial port. If omitted, `idf.py` auto-detects USB ports.

The `flash` command automatically builds before flashing.

**Flash arguments files** (generated in `build/`):
- `flash_project_args` -- flash entire project
- `flash_app_args` -- flash only the app
- `flash_bootloader_args` -- flash only the bootloader
- `flasher_args.json` -- JSON format for tools

Manual flash with esptool:
```bash
esptool --chip esp32 write-flash @build/flash_project_args
```

**Flash erase commands:**
```bash
idf.py -p PORT erase-flash      # Erase entire flash
idf.py -p PORT erase-otadata    # Erase OTA data
```

### 3.3 Monitoring

```bash
idf.py -p /dev/ttyUSB0 monitor
```

Combines build, flash, and monitor in one step:
```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

Exit monitor: `Ctrl+]`

### 3.4 DFU (Device Firmware Upgrade via USB)

For chips with USB OTG (ESP32-S2, ESP32-S3, ESP32-P4):

```bash
idf.py dfu           # Build DFU image
idf.py dfu-flash     # Flash via USB DFU
idf.py dfu-list      # List DFU devices
```

Select specific device when multiple are connected:
```bash
idf.py dfu-flash --path 1-10
```

### 3.5 Troubleshooting

- **Permission denied on Linux**: Add user to `dialout` or `uucp` group:
  ```bash
  sudo usermod -a -G dialout $USER
  ```
- **Garbled output**: Check XTAL frequency in menuconfig (`Component config` > `Hardware Settings` > `Main XTAL Config`)
- **Python compatibility**: ESP-IDF requires Python 3.10+

---

## 4. Component Management

### 4.1 IDF Component Manager

The IDF Component Manager automatically downloads dependencies during CMake configuration. Components can be sourced from:
- **ESP Component Registry**: https://components.espressif.com
- **Git repositories**
- **Local directories**

### 4.2 Manifest File (idf_component.yml)

Each component's dependencies are defined in `idf_component.yml`:

```yaml
dependencies:
  # From ESP Component Registry
  example/cmp: ">=1.0.0"

  # From Git repository
  test_component:
    path: test_component
    git: ssh://git@gitlab.com/user/components.git

  # Local dependency
  some_local_component:
    path: ../../projects/component
```

### 4.3 Component Manager Commands

| Command | Description |
|---|---|
| `idf.py create-manifest` | Create manifest for main component |
| `idf.py create-manifest --component=my_component` | Create manifest for specific component |
| `idf.py add-dependency example/cmp` | Add dependency to main component |
| `idf.py add-dependency --component=my_component example/cmp<=3.3.3` | Add to specific component |
| `idf.py update-dependencies` | Update all dependencies |
| `idf.py reconfigure` | Re-run CMake (re-triggers Component Manager) |

### 4.4 Managed Files

- `dependencies.lock` -- full list of dependencies with versions (auto-generated, do not edit)
- `managed_components/` -- downloaded component sources (auto-managed, do not modify manually)

To override a managed component: copy it to the `components/` directory and modify there.

Disable Component Manager:
```bash
export IDF_COMPONENT_MANAGER=0
```

### 4.5 Board Support Packages (BSPs)

BSPs are distributed via the Component Manager and found in the ESP Component Registry:

```bash
idf.py add-dependency esp-box           # ESP-BOX BSP
idf.py add-dependency esp_wrover_kit    # ESP-WROVER-KIT BSP
```

### 4.6 Embedding Binary Data

In component CMakeLists.txt:
```cmake
idf_component_register(...
                       EMBED_FILES server_root_cert.der)

idf_component_register(...
                       EMBED_TXTFILES server_root_cert.pem)
```

In project CMakeLists.txt:
```cmake
target_add_binary_data(myproject.elf "main/data.bin" TEXT)
```

Access in C code:
```c
extern const uint8_t server_root_cert_pem_start[] asm("_binary_server_root_cert_pem_start");
extern const uint8_t server_root_cert_pem_end[]   asm("_binary_server_root_cert_pem_end");
```

---

## 5. OTA Update Mechanism

### 5.1 OTA Process Overview

OTA allows a device to update firmware over Wi-Fi, Bluetooth, or Ethernet.

**Safe update mode** (application): Writes new firmware to the inactive OTA slot. Only switches boot target after verification. Power loss during update does not brick the device.

**Unsafe update mode** (bootloader, partition table, data partitions): Uses a temporary partition. Power loss during the final copy phase can cause issues.

### 5.2 Partition Table Requirements for OTA

```csv
# Name,   Type, SubType,  Offset,   Size
nvs,      data, nvs,      0x9000,   0x4000
otadata,  data, ota,      0xd000,   0x2000
phy_init, data, phy,      0xf000,   0x1000
factory,  app,  factory,  0x10000,  1M
ota_0,    app,  ota_0,    ,         1M
ota_1,    app,  ota_1,    ,         1M
```

Or select "Factory app, two OTA definitions" in menuconfig.

**OTA Data Partition**: Type `data`, subtype `ota`, 0x2000 bytes. Stores which OTA slot to boot. Two flash sectors for redundancy.

### 5.3 Core OTA API (esp_ota_ops)

```c
#include "esp_ota_ops.h"

// Get the partition to write to
const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);

// Begin OTA update
esp_ota_handle_t update_handle;
esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);

// Write firmware data (in chunks)
esp_ota_write(update_handle, data, data_len);

// End and set boot partition
esp_ota_end(update_handle);
esp_ota_set_boot_partition(update_partition);

// Reboot
esp_restart();
```

### 5.4 App Rollback

When `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE` is enabled:

1. New app is set to `ESP_OTA_IMG_NEW` state
2. Bootloader changes state to `ESP_OTA_IMG_PENDING_VERIFY` on first boot
3. App must call `esp_ota_mark_app_valid_cancel_rollback()` after successful self-test
4. If app fails or doesn't confirm, it's marked `ESP_OTA_IMG_ABORTED` and rolled back

```c
const esp_partition_t *running = esp_ota_get_running_partition();
esp_ota_img_states_t ota_state;
if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
    if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
        bool diagnostic_is_ok = diagnostic();
        if (diagnostic_is_ok) {
            esp_ota_mark_app_valid_cancel_rollback();
        } else {
            esp_ota_mark_app_invalid_rollback_and_reboot();
        }
    }
}
```

### 5.5 Anti-Rollback

Prevents downgrading to older firmware versions. Uses eFuse to store security version.

- Enable `CONFIG_BOOTLOADER_APP_ANTI_ROLLBACK`
- Set security version with `CONFIG_BOOTLOADER_APP_SECURE_VERSION`
- Check before downloading: `esp_efuse_check_secure_version(new_app_info.secure_version)`

### 5.6 ESP HTTPS OTA (Simplified API)

```c
#include "esp_https_ota.h"

esp_http_client_config_t config = {
    .url = CONFIG_FIRMWARE_UPGRADE_URL,
    .cert_pem = (char *)server_cert_pem_start,
};
esp_https_ota_config_t ota_config = {
    .http_config = &config,
};
esp_err_t ret = esp_https_ota(&ota_config);
if (ret == ESP_OK) {
    esp_restart();
}
```

**Features:**
- Partial image download (configurable chunk size)
- OTA resumption (resume from last written byte)
- Signature verification
- Pre-encrypted firmware support
- Bulk flash erase for faster updates

**Events** (register with `esp_event_handler_register`):
- `ESP_HTTPS_OTA_START`, `ESP_HTTPS_OTA_CONNECTED`, `ESP_HTTPS_OTA_GET_IMG_DESC`
- `ESP_HTTPS_OTA_VERIFY_CHIP_ID`, `ESP_HTTPS_OTA_VERIFY_CHIP_REVISION`
- `ESP_HTTPS_OTA_WRITE_FLASH`, `ESP_HTTPS_OTA_UPDATE_BOOT_PARTITION`
- `ESP_HTTPS_OTA_FINISH`, `ESP_HTTPS_OTA_ABORT`

### 5.7 OTA Tool (otatool.py)

Located at `$IDF_PATH/components/app_update/otatool.py`.

```bash
# CLI usage
otatool.py --port "/dev/ttyUSB1" erase_otadata
otatool.py --port "/dev/ttyUSB1" erase_ota_partition --slot 0
otatool.py --port "/dev/ttyUSB1" switch_ota_partition --slot 1
otatool.py --port "/dev/ttyUSB1" read_ota_partition --name=ota_3 --output=ota_3.bin
```

---

## 6. Power Management APIs

### 6.1 Overview

ESP-IDF power management can:
- Adjust APB (Advanced Peripheral Bus) frequency
- Adjust CPU frequency
- Automatically enter Light-sleep mode

Components express requirements by acquiring/releasing power management locks.

Enable at compile time: `CONFIG_PM_ENABLE`

### 6.2 Power Management Locks

| Lock | Description |
|---|---|
| `ESP_PM_CPU_FREQ_MAX` | Request CPU at maximum frequency |
| `ESP_PM_APB_FREQ_MAX` | Request APB at maximum frequency (80 MHz) |
| `ESP_PM_NO_LIGHT_SLEEP` | Disable automatic Light-sleep |

Locks have acquire/release counters. Must release same number of times as acquired.

### 6.3 Configuration

```c
#include "esp_pm.h"

esp_pm_config_t pm_config = {
    .max_freq_mhz = 240,         // Maximum CPU frequency
    .min_freq_mhz = 40,          // Minimum CPU frequency (idle)
    .light_sleep_enable = true    // Enable auto Light-sleep
};
esp_pm_configure(&pm_config);
```

Or enable DFS automatically via Kconfig:
- `CONFIG_PM_DFS_INIT_AUTO` -- auto DFS with max freq from `CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ`
- Min frequency locked to XTAL frequency

**Note**: Auto Light-sleep requires `CONFIG_FREERTOS_USE_TICKLESS_IDLE`.

### 6.4 Sleep Modes

#### Light-sleep

- CPUs, digital peripherals, most RAM are clock-gated; supply voltage reduced
- Internal states preserved on wake
- Entered via `esp_light_sleep_start()`

#### Deep-sleep

- CPUs, most RAM, all APB-clocked peripherals powered off
- Only RTC controller, ULP coprocessor, RTC memory remain powered
- Entered via `esp_deep_sleep_start()`
- CPU context lost on wake; bootloader re-runs

#### Auto Light-sleep

When all locks released and all FreeRTOS tasks blocked, the system:
1. Calculates next wake-up event time
2. If idle time exceeds threshold (`CONFIG_FREERTOS_IDLE_TIME_BEFORE_SLEEP`), enters Light-sleep
3. Wakes before the nearest event

### 6.5 Wakeup Sources

| Source | Mode | Description |
|---|---|---|
| Timer | Both | RTC timer, microsecond precision |
| Touchpad | Both | Touch sensor interrupt |
| ext0 | Both | Single RTC GPIO to predefined level |
| ext1 | Both | Multiple RTC GPIOs (ANY_HIGH or ALL_LOW) |
| GPIO | Light-sleep | Any GPIO, configurable high/low |
| UART | Light-sleep | RX positive edges |
| ULP | Both | ULP coprocessor wakeup |

**Enabling wakeup sources:**
```c
// Timer
esp_sleep_enable_timer_wakeup(5000000);  // 5 seconds

// GPIO
esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0);  // Wake when GPIO0 is low

// ext1
esp_sleep_enable_ext1_wakeup(BITMASK, ESP_EXT1_WAKEUP_ALL_LOW);

// UART
uart_set_wakeup_threshold(UART_NUM_0, 3);
esp_sleep_enable_uart_wakeup(UART_NUM_0);
```

**Checking wakeup cause:**
```c
esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
```

### 6.6 Power-down Options

```c
// Configure power domains
esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_OFF);
```

Power domain options: `ESP_PD_OPTION_ON`, `ESP_PD_OPTION_OFF`, `ESP_PD_OPTION_AUTO`.

**IO isolation** (Deep-sleep, to reduce current):
```c
rtc_gpio_isolate(GPIO_NUM_12);
```

### 6.7 Dynamic Frequency Scaling and Peripheral Drivers

When DFS is enabled, APB frequency can change within a single RTOS tick. Peripheral drivers that are DFS-aware automatically acquire `ESP_PM_APB_FREQ_MAX`:

- SPI master, I2C, I2S, SDMMC (during transactions)
- SPI slave, GPTimer, Ethernet, WiFi, TWAI, Bluetooth (while enabled)

Use `REF_TICK`, `XTAL`, or `RC_FAST` as peripheral clock source for consistent behavior during DFS.

### 6.8 Debugging Power Management

```c
esp_pm_dump_locks(stdout);                    // Dump all locks
esp_pm_get_lock_stats_all(&stats);            // Get stats for all lock types
esp_pm_lock_get_stats(lock, &lock_stats);     // Stats for specific lock
```

Enable profiling: `CONFIG_PM_PROFILING`

---

## 7. Bootloader

### 7.1 Second Stage Bootloader

Located at flash offset `CONFIG_BOOTLOADER_OFFSET` (typically 0x1000 for ESP32, 0x0 for others).

**Functions:**
1. Minimal internal module configuration
2. Initialize Flash Encryption and/or Secure Boot (if configured)
3. Select application partition based on partition table and OTA data
4. Load app image to RAM and transfer control

### 7.2 Bootloader Compatibility

- Newer bootloader can boot apps from newer ESP-IDF versions
- Bootloader does NOT support booting apps from older ESP-IDF versions
- OTA updates can flash new apps but cannot flash a new bootloader

### 7.3 Bootloader Size Limits

| Target | Max Size |
|---|---|
| ESP32 | 48 KB (0xC000 bytes) |
| ESP32-S2, S3, C2, C3, C6, H2, P4 | 64 KB (0x10000 bytes) |

If too large, reduce log level or increase `CONFIG_PARTITION_TABLE_OFFSET`.

### 7.4 Factory Reset

Configure in Kconfig:
- `CONFIG_BOOTLOADER_FACTORY_RESET` -- enable factory reset
- `CONFIG_BOOTLOADER_DATA_FACTORY_RESET` -- comma-separated list of partitions to erase
- `CONFIG_BOOTLOADER_OTA_DATA_ERASE` -- boot from factory partition after reset
- `CONFIG_BOOTLOADER_NUM_PIN_FACTORY_RESET` -- GPIO pin to trigger
- `CONFIG_BOOTLOADER_HOLD_TIME_GPIO` -- hold time (default 5 seconds)

### 7.5 Custom Bootloader

Two approaches:
1. **Hooks**: Connect hooks to bootloader initialization
2. **Override**: Replace bootloader implementation entirely

Custom bootloader components go in `bootloader_components/` directory.

### 7.6 Recovery Bootloader (newer chips)

When primary bootloader fails, ROM bootloader attempts to load recovery bootloader from eFuse-specified address.

---

## 8. Partition Tables

### 8.1 Overview

Flash can contain multiple apps and data types. Partition table is flashed at offset 0x8000 (configurable). Max 95 entries, 4 KB total with MD5 checksum.

### 8.2 Built-in Partition Tables

**Single factory app, no OTA:**
```
# Name,   Type, SubType, Offset,  Size
nvs,      data, nvs,     0x9000,  0x6000
phy_init, data, phy,     0xf000,  0x1000
factory,  app,  factory, 0x10000, 1M
```

**Factory app, two OTA definitions:**
```
# Name,   Type, SubType, Offset,  Size
nvs,      data, nvs,     0x9000,  0x4000
otadata,  data, ota,     0xd000,  0x2000
phy_init, data, phy,     0xf000,  0x1000
factory,  app,  factory, 0x10000,  1M
ota_0,    app,  ota_0,   0x110000, 1M
ota_1,    app,  ota_1,   0x210000, 1M
```

### 8.3 Custom Partition Table CSV

Select "Custom partition table CSV" in menuconfig.

```csv
# Name,   Type, SubType, Offset, Size, Flags
nvs,      data, nvs,     0x9000, 0x4000
otadata,  data, ota,     0xd000, 0x2000
phy_init, data, phy,     0xf000, 0x1000
factory,  app,  factory, 0x10000, 1M
ota_0,    app,  ota_0,   ,       1M
ota_1,    app,  ota_1,   ,       1M
nvs_key,  data, nvs_keys,,       0x1000
```

- Blank offsets are auto-calculated
- App partitions must be 64 KB (0x10000) aligned
- Sizes can use K (1024) or M (1024*1024) suffixes

### 8.4 Partition Types and Subtypes

| Type | Value | Subtypes |
|---|---|---|
| `app` | 0x00 | `factory` (0x00), `ota_0`-`ota_15` (0x10-0x1F), `test` (0x20) |
| `data` | 0x01 | `ota` (0x00), `phy` (0x01), `nvs` (0x02), `coredump` (0x03), `nvs_keys` (0x04), `fat` (0x81), `spiffs` (0x82), `littlefs` (0x83) |
| `bootloader` | 0x02 | `primary` (0x00), `ota` (0x01), `recovery` (0x02) |
| `partition_table` | 0x03 | `primary` (0x00), `ota` (0x01) |
| Custom | 0x40-0xFE | Application-defined |

### 8.5 Flags

- `encrypted` -- partition encrypted if Flash Encryption enabled
- `readonly` -- read-only partition (data type only, except ota/coredump)

Multiple flags: `encrypted:readonly`

### 8.6 Partition Tool (parttool.py)

Located at `$IDF_PATH/components/partition_table/parttool.py`.

```bash
# CLI
parttool.py --port "/dev/ttyUSB1" erase_partition --partition-name=storage
parttool.py --port "/dev/ttyUSB1" read_partition --partition-type=data --partition-subtype=spiffs --output "spiffs.bin"
parttool.py --port "/dev/ttyUSB1" write_partition --partition-name=factory --input "factory.bin"
```

---

## Quick Reference

### Essential Environment Variables

| Variable | Description |
|---|---|
| `IDF_PATH` | Path to ESP-IDF directory |
| `ESPPORT` | Serial port (alternative to `-p`) |
| `ESPBAUD` | Flash baud rate |

### Essential Kconfig Options

| Option | Description |
|---|---|
| `CONFIG_IDF_TARGET` | Target chip |
| `CONFIG_PARTITION_TABLE_TYPE` | Partition table type |
| `CONFIG_FREERTOS_HZ` | RTOS tick rate |
| `CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ` | Default CPU frequency |
| `CONFIG_PM_ENABLE` | Power management |
| `CONFIG_FREERTOS_USE_TICKLESS_IDLE` | Required for auto Light-sleep |
| `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE` | OTA rollback |
| `CONFIG_BOOTLOADER_APP_ANTI_ROLLBACK` | Anti-rollback protection |

### Key Source Locations

| Path | Description |
|---|---|
| `$IDF_PATH/tools/cmake/project.cmake` | Build system entry point |
| `$IDF_PATH/components/bootloader/` | Bootloader source |
| `$IDF_PATH/components/partition_table/` | Partition tools |
| `$IDF_PATH/components/app_update/` | OTA tools and API |
| `$IDF_PATH/docs/en/` | Documentation root |
