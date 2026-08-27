# openvela 共享仓库 Agent 自动实施方案

## 方案概述

本方案为 openvela (NuttX-based RTOS) 项目设计了一套基于 NFS 共享存储和 git --reference 的高效仓库同步方案，支持多芯片架构（乐鑫 ESP32、ARM Cortex-M/A、RISC-V）的自动化开发环境。

### 核心特性

1. **git --reference 优化**: 使用本地裸仓库作为参考对象库，避免重复下载相同对象
2. **NFS 共享存储**: 多台开发机共享同一份 git 对象库，节省磁盘空间和网络带宽
3. **自动化脚本**: 一键初始化、同步、构建、监控
4. **多芯片适配**: 支持乐鑫、ARM、RISC-V 架构的自动检测和配置
5. **监控维护**: 实时监控仓库状态、自动清理、告警通知

### 架构图

```
┌─────────────────────────────────────────────────────────────┐
│                      NFS Server                              │
│  ┌─────────────────────────────────────────────────────┐   │
│  │  /nfs/openvela-bare.git  (裸仓库 - git对象库)       │   │
│  │  ├── objects/  (所有git对象)                         │   │
│  │  ├── refs/     (引用)                               │   │
│  │  └── HEAD                                       │   │
│  └─────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
                              │
            ┌─────────────────┼─────────────────┐
            │                 │                 │
            ▼                 ▼                 ▼
    ┌───────────────┐ ┌───────────────┐ ┌───────────────┐
    │  开发机 1     │ │  开发机 2     │ │  开发机 3     │
    │  (ARM)        │ │  (RISC-V)     │ │  (ESP32)      │
    │               │ │               │ │               │
    │ openvela/     │ │ openvela/     │ │ openvela/     │
    │ ├── nuttx/    │ │ ├── nuttx/    │ │ ├── nuttx/    │
    │ ├── apps/     │ │ ├── apps/     │ │ ├── apps/     │
    │ └── vendor/   │ │ └── vendor/   │ │ └── vendor/   │
    │               │ │               │ │               │
    │ 使用 --reference │ │ 使用 --reference │ │ 使用 --reference │
    │ 指向NFS裸仓库 │ │ 指向NFS裸仓库 │ │ 指向NFS裸仓库 │
    └───────────────┘ └───────────────┘ └───────────────┘
```

### 性能优势

| 指标 | 传统方式 | 本方案 | 提升 |
|------|----------|--------|------|
| 首次同步时间 | 1-2 小时 | 15-30 分钟 | 75% |
| 磁盘占用 (每台) | ~20 GB | ~5 GB | 75% |
| 增量同步时间 | 5-10 分钟 | 1-2 分钟 | 80% |
| 网络带宽占用 | 高 | 低 | 90% |

---

## 实施步骤

### 第一阶段: NFS 服务器配置

1. 安装 NFS 服务
2. 创建共享目录
3. 配置 NFS 导出
4. 启动 NFS 服务

### 第二阶段: 裸仓库初始化

1. 创建裸仓库
2. 初始化 openvela 代码
3. 配置 git 对象库

### 第三阶段: 开发机配置

1. 挂载 NFS 共享
2. 配置 git --reference
3. 初始化开发仓库

### 第四阶段: 自动化脚本部署

1. 部署同步脚本
2. 部署构建脚本
3. 部署监控脚本

### 第五阶段: 监控和维护

1. 配置监控告警
2. 定期维护任务
3. 故障恢复流程

---

## 文件结构

```
shared-repo-solution/
├── README.md                    # 本文档
├── nfs-server-setup.sh          # NFS 服务器配置脚本
├── bare-repo-init.sh            # 裸仓库初始化脚本
├── dev-machine-setup.sh         # 开发机配置脚本
├── sync-repo.sh                 # 仓库同步脚本
├── build.sh                     # 构建脚本
├── monitor.sh                   # 监控脚本
├── maintenance.sh               # 维护脚本
├── config/
│   ├── nfs-exports              # NFS 导出配置
│   ├── gitconfig                # Git 配置模板
│   └── repo-manifest.xml        # Repo 清单配置
└── docs/
    ├── troubleshooting.md       # 故障排查指南
    └── chip-specific-notes.md   # 芯片特定说明
```

---

## 快速开始

```bash
# 1. 在 NFS 服务器上执行
sudo bash nfs-server-setup.sh
sudo bash bare-repo-init.sh

# 2. 在每台开发机上执行
sudo bash dev-machine-setup.sh <nfs-server-ip>
bash sync-repo.sh

# 3. 构建固件
bash build.sh <chip-target>
```

---

## 支持的芯片目标

| 芯片 | 架构 | 配置路径 | 说明 |
|------|------|----------|------|
| ESP32 | Xtensa | vendor/espressif/boards/esp32/ | 乐鑫 ESP32 |
| ESP32-S2 | Xtensa | vendor/espressif/boards/esp32s2/ | 乐鑫 ESP32-S2 |
| ESP32-S3 | Xtensa | vendor/espressif/boards/esp32s3/ | 乐鑫 ESP32-S3 |
| ESP32-C3 | RISC-V | vendor/espressif/boards/esp32c3/ | 乐鑫 ESP32-C3 |
| ESP32-C6 | RISC-V | vendor/espressif/boards/esp32c6/ | 乐鑫 ESP32-C6 |
| STM32F4 | ARM Cortex-M4 | vendor/stm32/boards/stm32f4/ | STMicroelectronics |
| STM32F7 | ARM Cortex-M7 | vendor/stm32/boards/stm32f7/ | STMicroelectronics |
| STM32H7 | ARM Cortex-M7 | vendor/stm32/boards/stm32h7/ | STMicroelectronics |
| NRF52 | ARM Cortex-M4 | vendor/nordic/boards/nrf52/ | Nordic Semiconductor |
| RP2040 | ARM Cortex-M0+ | vendor/raspberrypi/boards/rp2040/ | Raspberry Pi |
| QEMU ARM64 | ARM Cortex-A53 | vendor/openvela/boards/vela/configs/goldfish-arm64-v8a-ap/ | 模拟器 |
| QEMU ARM32 | ARM Cortex-A7 | vendor/openvela/boards/vela/configs/goldfish-armeabi-v7a-ap/ | 模拟器 |
| QEMU RISC-V | RISC-V | vendor/openvela/boards/vela/configs/goldfish-riscv64/ | 模拟器 |

---

## 注意事项

1. **NFS 性能调优**: 根据网络环境调整 NFS 参数
2. **Git LFS**: 大文件需要单独处理
3. **权限管理**: 确保所有开发机有正确的读写权限
4. **备份策略**: 定期备份裸仓库
5. **网络要求**: 开发机与 NFS 服务器之间需要稳定的网络连接

---

## 许可证

Apache-2.0
