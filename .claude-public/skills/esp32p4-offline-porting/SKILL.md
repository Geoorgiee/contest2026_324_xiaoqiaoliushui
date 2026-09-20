---
name: esp32p4-offline-porting
description: "ESP32-P4 (contest2026_288_board) openvela/NuttX 新平台离线适配工作流: NSH 起不来的三层根因排查(defconfig 控制台互换 / nuttx 内核 ABI(nxtask_init/posix_spawnattr/MM_KERNEL_HEAP)/ 烧录偏移 0x2000), MIPI-DSI/GT911/LVGL/ES8311/BLE 的 out-of-tree BSP 移植模板, 以及零 GitHub 的离线 HAL 准备(Make.defs 短路 + prepare_esp_hal.sh 只读备份自愈)。Trigger: esp32p4 适配, P4 bringup, NSH 起不来, nshlcd, MIPI-DSI 移植, GT911, ES8311, 烧录偏移, 0x2000, MM_KERNEL_HEAP 报错, nxtask_init 编译错误, prepare_esp_hal, 离线适配, offline HAL."
---

# ESP32-P4 Offline Porting — openvela 新平台适配方法论

一个把"openvela/NuttX 移植到 ESP32-P4"这个动作沉淀成**可复现流程**的 skill,
适配 contest2026_288_board (ESP32-P4 Function EV Board rev v3.2) 真机。

本 skill 全部流程假设: **没有 GitHub 访问**、esp-hal-3rdparty 是本地只读
备份(备份不写),工作区是 openvela repo。

## 1. 起死回生三板斧(NSH 起不来 / 崩溃重启循环的根因树)

按命中率顺序排查,三层独立验证:

1. **控制台后端互换** — J20 USB-Serial/JTAG 是渲染口还是烧录口?
   `configs/nsh/defconfig` 里必须有:
   ```
   CONFIG_ESPRESSIF_USBSERIAL=y
   # CONFIG_ESPRESSIF_UART0 is not set
   ```
   UART0(GPIO37/38)配置对应 configs/uart0不要混进 nsh — 对 ACM
   口永远 "没有输出" 的典型症状。抓串口用:
   `picocom -b 115200 /dev/ttyACM0 --flow nortscts`

2. **nuttx 内核 ABI 漂移**(esp-hal-3rdparty 兼容补丁对不上)
    - 老签名: `nxtask_init(tcb,name,pri,NULL,stk,fn,argv,NULL,NULL)`(9 参数)
    - 新签名: `nxtask_init(tcb,name,entry,NULL,&posix_spawnattr,argv,NULL)`(7 参数,均由 attr 提供 priority/stacksize)
    - 若链接期 undefined reference 到 `kmm_malloc` 等 →
      **新版内核里 `kmm_*` 系列只在 `CONFIG_MM_KERNEL_HEAP=y` 才编译**,
      必须显式加: `CONFIG_MM_KERNEL_HEAP=y`(flat 无 MPU 也需要 kmm heap 区域)。

3. **烧录偏移** — `nuttx.bin` 必须烧在 **flash 0x2000**,
   不是 `0x0`。伪代码:
   `esptool --chip esp32p4 -p /dev/ttyACM0 -b 921600 write-flash 0x2000 nuttx/nuttx.bin`

调试难判时,先在 `__esp_start()` 的 ROM 日志里加 `showprogress("字母")` 打点
(在 `CONFIG_DEBUG_FEATURES=y` 下)。输出可以一直追到"哪个阶段才崩"。

## 2. SPIRAM / Framebuffer 堆

MIPI-DSI 双缓冲(800*1280*2B*2)≈4MB。要在 flat build 里健全:
```
CONFIG_ESPRESSIF_SPIRAM=y
CONFIG_ESPRESSIF_SPIRAM_BOOT_INIT=y
CONFIG_ESPRESSIF_SPIRAM_USER_HEAP=y
CONFIG_MM_KERNEL_HEAP=y
```
驱动端 fb 内存分配用 `kmm_zalloc`,浮到 PSRAM/USER_HEAP 才放得下。

