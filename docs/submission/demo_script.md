# 演示视频脚本（≤5 分钟建议 3 分 30 秒）

主题：ESP32-P4 新平台适配 + LVGL 演示（contest2026_324_xiaoqiaoliushui）

## 建议分镜（3 段式）

| 段 | 镜头 | 时长 | 画面/动作 |
|---|---|---|---|
| 1 | 开机即用 | 00:00–00:30 | 实拍板子连接 USB，picocom 打开，敲 Enter 出现 `nsh>`；连续演示 `uname -a`、`free`、`ps` |
| 2 | 功能演示 | 00:30–02:00 | (a) `lvsysmon` 打开 procfs 面板（任务/内存/uptime 自动刷新）；(b) `lvcalc` 计算器添加运算；(c) `i2ctool -b1 scan-obuf` 打印 GT911 (0x18/0x5d/0x14)；(d) `spktest` 播放 440 Hz 正弦（板端真实发声）；(e) `bledctl adv openvela-p4` 状态机演示 |
| 3 | AI 能力展示 | 02:00–03:00 | Claude Code 命令行实操三镜头：`esp32p4-offline-porting` skill 被引用；distclean→build 一键离线恢复（prepare_esp_hal.sh 自愈片段）；调 AI 调试日志 `logs/Geoorgiee/2026-09-20/session.nshlcd.md` 展示根因跟踪链 |
| 4 | 数据就位 | 03:00–03:30 | 镜像大小/入口地址/烧录时间/烧录 3 次成功等关键量化数据帧配合口播（与"技术报告 3.5 表格"同口径）|

## 注意事项（与真机演示一致）

- 视频里 **不要中途拔插 USB**（会触发 USB-Serial/JTAG 的 rst:0x17 良性复位，看起来像"重启"）。
- 不含语音唤醒演示（当前作品未接 MIC/AI 推理），展示以视觉效果与 CLI 交互为主。
- 若 clip 不能都上 5 分钟内，可优先 1+2 段，AI 能力展示放弃 3 秒内部速查。

---

## 打包清单说明

1. 技术报告：`tech_report.md` → **导出 PDF**（建议 pandoc: `pandoc tech_report.md -o tech_report.pdf` 或 Word 另存）
2. 演示视频：`demo_script.md` 按脚本拍摄 → `demo.mp4`
3. 作品展示照片：板卡前/后/侧/俯视图（放 `photos/` 目录）
4. 海报 / 答辩 PPT（可选，入围决赛再交）

## 压缩包命名

按下官方示例：

```
流水不争水-ESP32P4新平台适配-contest2026_324_xiaoqiaoliushui.zip
```

包含：`tech_report.pdf` + `demo.mp4` + `照片/`（可选）。
源码与 AI 日志**不要**放压缩包（评审直接 clone 仓库验证）。
