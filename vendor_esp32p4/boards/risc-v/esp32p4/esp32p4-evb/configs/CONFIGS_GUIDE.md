# ESP32-P4 配置指南

本指南介绍 ESP32-P4 EVB 的各个配置及其验证方法。

## 配置概述

| 配置 | 用途 | 验证目标 | 优先级 |
|------|------|----------|--------|
| nsh | 最小 NSH 配置 | NSH 启动成功 | 高 |
| uart0 | UART0 验证配置 | UART 通信正常 | 高 |
| i2c | I2C 验证配置 | I2C 总线工作正常 | 高 |
| display | 显示验证配置 | MIPI-DSI 显示正常 | 中 |
| touch | 触摸验证配置 | GT911 触摸正常 | 中 |
| lvgl | LVGL UI 配置 | LVGL UI 运行正常 | 低 |
| full | 完整功能配置 | 所有外设工作正常 | 低 |

## 配置详情

### 1. nsh - 最小 NSH 配置

**用途**：最小 NSH 启动配置，用于验证基础系统启动。

**包含功能**：
- UART0 串口控制台
- NSH shell
- 基础系统服务

**验证方法**：
```bash
# 1. 构建固件
./vendor_esp32p4/scripts/build.sh nsh

# 2. 烧录固件
./vendor_esp32p4/scripts/flash.sh

# 3. 连接串口
picocom -b 115200 /dev/ttyACM0

# 4. 验证命令
nsh> help
nsh> uname -a
nsh> free
nsh> ps
nsh> reboot
```

**验收标准**：
- [ ] NSH 提示符出现
- [ ] `help` 命令显示可用命令
- [ ] `uname -a` 显示系统信息
- [ ] `free` 显示内存信息
- [ ] `ps` 显示进程信息
- [ ] `reboot` 正常重启

**证据收集**：
```bash
./vendor_esp32p4/scripts/collect_evidence.sh --build nsh
./vendor_esp32p4/scripts/collect_evidence.sh --flash nsh
./vendor_esp32p4/scripts/collect_evidence.sh --verify nsh
```

---

### 2. uart0 - UART0 验证配置

**用途**：验证 UART0 串口通信功能。

**包含功能**：
- UART0 串口控制台
- UART 测试工具
- NSH shell

**验证方法**：
```bash
# 1. 构建固件
./vendor_esp32p4/scripts/build.sh uart0

# 2. 烧录固件
./vendor_esp32p4/scripts/flash.sh

# 3. 连接串口
picocom -b 115200 /dev/ttyACM0

# 4. 验证命令
nsh> ls /dev/ttyS0
nsh> echo "Hello UART" > /dev/ttyS0
```

**验收标准**：
- [ ] UART0 设备节点存在
- [ ] 串口通信正常
- [ ] 数据收发正确

**证据收集**：
```bash
./vendor_esp32p4/scripts/collect_evidence.sh --build uart0
./vendor_esp32p4/scripts/collect_evidence.sh --flash uart0
./vendor_esp32p4/scripts/collect_evidence.sh --verify uart0
```

---

### 3. i2c - I2C 验证配置

**用途**：验证 I2C 总线通信功能。

**包含功能**：
- I2C 驱动
- I2C 测试工具
- NSH shell

**验证方法**：
```bash
# 1. 构建固件
./vendor_esp32p4/scripts/build.sh i2c

# 2. 烧录固件
./vendor_esp32p4/scripts/flash.sh

# 3. 连接串口
picocom -b 115200 /dev/ttyACM0

# 4. 验证命令
nsh> ls /dev/i2c1
nsh> i2c dev 0x03 0x77
```

**验收标准**：
- [ ] I2C 设备节点存在
- [ ] I2C 总线扫描正常
- [ ] 能检测到 I2C 设备

**证据收集**：
```bash
./vendor_esp32p4/scripts/collect_evidence.sh --build i2c
./vendor_esp32p4/scripts/collect_evidence.sh --flash i2c
./vendor_esp32p4/scripts/collect_evidence.sh --verify i2c
```

---

### 4. display - 显示验证配置

**用途**：验证 MIPI-DSI 显示功能。

**包含功能**：
- EK79007 MIPI-DSI 驱动
- framebuffer 驱动
- 显示测试工具
- NSH shell

**验证方法**：
```bash
# 1. 构建固件
./vendor_esp32p4/scripts/build.sh display

# 2. 烧录固件
./vendor_esp32p4/scripts/flash.sh

# 3. 连接串口
picocom -b 115200 /dev/ttyACM0

# 4. 验证命令
nsh> ls /dev/fb0
nsh> dsi_probe pattern 10
nsh> dsi_probe video 10
nsh> fb
```

**验收标准**：
- [ ] framebuffer 设备节点存在
- [ ] 色条测试显示正常
- [ ] 视频通路测试正常
- [ ] framebuffer 绘制正常

**证据收集**：
```bash
./vendor_esp32p4/scripts/collect_evidence.sh --build display
./vendor_esp32p4/scripts/collect_evidence.sh --flash display
./vendor_esp32p4/scripts/collect_evidence.sh --verify display
```

**物理验证**：
- 拍摄显示效果照片
- 记录显示分辨率和刷新率
- 测量背光亮度

---

### 5. touch - 触摸验证配置

**用途**：验证 GT911 触摸屏功能。

**包含功能**：
- GT911 触摸驱动
- 触摸测试工具
- NSH shell

