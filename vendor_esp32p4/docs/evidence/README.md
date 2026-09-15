# 证据链目录

本目录包含 ESP32-P4 适配过程中的完整证据链。

## 目录结构

```
evidence/
├── build-logs/          # 构建日志
├── flash-logs/          # 烧录日志
├── verification-logs/   # 验证日志
├── measurement-data/    # 测量数据
├── photos/              # 照片和视频
└── README.md            # 本文件
```

## 证据类型

### 构建日志 (build-logs/)
- 构建配置
- 构建输出
- 构建错误
- 构建报告

### 烧录日志 (flash-logs/)
- 芯片识别
- 烧录过程
- 烧录验证
- 烧录报告

### 验证日志 (verification-logs/)
- NSH 启动验证
- UART 验证
- I2C 验证
- 显示验证
- 触摸验证
- 功能验证

### 测量数据 (measurement-data/)
- GPIO 波形数据
- Timer 稳定性数据
- 冷启动测试数据
- 重启测试数据

### 照片和视频 (photos/)
- 硬件连接照片
- 显示效果照片
- 测试过程视频

## 使用方法

1. 初始化证据目录: `./scripts/collect_evidence.sh --init`
2. 收集构建证据: `./scripts/collect_evidence.sh --build <config>`
3. 收集烧录证据: `./scripts/collect_evidence.sh --flash <config>`
4. 收集验证证据: `./scripts/collect_evidence.sh --verify <config>`
5. 生成证据报告: `./scripts/collect_evidence.sh --report`
