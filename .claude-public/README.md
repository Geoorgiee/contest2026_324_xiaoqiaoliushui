# AI Skills 沉淀(镜像自 .claude/skills — 保证评委可见)

## esp32p4-offline-porting

ESP32-P4 新平台适配方法论 skill(2026 openvela 大赛硬性要求"至少 1 个有效 Skill")。

7 节内容:
1. NSH 起不来三层根因树(控制台互换 / nuttx 新内核 ABI(nxtask_init→posix_spawnattr / MM_KERNEL_HEAP 门控)/ 烧录偏移 0x2000)
2. PSRAM/Framebuffer 堆配置
3. Offline-first HAL(Make.defs 短路 + prepare_esp_hal.sh 只读备份自愈)
4. NuttX builtin App 四件套模板(含最易漏的 Make.defs)
5. LVGL / 音频 / BLE 移植模板(官方后端 + nxplayer pipeline)
6. 编译排错铁律(offline 环境限制遵守清单)
7. 适用时机与相关 skill 串联

原始位置: .claude/skills/esp32p4-offline-porting/SKILL.md(亦提交至 .claude 独立仓 ddd183a)