## 3. Offline-first HAL(Make.defs 短路)

ESP32 out-of-tree BSP 需要保证 `chip/esp-hal-3rdparty` 就地可用:

- Make.defs 短路检查(local tree + commit match + mbedtls/psa 就位 → skip clone)
- `distclean` **不删 HAL 目录**, 否则下次 build 又要联网克隆
- 恢复顺序: chip tree(整目录) → mbedtls 子目录(.git/或纯目录皆可) →
  mbedtls 4.x 的 **`tf-psa-crypto`(psa 头所在目录)** 可单独从备份补齐
- `prepare_esp_hal.sh` 应该把以上三点做全,永远不 touch 备份本体

## 4. App/Builtin 注册模板(照抄即可)

一个 NuttX apps 下的新 app 需要 **4 件套**:

```
apps/<category>/<app>/
├── <app>_main.c        # 应用主函数
├── Kconfig             # CONFIG_<CAT>_<APP>
├── Makefile            # PROGNAME + MODULE $(CONFIG_…)
└── Make.defs           # ← 易漏!决定 CONFIGURED_APPS,没有它就不会注册
```

外加 aggregator `apps/<category>/Kconfig` 的 `source` 行(mkkconfig 自动生成,
手动加也行)。如果 make 输出没打印 `Register: <app>`,手写
`apps/builtin/registry/<progname>.bdat` + `.pdat` 再重链:
```
{ "appname", <pri>, <stk>, <appname>_main },
```

## 5. 应用开发积累

- **LVGL**(v9): NuttX 官方集成在 LVGL 仓库 `src/drivers/nuttx/lv_nuttx_entry.c`
  — 直接喂 `lv_nuttx_dsc_t{fb_path="/dev/fb0",input_path="/dev/input/event0"}`
  即可; app 主循环 `while(1){usleep(lv_timer_handler()*1000);}`
  (上游 `LV_USE_NUTTX` 需要 `apps/graphics/lvgl/Kconfig` 恢复被注释的
  `source "$APPSDIR/graphics/lvgl/lvgl/Kconfig"`)
- **音频**: nuttx 音频框架格式是 nxplayer 系(`apps/system/nxplayer` 为最精应参考):
  GETBUFFERINFO → ALLOCBUFFER → CONFIGURE → ENQUEUEBUFFER → START
  而 spktest 生成 440Hz sine 用 `sinf(2π*FREQ*t)`, buffer 大小用
  `ap_buffer_info_s.buffer_size`, **没有 `nframes` 字段 — 叫 `nbytes`**
- **BLE**: P4 本地无 2.4G 射频, 生产路径是 ESP32-C6 co-pro HCI-over-SPI。
  没有外围硬件时做 honest stub: 互斥锁保护状态机 + 5 API + clear TODO。

## 6. Bug 排错路径(普适)

```
1. 铁律: 绝不 modify nuttx core/vendor fork; 修改保持在你的 out-of-tree BSP + apps
2. 编译失败 → 先做零 statement check ("make -n 干跑")再改
3. distclean 只在要重建 defconfig 差异时用(并且已确保 HAL 目录会重生)
4. 反复编译失败时, 手动 `python3 olddefconfig` (kconfiglib) 可以比 kconfig-conf
   二进制多恢复一些隐藏默认值
5. build.log/gitee 登录名不是凭据, **永远不要上传公网**.
```

## 7. 何时该用

- openvela 新资产适配比赛作品收口时
- ESP32/P4 新板 bringup(boot 不亮/触屏/音频帧串无输出)
- 准备把一个适配做成可复现、可 结构化 的提交材料时

## Related Skills

- `contest-log-collector` — AI Coding 日志归集(按大赛要求自动入仓)
- `openvela-build` — 通用 build/flash 命令 IQ
- `embedded-bug-hunter` — 通用崩溃定位流程
