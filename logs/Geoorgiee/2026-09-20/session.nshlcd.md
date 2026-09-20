# ESP32-P4 contest2026_288_board — nshlcd 收口日志(2026-09-20)

## 里程碑
1. NSH 基线起死回生(根因 3 层): UART0<->USB-Serial/JTAG defconfig 互换、
   nuttx 76354c63785 新 ABI(nxtask_init 9参数→posix_spawnattr)、MM_KERNEL_HEAP
  在新内核被 gate 而未开,三处全部修正后 1 号真机可以正常进 NSH 交互。
2. MIPI-DSI /dev/fb0 (ILI9881C 800×1280) 寄存器级驱动已移植并按 HAL b90b1837cb5
   修正 host 0x500A0000 / bridge 0x500A0800 等偏移;GT911 (I2C1 @0x5d/0x14)
   轮询 half-half /dev/input/event0;LVGL v9 接入 lv_nuttx_entry。
3. 内外 offline 化:chip/espressif/Make.defs 短路 + prepare_esp_hal.sh 自动从
   只读备份修复(零 GitHub),备份永不写入。
4. apps 侧新增:apps/graphics/lvgl_calc(计算器) + lvgl_sysmon(procfs 监视器)
   + apps/audioutils/spktest(440Hz sine 播放)+ apps/wireless/{bledctl,lvbled}
   (BLE stub + CLI --help);全部 builtin 自动生成 .bdat/.pdat 注册。
5. 真机五步验收(待插板待验证):
   uname -a → lvsysmon → lvcalc → fb0 画光标 → GT911 i2c scan → spktest → bledctl

## 遗留(不在本轮完成范围)
- I2S0 BCLK/WS/DIN/DOUT/MCLK GPIO 映射需按 EV 板原理图填
- GT911 实际 I2C 地址待 i2ctool 扫描确认(0x5d 或 0x14)
- LCD 复位/背光 GPIO 号待真机标注
- BLE C6 副控 SPI-HCI 未实装(当前只是 stub 但可做内存状态机演示)