**验证方法**：
```bash
# 1. 构建固件
./vendor_esp32p4/scripts/build.sh touch

# 2. 烧录固件
./vendor_esp32p4/scripts/flash.sh

# 3. 连接串口
picocom -b 115200 /dev/ttyACM0

# 4. 验证命令
nsh> ls /dev/input0
nsh> gt911_probe 15
```

**验收标准**：
- [ ] 触摸设备节点存在
- [ ] 触摸事件检测正常
- [ ] 坐标数据正确
- [ ] 多点触摸支持

**证据收集**：
```bash
./vendor_esp32p4/scripts/collect_evidence.sh --build touch
./vendor_esp32p4/scripts/collect_evidence.sh --flash touch
./vendor_esp32p4/scripts/collect_evidence.sh --verify touch
```

**物理验证**：
- 测试单点触摸
- 测试多点触摸
- 测试滑动手势
- 记录触摸响应时间

---

### 6. lvgl - LVGL UI 配置

**用途**：验证 LVGL UI 显示和交互功能。

**包含功能**：
- LVGL 图形库
- EK79007 MIPI-DSI 驱动
- GT911 触摸驱动
- Smart Home UI 应用
- NSH shell

**验证方法**：
```bash
# 1. 构建固件
./vendor_esp32p4/scripts/build.sh lvgl

# 2. 烧录固件
./vendor_esp32p4/scripts/flash.sh

# 3. 连接串口
picocom -b 115200 /dev/ttyACM0

# 4. 验证命令
nsh> ls /dev/fb0
nsh> ls /dev/input0
nsh> smart_home
```

**验收标准**：
- [ ] LVGL UI 显示正常
- [ ] 触摸交互响应正常
- [ ] 界面切换流畅
- [ ] 字体显示正确

**证据收集**：
```bash
./vendor_esp32p4/scripts/collect_evidence.sh --build lvgl
./vendor_esp32p4/scripts/collect_evidence.sh --flash lvgl
./vendor_esp32p4/scripts/collect_evidence.sh --verify lvgl
```

**物理验证**：
- 拍摄 UI 界面照片
- 测试触摸交互
- 记录 UI 响应时间
- 测试长时间运行稳定性

---

### 7. full - 完整功能配置

**用途**：验证所有外设功能。

**包含功能**：
- 所有驱动（显示、触摸、音频、存储等）
- 所有测试工具
- 完整应用
- NSH shell

**验证方法**：
```bash
# 1. 构建固件
./vendor_esp32p4/scripts/build.sh full

# 2. 烧录固件
./vendor_esp32p4/scripts/flash.sh

# 3. 连接串口
picocom -b 115200 /dev/ttyACM0

# 4. 验证命令
nsh> help
nsh> uname -a
nsh> ls /dev/fb0
nsh> ls /dev/input0
nsh> ls /dev/i2c1
nsh> ls /dev/ttyS0
```

**验收标准**：
- [ ] 所有设备节点存在
- [ ] 所有外设工作正常
- [ ] 系统稳定运行
- [ ] 无内存泄漏

**证据收集**：
```bash
./vendor_esp32p4/scripts/collect_evidence.sh --build full
./vendor_esp32p4/scripts/collect_evidence.sh --flash full
./vendor_esp32p4/scripts/collect_evidence.sh --verify full
```

**物理验证**：
- 测试所有外设
- 长时间运行测试
- 压力测试
- 记录系统资源使用情况

---

## 验证流程

### 第一阶段：基础验证
1. 构建并烧录 `nsh` 配置
2. 验证 NSH 启动成功
3. 收集构建和烧录证据

### 第二阶段：外设验证
1. 构建并烧录 `uart0` 配置
2. 验证 UART 通信正常
3. 构建并烧录 `i2c` 配置
4. 验证 I2C 总线正常

### 第三阶段：显示验证
1. 构建并烧录 `display` 配置
2. 验证 MIPI-DSI 显示正常
3. 收集显示效果照片

### 第四阶段：触摸验证
1. 构建并烧录 `touch` 配置
2. 验证 GT911 触摸正常
3. 收集触摸测试数据

### 第五阶段：完整验证
1. 构建并烧录 `lvgl` 配置
2. 验证 LVGL UI 运行正常
3. 构建并烧录 `full` 配置
4. 验证所有功能正常

## 证据收集

### 构建证据
- 构建日志
- 构建配置
- 固件文件信息
- 构建报告

### 烧录证据
- 烧录日志
- 芯片识别信息
- Flash 信息
- 烧录报告

### 验证证据
- 验证日志
- 测试结果
- 测量数据
- 验证报告

### 物理证据
- 硬件连接照片
- 显示效果照片
- 测试过程视频
- 测量数据记录

## 常见问题

### 构建失败
- 检查工具链是否正确安装
- 检查依赖库是否完整
- 查看构建日志定位错误

### 烧录失败
- 检查串口连接是否正常
- 检查设备是否进入下载模式
- 查看烧录日志定位错误

### 验证失败
- 检查硬件连接是否正确
- 检查驱动是否正确加载
- 查看验证日志定位问题

## 最佳实践

1. **分阶段验证**：从最小配置开始，逐步启用外设
2. **收集证据**：每次构建、烧录、验证都收集证据
3. **记录问题**：遇到问题及时记录和解决
4. **物理验证**：重要功能进行物理验证
5. **长稳测试**：关键功能进行长时间稳定性测试
