# ESP32-P4X contest board (contest2026_288_board)

该目录通过 manifest 映射到：

```text
vendor/openvela/boards/contest2026_288_board
```

它包含 ESP32-P4 out-of-tree 芯片层、ESP32-P4X Function EV Board 板级实现、链接脚本和多套 NSH 配置（含图形显示/触摸/音频/BLE 扩展）。当前真机基线为 **ESP32-P4 revision v3.2**，控制台使用 **J20 USB-Serial/JTAG**（`/dev/ttyACM0`）。

## 已验证能力（NSH 基线）

- NSH 启动与交互：`help`、`uname -a`、`free`、`ps`、`reboot`
- procfs 挂载（early init + board late init）
- USB-Serial/JTAG 烧录 + 内核控制台一体（`CONFIG_ESPRESSIF_USBSERIAL=y`）
- 真机烧录偏移 **0x2000**（`esptool --chip esp32p4 … write-flash 0x2000 nuttx/nuttx.bin`）

## 第三方依赖（离线优先）

运行以下脚本准备固定版本的 ESP HAL：

```bash
./tools/prepare_esp_hal.sh
```

要点：

- **优先从本地只读备份** `vendor_esp32p4/chips/esp32p4/esp-hal-3rdparty` 恢复（离线可用；备份本身永不写入）；
- 固定 commit `b90b1837cb5`，并应用 `patches/esp-hal-openvela-compat.patch`；
- 同时保证 mbedtls 子目录完整（`tf-psa-crypto` 等可按目录单点恢复）；
- `chip/esp-hal-3rdparty/` 本体被 gitignore，不入库。

`chip/espressif/Make.defs` 有同源短路：本地树存在 + commit 匹配 + mbedtls 就位 → 直接复用当前树，distclean 也不会触发任何克隆。

## 配置

| 配置 | 说明 |
|---|---|
| `configs/nsh/defconfig` | 最小 NSH 基线：USB-Serial/JTAG、NSH、procfs、board late init、`CONFIG_MM_KERNEL_HEAP=y`（新内核下 `kmm_*` 系列必需） |
| `configs/nshlcd/defconfig` | NSH + **MIPI-DSI `/dev/fb0`**（ILI9881C 800×1280）+ **GT911 touch `/dev/input/event0`**（I2C1 SDA=GPIO7 / SCL=GPIO8）+ **PSRAM 200MHz** + **LVGL**（计算器/系统监视器）+ **ES8311/I2S0 音频** + **BLE stub**（`bledctl` / `lvbled` CLI） |

## 应用（builtin）

| 命令 | 内容 |
|---|---|
| `lvcalc` | LVGL 计算器（fb0 显示 + 触摸输入） |
| `lvsysmon` | LVGL 系统监视器：uptime / 内存 / 任务表（procfs 数据，定时自动刷新） |
| `spktest` | 440 Hz 正弦播放：I2S0 → ES8311 → NS4150B 功放（`/dev/audio/pcm0`） |
| `bledctl` | BLE CLI（`adv <name>` / `state` / `quit`）；当前为 stub 内存状态机演示 |
| `lvbled` | LVGL 面板：BLE 广播状态显示（500ms 自动刷新，与 `bledctl` 配套） |

## 构建

从 openvela 工作区根目录执行：

```bash
# 最小 NSH 基线
PATH="$HOME/.local/bin:$PATH" \
  ./build.sh vendor/openvela/boards/contest2026_288_board/configs/nsh -j4

# 含 LCD / 触摸 / LVGL / 音频 / BLE 的演示配置
PATH="$HOME/.local/bin:$PATH" \
  ./build.sh vendor/openvela/boards/contest2026_288_board/configs/nshlcd -j4
```

产物：`nuttx/nuttx.bin`（当前 `nshlcd` 约 507 KB，入口 `0x4ff46120`）。

## 烧录与验收

```bash
# 烧录（偏移必须是 0x2000）
esptool --chip esp32p4 --port /dev/ttyACM0 --baud 921600 \
  write-flash 0x2000 nuttx/nuttx.bin

# 控制台（USB-Serial/JTAG）
picocom -b 115200 /dev/ttyACM0 --flow nortscts
```

真机五步回归：

```text
uname -a
lvsysmon
lvcalc
i2ctool -b1 scan-obuf    # GT911 实际地址确认（0x5d / 0x14）
spktest                  # ES8311 + I2S sine 播放
bledctl adv openvela-p4  # BLE（需接 ESP32-C6 子板才有射频）
```

## 真机待办（TODO）

1. **I2S0 `BCLK/WS/DIN/DOUT/MCLK` GPIO** —— 按板卡原理图填 `CONFIG_ESPRESSIF_I2S0_*PIN`；
2. **GT911 实际 I2C 地址** —— 0x5d / 0x14 二选一，真机 `i2ctool -b1` 扫描确认；
3. **LCD 复位/背光 GPIO** —— `CONFIG_ESPRESSIF_*_(RESET|BACKLIGHT)_GPIO`，当前默认 -1（跳过）；
4. **BLE 实装** —— ESP32-C6 副控 SPI-HCI transport（当前 stub 只翻状态不触碰射频）。

详细识别、烧录和验收步骤见仓库根 README 与 `docs/bringup/`。
