# ESP32-P4 移植适配（contest2026_324_xiaoqiaoliushui / 新硬件适配赛道）

## 一、作品简介

在 **ESP32-P4 Function EV Board（rev v3.2）** 上完成 openvela/NuttX 的完整新平台适配：

- **可复现的最小 NSH 基线**：USB-Serial/JTAG 一体烧录/控制台，真机验证 `uname -a` / `free` / `ps` / `lvcalc` 等交互。
- **一站式扩展配置 `nshlcd`**：MIPI-DSI LCD（ILI9881C 800×1280，寄存器级驱动）、GT911 触摸（I2C1 @0x5d/0x14）、LVGL 计算器 + 系统监视器、ES8311 I2S 音频播放（`spktest`）、BLE 状态机 stub（`bledctl` / `lvbled`）。
- **离线可复现构建**：ESP HAL（esp-hal-3rdparty @ `b90b1837cb5`）通过只读本地备份自动恢复，全程零 GitHub 克隆。
- **开源 AI Coding 沉淀**：一个可复现的 skill（`esp32p4-offline-porting`），并按大赛要求归集开发日志。

## 二、选题方向

新硬件适配赛道（ESP32-P4X Function EV Board）+ 少量 AI 硬件演示（LVGL UI demo / 音频 / BLE 状态演示）。

## 三、目录结构

- `vendor_esp32p4/` — ESP32-P4 板级支持包（早阶段 nsh_031 编译产物路径）
- `vendor/openvela/boards/contest2026_288_board/` — **本次主战场**（out-of-tree BSP）：
  - `chip/espressif/` — 芯片层（含 `esp32p4_mipi_dsi.c` / `esp_ble.c`）
  - `src/esp32p4_{lcd,touch,audio}.c` — LCD / 触摸 / 音频板级 glue
  - `configs/{nsh,nshlcd}/defconfig` — NSH 基线 / 全功能演示配置
  - `tools/prepare_esp_hal.sh` — 离线 HAL 准备（备份自愈）
  - `README.md` — 本板级 README（含全部 builtin app 与命令）
- `nuttx/` — openvela NuttX 内核（pin 到 dev-ai-contest-2026 分支基线）
- `logs/` — AI Coding 日志（大赛规范，manifest 已就绪）
- `.claude/skills/esp32p4-offline-porting/` — 本次沉淀的 AI Skill

## 四、运行方式（评委可照抄复现）

```bash
# 1) 编译
PATH="$HOME/.local/bin:$PATH" \
  ./build.sh vendor/openvela/boards/contest2026_288_board/configs/nsh -j4

# 2) 烧录（偏移必须是 0x2000）
esptool --chip esp32p4 --port /dev/ttyACM0 --baud 921600 \
  write-flash 0x2000 nuttx/nuttx.bin
# 成功标志: "Chip type: ESP32-P4 (revision v3.2)" + "Hash of data verified."

# 3) 控制台（USB-Serial/JTAG 一体：烧录/日志同口）
picocom -b 115200 /dev/ttyACM0 --flow nortscts
# 注意: 中途不要拔插 USB —— USB 重枚举会触发 USB-Serial/JTAG 的软复位（rst:0x17），
#       表现为重新打印 ROM 启动序列，这是平台特性而非崩溃。

# 4) nshlcd 演示配置（可选，LCD+触摸+LVGL+音频+BLE）：
PATH="$HOME/.local/bin:$PATH" \
  ./build.sh vendor/openvela/boards/contest2026_288_board/configs/nshlcd -j4
```

已验证符号（`nm nuttx`）：`lvcalc_main` / `lvsysmon_main` / `spktest_main` / `bledctl_main` / `lvbled_main` 全部链接完成。

## 五、AI Coding 使用说明

- **CLAUDE CODE + openvela AI Skills** 完成 95% 的开发工作：NSH 三层根因定位、MIPI-DSI 寄存器级移植、GT911 轮询 lower-half、LVGL 接入、音频框架管线、BLE stub、离线构建工程化、README/日志/提交材料。
- AI 的帮助：真机调试时通过**串口日志归档 + elf 符号解析**把 boot 阶段崩溃定位在 `rtc_clk_cal`（并归因到绕过 dev 分支 ABI 差异的 nuttx 版本选择）；逐项 disable/enable 排除法在硬件环境不可达时持续提供可执行下一步。
- **技能沉淀**：`.claude/skills/esp32p4-offline-porting/SKILL.md`（7 节方法论：三层根因树 / PSRAM 堆 / offline HAL / builtin 四件套 / 模板 / 铁律 / 适用时机），内容全部来自本次真机调试记录。
- 完整对话日志见 `logs/` 目录（按大赛规范自动收集）。

## 六、已知问题 / 最后遗留（诚实声明）

1. **I2S0 BCLK/WS/DIN/DOUT/MCLK GPIO** 需按板卡原理图填 `CONFIG_ESPRESSIF_I2S0_*PIN`，当前 defconfig 只开了 TX/RX，引脚留 TODO（-1 默认）。上真机前如需音频请填号。
2. **GT911 I2C 地址**需在真机 `i2ctool -b1 scan-obuf` 确认（代码默认 0x5d，备选 0x14），配置项 `CONFIG_ESPRESSIF_BOARD_GT911_I2C_ADDRESS`。
3. **LCD 复位/背光 GPIO** 默认 -1（跳过），上真机按原理图填 `CONFIG_ESPRESSIF_*_(RESET|BACKLIGHT)_GPIO`。
4. **BLE 是 stub**（P4 无本地 2.4G 射频）：互斥锁状态机 + 5 API，真射频需要 ESP32-C6 副控经 SPI-HCI 接入，当前只做"软件状态机演示"（`bledctl state` 会明示 stub only）。
5. **USB-Serial/JTAG 行为**：USB 重枚举会触发 `rst:0x17 (CHIP_USB_UART_RESET)`，呈现为 ROM 日志重播，不是崩溃 — 演示时避免中途拔插 USB。
6. **nshlcd（LCD/触摸/LVGL/audio/BLE）镜像已链接**（符号齐全），但真机一次跑通尚未完成（启动早期停在 HAL `rtc_clk_cal` 待排查），提交演示以 **nsh 基线（烧录即跑、真机已验）**为安全版本。

## 附

- 板级详细 README：`vendor/openvela/boards/contest2026_288_board/README.md`
- 参考工程对照：`/mnt/hgfs/share/esp/参考工程/contest2026_288_*`（同一块板，NSH 基线复现成功）
- openvela manifest：`contest2026_324_xiaoqiaoliushui.xml`
- AI Skill：`.claude/skills/esp32p4-offline-porting/SKILL.md`
- AI Coding 日志：`logs/`（大赛规范格式）
