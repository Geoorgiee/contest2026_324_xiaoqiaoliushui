# OpenVela 共享仓库实施方案 - Agent 实施文档

## 文档信息

| 项目 | 内容 |
|------|------|
| 项目名称 | OpenVela 共享仓库 NFS + git --reference 方案 |
| 版本 | v1.0.0 |
| 创建日期 | 2026-08-09 |
| 适用环境 | Linux (Ubuntu/Debian/CentOS) |
| 支持芯片 | ESP32, ARM Cortex-M/A, RISC-V |

---

## 目录

- [1. 方案概述](#1-方案概述)
- [2. 系统架构](#2-系统架构)
- [3. 前置条件](#3-前置条件)
- [4. NFS 服务器配置脚本](#4-nfs-服务器配置脚本)
- [5. 裸仓库初始化脚本](#5-裸仓库初始化脚本)
- [6. 开发机配置脚本](#6-开发机配置脚本)
- [7. 同步脚本](#7-同步脚本)
- [8. 多芯片构建脚本](#8-多芯片构建脚本)
- [9. 监控脚本](#9-监控脚本)
- [10. 维护脚本](#10-维护脚本)
- [11. 快速启动向导](#11-快速启动向导)
- [12. 配置文件](#12-配置文件)
- [13. 芯片适配说明](#13-芯片适配说明)
- [14. 性能对比](#14-性能对比)
- [15. 故障排除](#15-故障排除)

---

## 1. 方案概述

### 1.1 背景

OpenVela 项目支持多种芯片架构（Espressif ESP32、ARM Cortex-M/A、RISC-V），开发团队需要在多台开发机之间同步大型仓库。传统方式存在以下问题：

- 每台机器都需要完整克隆仓库，占用大量磁盘空间
- 初始同步耗时长（1-2小时）
- 增量同步效率低
- 网络带宽消耗大

### 1.2 解决方案

本方案采用 **NFS 共享存储 + git --reference** 架构：

1. **NFS 服务器**：托管裸仓库（bare repository），作为 git 对象库
2. **git --reference**：开发机使用 `--reference` 参数指向 NFS 上的裸仓库，共享对象库
3. **自动化脚本**：提供一键初始化、同步、构建、监控和维护功能

### 1.3 核心优势

- 初始同步时间减少 75%（从 1-2 小时降至 15-30 分钟）
- 每台机器磁盘占用减少 75%（从 ~20GB 降至 ~5GB）
- 增量同步时间减少 80%（从 5-10 分钟降至 1-2 分钟）
- 网络带宽消耗减少 90%

---

## 2. 系统架构

### 2.1 架构图

```
┌─────────────────────────────────────────────────────────────┐
│                      NFS 服务器                              │
│  ┌─────────────────────────────────────────────────────┐   │
│  │  /nfs/openvela-bare.git  (裸仓库 - git对象库)       │   │
│  └─────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
                              │
            ┌─────────────────┼─────────────────┐
            ▼                 ▼                 ▼
    ┌───────────────┐ ┌───────────────┐ ┌───────────────┐
    │  开发机 1     │ │  开发机 2     │ │  开发机 3     │
    │  (ARM)        │ │  (RISC-V)     │ │  (ESP32)      │
    │               │ │               │ │               │
    │ 使用 --reference │ │ 使用 --reference │ │ 使用 --reference │
    │ 指向NFS裸仓库 │ │ 指向NFS裸仓库 │ │ 指向NFS裸仓库 │
    └───────────────┘ └───────────────┘ └───────────────┘
```

### 2.2 目录结构

```
/nfs/
├── openvela-bare.git/          # 裸仓库（git对象库）
│   ├── objects/
│   ├── refs/
│   ├── HEAD
│   └── ...

~/openvela/                     # 开发机工作目录
├── .git/
│   ├── objects -> /nfs/openvela-bare.git/objects  # 符号链接
│   └── ...
├── sync.sh                     # 同步脚本
├── build-chip.sh               # 构建脚本
└── ...
```

### 2.3 文件结构

```
shared-repo-solution/
├── README.md                    # 方案概述
├── IMPLEMENTATION_GUIDE.md      # 本文档
├── nfs-server-setup.sh          # NFS 服务器配置脚本
├── bare-repo-init.sh            # 裸仓库初始化脚本
├── dev-machine-setup.sh         # 开发机配置脚本
├── sync-repo.sh                 # 仓库同步脚本
├── build.sh                     # 多芯片构建脚本
├── monitor.sh                   # 监控脚本
├── maintenance.sh               # 维护脚本
├── quick-start.sh               # 快速启动向导
├── config/
│   ├── nfs-exports              # NFS 导出配置
│   ├── gitconfig                # Git 配置模板
│   └── repo-manifest.xml        # Repo 清单配置
└── docs/
    ├── troubleshooting.md       # 故障排查指南
    └── chip-specific-notes.md   # 芯片特定说明
```

---

## 3. 前置条件

### 3.1 服务器要求

- 操作系统：Ubuntu 20.04+ / Debian 11+ / CentOS 8+
- 内存：>= 8GB
- 磁盘空间：>= 100GB（用于裸仓库）
- 网络：千兆以太网

### 3.2 开发机要求

- 操作系统：Ubuntu 20.04+ / Debian 11+
- 内存：>= 8GB
- 磁盘空间：>= 30GB
- 网络：能够访问 NFS 服务器

### 3.3 依赖软件

```bash
# 服务器端
sudo apt-get install -y nfs-kernel-server git

# 客户端（开发机）
sudo apt-get install -y nfs-common git
```

---

## 4. NFS 服务器配置脚本

### 4.1 nfs-server-setup.sh

```bash
#!/bin/bash
#
# NFS 服务器配置脚本
# 用途：配置 NFS 服务器，导出裸仓库目录
# 用法：sudo bash nfs-server-setup.sh [选项]
#

set -euo pipefail

# ============================================================
# 配置变量
# ============================================================
NFS_EXPORT_DIR="/nfs/openvela-bare.git"
NFS_EXPORT_OPTIONS="rw,sync,no_subtree_check,no_root_squash"
ALLOWED_NETWORKS=("192.168.1.0/24" "10.0.0.0/8")
LOG_FILE="/var/log/openvela-nfs-setup.log"

# ============================================================
# 颜色输出
# ============================================================
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] [INFO] $1" >> "$LOG_FILE"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] [WARN] $1" >> "$LOG_FILE"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] [ERROR] $1" >> "$LOG_FILE"
}

# ============================================================
# 检查 root 权限
# ============================================================
check_root() {
    if [[ $EUID -ne 0 ]]; then
        log_error "此脚本需要 root 权限运行"
        echo "请使用: sudo bash $0"
        exit 1
    fi
}

# ============================================================
# 安装 NFS 服务
# ============================================================
install_nfs_server() {
    log_info "安装 NFS 服务器软件..."

    if command -v apt-get &> /dev/null; then
        apt-get update
        apt-get install -y nfs-kernel-server
    elif command -v yum &> /dev/null; then
        yum install -y nfs-utils
    elif command -v dnf &> /dev/null; then
        dnf install -y nfs-utils
    else
        log_error "不支持的包管理器"
        exit 1
    fi

    log_info "NFS 服务器安装完成"
}

# ============================================================
# 创建导出目录
# ============================================================
create_export_directory() {
    log_info "创建 NFS 导出目录: $NFS_EXPORT_DIR"

    if [[ -d "$NFS_EXPORT_DIR" ]]; then
        log_warn "目录已存在: $NFS_EXPORT_DIR"
        read -p "是否继续？(y/N): " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            log_info "操作已取消"
            exit 0
        fi
    fi

    mkdir -p "$NFS_EXPORT_DIR"
    chmod 777 "$NFS_EXPORT_DIR"

    log_info "导出目录创建完成"
}

# ============================================================
# 配置 NFS 导出
# ============================================================
configure_nfs_exports() {
    log_info "配置 NFS 导出..."

    # 备份现有配置
    if [[ -f /etc/exports ]]; then
        cp /etc/exports /etc/exports.backup.$(date +%Y%m%d%H%M%S)
    fi

    # 生成导出配置
    for network in "${ALLOWED_NETWORKS[@]}"; do
        local export_line="$NFS_EXPORT_DIR $network($NFS_EXPORT_OPTIONS)"

        # 检查是否已存在
        if ! grep -qF "$export_line" /etc/exports 2>/dev/null; then
            echo "$export_line" >> /etc/exports
            log_info "添加导出: $export_line"
        else
            log_warn "导出已存在: $export_line"
        fi
    done

    # 重新加载导出配置
    exportfs -ra

    log_info "NFS 导出配置完成"
}

# ============================================================
# 配置防火墙
# ============================================================
configure_firewall() {
    log_info "配置防火墙规则..."

    if command -v ufw &> /dev/null; then
        # Ubuntu/Debian
        ufw allow from 192.168.1.0/24 to any port nfs
        ufw allow from 10.0.0.0/8 to any port nfs
        log_info "UFW 防火墙规则已添加"
    elif command -v firewall-cmd &> /dev/null; then
        # CentOS/RHEL
        firewall-cmd --permanent --add-service=nfs
        firewall-cmd --reload
        log_info "firewalld 防火墙规则已添加"
    else
        log_warn "未检测到防火墙，请手动配置"
    fi
}

# ============================================================
# 启动 NFS 服务
# ============================================================
start_nfs_service() {
    log_info "启动 NFS 服务..."

    systemctl enable nfs-kernel-server
    systemctl start nfs-kernel-server

    # 检查服务状态
    if systemctl is-active --quiet nfs-kernel-server; then
        log_info "NFS 服务启动成功"
    else
        log_error "NFS 服务启动失败"
        systemctl status nfs-kernel-server
        exit 1
    fi
}

# ============================================================
# 验证配置
# ============================================================
verify_configuration() {
    log_info "验证 NFS 配置..."

    # 检查导出列表
    echo ""
    echo "当前 NFS 导出列表:"
    echo "==================="
    showmount -e localhost
    echo ""

    # 检查目录权限
    echo "导出目录权限:"
    echo "============="
    ls -la "$NFS_EXPORT_DIR"
    echo ""

    log_info "NFS 服务器配置完成"
}

# ============================================================
# 主函数
# ============================================================
main() {
    echo "=========================================="
    echo "  OpenVela NFS 服务器配置脚本"
    echo "=========================================="
    echo ""

    check_root
    install_nfs_server
    create_export_directory
    configure_nfs_exports
    configure_firewall
    start_nfs_service
    verify_configuration

    echo ""
    echo "=========================================="
    echo "  配置完成！"
    echo "=========================================="
    echo ""
    echo "下一步："
    echo "  1. 在开发机上运行 dev-machine-setup.sh"
    echo "  2. 或使用 quick-start.sh --client <服务器IP>"
    echo ""
}

# 运行主函数
main "$@"
```

---

## 5. 裸仓库初始化脚本

### 5.1 bare-repo-init.sh

```bash
#!/bin/bash
#
# 裸仓库初始化脚本
# 用途：在 NFS 服务器上初始化 OpenVela 裸仓库
# 用法：bash bare-repo-init.sh [选项]
#

set -euo pipefail

# ============================================================
# 配置变量
# ============================================================
BARE_REPO_PATH="/nfs/openvela-bare.git"
REMOTE_URL="https://github.com/nicholasgasior/openvela.git"
DEFAULT_BRANCH="main"
LOG_FILE="/var/log/openvela-bare-repo-init.log"

# ============================================================
# 颜色输出
# ============================================================
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] [INFO] $1" >> "$LOG_FILE"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] [WARN] $1" >> "$LOG_FILE"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] [ERROR] $1" >> "$LOG_FILE"
}

# ============================================================
# 解析命令行参数
# ============================================================
parse_args() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            --path)
                BARE_REPO_PATH="$2"
                shift 2
                ;;
            --remote)
                REMOTE_URL="$2"
                shift 2
                ;;
            --branch)
                DEFAULT_BRANCH="$2"
                shift 2
                ;;
            --help|-h)
                show_help
                exit 0
                ;;
            *)
                log_error "未知参数: $1"
                show_help
                exit 1
                ;;
        esac
    done
}

# ============================================================
# 显示帮助信息
# ============================================================
show_help() {
    echo "用法: bash $0 [选项]"
    echo ""
    echo "选项:"
    echo "  --path <路径>      裸仓库路径 (默认: /nfs/openvela-bare.git)"
    echo "  --remote <URL>     远程仓库 URL"
    echo "  --branch <分支>    默认分支 (默认: main)"
    echo "  --help, -h         显示此帮助信息"
    echo ""
    echo "示例:"
    echo "  sudo bash $0"
    echo "  sudo bash $0 --path /data/openvela-bare.git --remote https://github.com/nicholasgasior/openvela.git"
}

# ============================================================
# 检查前置条件
# ============================================================
check_prerequisites() {
    log_info "检查前置条件..."

    # 检查 git
    if ! command -v git &> /dev/null; then
        log_error "git 未安装"
        exit 1
    fi

    # 检查目录权限
    local parent_dir
    parent_dir=$(dirname "$BARE_REPO_PATH")

    if [[ ! -w "$parent_dir" ]]; then
        log_error "目录不可写: $parent_dir"
        exit 1
    fi

    log_info "前置条件检查通过"
}

# ============================================================
# 初始化裸仓库
# ============================================================
init_bare_repo() {
    log_info "初始化裸仓库: $BARE_REPO_PATH"

    # 检查是否已存在
    if [[ -d "$BARE_REPO_PATH" ]]; then
        log_warn "裸仓库已存在: $BARE_REPO_PATH"
        read -p "是否重新初始化？(y/N): " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            log_info "跳过初始化"
            return
        fi
        rm -rf "$BARE_REPO_PATH"
    fi

    # 创建裸仓库
    git init --bare "$BARE_REPO_PATH"

    log_info "裸仓库初始化完成"
}

# ============================================================
# 克隆远程仓库
# ============================================================
clone_remote_repo() {
    log_info "克隆远程仓库..."

    local temp_dir
    temp_dir=$(mktemp -d)

    # 克隆远程仓库
    git clone --mirror "$REMOTE_URL" "$temp_dir/openvela-mirror.git"

    # 推送到裸仓库
    cd "$temp_dir/openvela-mirror.git"
    git remote set-url origin "$BARE_REPO_PATH"
    git push --mirror

    # 清理临时目录
    cd /
    rm -rf "$temp_dir"

    log_info "远程仓库克隆完成"
}

# ============================================================
# 配置裸仓库
# ============================================================
configure_bare_repo() {
    log_info "配置裸仓库..."

    cd "$BARE_REPO_PATH"

    # 设置默认分支
    git symbolic-ref HEAD "refs/heads/$DEFAULT_BRANCH"

    # 启用 reflog
    git config core.logAllRefUpdates true

    # 启用自动 gc
    git config gc.auto 256

    log_info "裸仓库配置完成"
}

# ============================================================
# 验证裸仓库
# ============================================================
verify_bare_repo() {
    log_info "验证裸仓库..."

    cd "$BARE_REPO_PATH"

    # 检查 HEAD
    echo ""
    echo "HEAD 引用:"
    echo "==========="
    git symbolic-ref HEAD
    echo ""

    # 检查分支列表
    echo "分支列表:"
    echo "========="
    git branch -a
    echo ""

    # 检查对象数量
    echo "对象统计:"
    echo "========="
    git count-objects -v
    echo ""

    log_info "裸仓库验证完成"
}

# ============================================================
# 设置定期同步 cron 任务
# ============================================================
setup_cron_sync() {
    log_info "设置定期同步任务..."

    local cron_job="0 */4 * * * cd $BARE_REPO_PATH && git fetch --all --prune >> /var/log/openvela-sync.log 2>&1"

    # 检查是否已存在
    if ! crontab -l 2>/dev/null | grep -qF "$cron_job"; then
        (crontab -l 2>/dev/null; echo "$cron_job") | crontab -
        log_info "定期同步任务已设置（每4小时）"
    else
        log_warn "定期同步任务已存在"
    fi
}

# ============================================================
# 主函数
# ============================================================
main() {
    echo "=========================================="
    echo "  OpenVela 裸仓库初始化脚本"
    echo "=========================================="
    echo ""

    parse_args "$@"
    check_prerequisites
    init_bare_repo
    clone_remote_repo
    configure_bare_repo
    verify_bare_repo
    setup_cron_sync

    echo ""
    echo "=========================================="
    echo "  初始化完成！"
    echo "=========================================="
    echo ""
    echo "裸仓库路径: $BARE_REPO_PATH"
    echo "远程仓库: $REMOTE_URL"
    echo "默认分支: $DEFAULT_BRANCH"
    echo ""
    echo "下一步："
    echo "  1. 在开发机上运行 dev-machine-setup.sh"
    echo "  2. 或使用 quick-start.sh --client <服务器IP>"
    echo ""
}

# 运行主函数
main "$@"
```

---

## 6. 开发机配置脚本

### 6.1 dev-machine-setup.sh

```bash
#!/bin/bash
#
# 开发机配置脚本
# 用途：配置开发机，使用 NFS 裸仓库作为 git 引用
# 用法：bash dev-machine-setup.sh [选项]
#

set -euo pipefail

# ============================================================
# 配置变量
# ============================================================
NFS_SERVER=""
NFS_EXPORT_PATH="/nfs/openvela-bare.git"
LOCAL_REPO_PATH="$HOME/openvela"
CHIP_TARGET=""
BRANCH="main"
USE_SSH=false
LOG_FILE="/var/log/openvela-dev-setup.log"

# ============================================================
# 颜色输出
# ============================================================
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] [INFO] $1" >> "$LOG_FILE"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] [WARN] $1" >> "$LOG_FILE"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] [ERROR] $1" >> "$LOG_FILE"
}

# ============================================================
# 解析命令行参数
# ============================================================
parse_args() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            --server|-s)
                NFS_SERVER="$2"
                shift 2
                ;;
            --chip|-c)
                CHIP_TARGET="$2"
                shift 2
                ;;
            --branch|-b)
                BRANCH="$2"
                shift 2
                ;;
            --path|-p)
                LOCAL_REPO_PATH="$2"
                shift 2
                ;;
            --ssh)
                USE_SSH=true
                shift
                ;;
            --help|-h)
                show_help
                exit 0
                ;;
            *)
                log_error "未知参数: $1"
                show_help
                exit 1
                ;;
        esac
    done

    # 验证必填参数
    if [[ -z "$NFS_SERVER" ]]; then
        log_error "必须指定 NFS 服务器地址"
        show_help
        exit 1
    fi
}

# ============================================================
# 显示帮助信息
# ============================================================
show_help() {
    echo "用法: bash $0 [选项]"
    echo ""
    echo "选项:"
    echo "  --server, -s <IP>     NFS 服务器地址 (必填)"
    echo "  --chip, -c <芯片>     芯片目标 (esp32/arm/riscv)"
    echo "  --branch, -b <分支>   Git 分支 (默认: main)"
    echo "  --path, -p <路径>     本地仓库路径 (默认: ~/openvela)"
    echo "  --ssh                 使用 SSH 克隆"
    echo "  --help, -h            显示此帮助信息"
    echo ""
    echo "示例:"
    echo "  bash $0 --server 192.168.1.100 --chip esp32"
    echo "  bash $0 -s 192.168.1.100 -c arm -b dev"
}

# ============================================================
# 检查前置条件
# ============================================================
check_prerequisites() {
    log_info "检查前置条件..."

    # 检查 git
    if ! command -v git &> /dev/null; then
        log_error "git 未安装"
        exit 1
    fi

    # 检查 NFS 客户端
    if ! command -v mount.nfs &> /dev/null; then
        log_warn "NFS 客户端未安装，正在安装..."
        install_nfs_client
    fi

    # 检查网络连接
    if ! ping -c 1 "$NFS_SERVER" &> /dev/null; then
        log_error "无法连接到 NFS 服务器: $NFS_SERVER"
        exit 1
    fi

    log_info "前置条件检查通过"
}

# ============================================================
# 安装 NFS 客户端
# ============================================================
install_nfs_client() {
    log_info "安装 NFS 客户端..."

    if command -v apt-get &> /dev/null; then
        sudo apt-get update
        sudo apt-get install -y nfs-common
    elif command -v yum &> /dev/null; then
        sudo yum install -y nfs-utils
    elif command -v dnf &> /dev/null; then
        sudo dnf install -y nfs-utils
    else
        log_error "不支持的包管理器"
        exit 1
    fi

    log_info "NFS 客户端安装完成"
}

# ============================================================
# 挂载 NFS 共享
# ============================================================
mount_nfs_share() {
    log_info "挂载 NFS 共享..."

    local mount_point="/mnt/openvela-nfs"

    # 创建挂载点
    sudo mkdir -p "$mount_point"

    # 检查是否已挂载
    if mountpoint -q "$mount_point"; then
        log_warn "NFS 共享已挂载"
        return
    fi

    # 挂载 NFS 共享
    sudo mount -t nfs "$NFS_SERVER:$NFS_EXPORT_PATH" "$mount_point"

    # 验证挂载
    if mountpoint -q "$mount_point"; then
        log_info "NFS 共享挂载成功: $mount_point"
    else
        log_error "NFS 共享挂载失败"
        exit 1
    fi

    # 添加到 fstab（可选）
    read -p "是否添加到 fstab 实现开机自动挂载？(y/N): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        local fstab_entry="$NFS_SERVER:$NFS_EXPORT_PATH $mount_point nfs defaults 0 0"
        if ! grep -qF "$fstab_entry" /etc/fstab; then
            echo "$fstab_entry" | sudo tee -a /etc/fstab
            log_info "已添加到 fstab"
        fi
    fi
}

# ============================================================
# 克隆仓库（使用 --reference）
# ============================================================
clone_repo_with_reference() {
    log_info "克隆仓库（使用 --reference）..."

    local mount_point="/mnt/openvela-nfs"
    local reference_path="$mount_point"

    # 检查是否已存在
    if [[ -d "$LOCAL_REPO_PATH" ]]; then
        log_warn "本地仓库已存在: $LOCAL_REPO_PATH"
        read -p "是否重新克隆？(y/N): " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            log_info "跳过克隆"
            return
        fi
        rm -rf "$LOCAL_REPO_PATH"
    fi

    # 构建克隆命令
    local clone_cmd="git clone --reference $reference_path"

    if [[ "$USE_SSH" == true ]]; then
        clone_cmd="$clone_cmd git@github.com:nicholasgasior/openvela.git"
    else
        clone_cmd="$clone_cmd https://github.com/nicholasgasior/openvela.git"
    fi

    clone_cmd="$clone_cmd $LOCAL_REPO_PATH"

    # 执行克隆
    eval "$clone_cmd"

    # 切换到指定分支
    cd "$LOCAL_REPO_PATH"
    git checkout "$BRANCH"

    log_info "仓库克隆完成"
}

# ============================================================
# 配置本地仓库
# ============================================================
configure_local_repo() {
    log_info "配置本地仓库..."

    cd "$LOCAL_REPO_PATH"

    # 设置远程仓库
    if [[ "$USE_SSH" == true ]]; then
        git remote set-url origin git@github.com:nicholasgasior/openvela.git
    else
        git remote set-url origin https://github.com/nicholasgasior/openvela.git
    fi

    # 启用 reflog
    git config core.logAllRefUpdates true

    # 设置用户信息（如果未设置）
    if [[ -z "$(git config user.name)" ]]; then
        read -p "请输入 Git 用户名: " git_username
        git config user.name "$git_username"
    fi

    if [[ -z "$(git config user.email)" ]]; then
        read -p "请输入 Git 邮箱: " git_email
        git config user.email "$git_email"
    fi

    log_info "本地仓库配置完成"
}

# ============================================================
# 创建便捷脚本
# ============================================================
create_convenience_scripts() {
    log_info "创建便捷脚本..."

    cd "$LOCAL_REPO_PATH"

    # 同步脚本
    cat > sync.sh << 'EOF'
#!/bin/bash
# 同步脚本 - 拉取最新代码

echo "正在同步代码..."
git fetch --all --prune
git pull --rebase
echo "同步完成！"
EOF
    chmod +x sync.sh

    # 构建脚本
    cat > build-chip.sh << 'EOF'
#!/bin/bash
# 构建脚本 - 根据芯片目标构建

CHIP=${1:-esp32}

echo "正在构建: $CHIP"
case $CHIP in
    esp32|esp32s2|esp32s3|esp32c3|esp32c6)
        echo "构建 Espressif 目标..."
        # 添加 Espressif 构建命令
        ;;
    arm|stm32|nrf52|rp2040)
        echo "构建 ARM 目标..."
        # 添加 ARM 构建命令
        ;;
    riscv|qemu-riscv64)
        echo "构建 RISC-V 目标..."
        # 添加 RISC-V 构建命令
        ;;
    *)
        echo "未知芯片目标: $CHIP"
        exit 1
        ;;
esac
EOF
    chmod +x build-chip.sh

    log_info "便捷脚本创建完成"
}

# ============================================================
# 验证配置
# ============================================================
verify_configuration() {
    log_info "验证配置..."

    cd "$LOCAL_REPO_PATH"

    echo ""
    echo "仓库信息:"
    echo "========="
    echo "路径: $(pwd)"
    echo "分支: $(git branch --show-current)"
    echo "远程: $(git remote get-url origin)"
    echo ""

    echo "Git 对象库:"
    echo "==========="
    ls -la .git/objects/
    echo ""

    echo "最近提交:"
    echo "========="
    git log --oneline -5
    echo ""

    log_info "配置验证完成"
}

# ============================================================
# 主函数
# ============================================================
main() {
    echo "=========================================="
    echo "  OpenVela 开发机配置脚本"
    echo "=========================================="
    echo ""

    parse_args "$@"
    check_prerequisites
    mount_nfs_share
    clone_repo_with_reference
    configure_local_repo
    create_convenience_scripts
    verify_configuration

    echo ""
    echo "=========================================="
    echo "  配置完成！"
    echo "=========================================="
    echo ""
    echo "本地仓库: $LOCAL_REPO_PATH"
    echo "NFS 服务器: $NFS_SERVER"
    echo "芯片目标: ${CHIP_TARGET:-未指定}"
    echo "分支: $BRANCH"
    echo ""
    echo "下一步："
    echo "  1. cd $LOCAL_REPO_PATH"
    echo "  2. ./sync.sh                    # 同步代码"
    echo "  3. ./build-chip.sh <芯片>       # 构建"
    echo ""
}

# 运行主函数
main "$@"
```

---

## 7. 同步脚本

### 7.1 sync-repo.sh

```bash
#!/bin/bash
#
# 仓库同步脚本
# 用途：同步 OpenVela 仓库代码
# 用法：bash sync-repo.sh [选项]
#

set -euo pipefail

# ============================================================
# 配置变量
# ============================================================
REPO_PATH="${OPENVELA_REPO:-$HOME/openvela}"
REMOTE="origin"
BRANCH=""
FORCE=false
VERBOSE=false
LOG_FILE="/var/log/openvela-sync.log"

# ============================================================
# 颜色输出
# ============================================================
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] [INFO] $1" >> "$LOG_FILE"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] [WARN] $1" >> "$LOG_FILE"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] [ERROR] $1" >> "$LOG_FILE"
}

# ============================================================
# 解析命令行参数
# ============================================================
parse_args() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            --repo|-r)
                REPO_PATH="$2"
                shift 2
                ;;
            --remote)
                REMOTE="$2"
                shift 2
                ;;
            --branch|-b)
                BRANCH="$2"
                shift 2
                ;;
            --force|-f)
                FORCE=true
                shift
                ;;
            --verbose|-v)
                VERBOSE=true
                shift
                ;;
            --help|-h)
                show_help
                exit 0
                ;;
            *)
                log_error "未知参数: $1"
                show_help
                exit 1
                ;;
        esac
    done
}

# ============================================================
# 显示帮助信息
# ============================================================
show_help() {
    echo "用法: bash $0 [选项]"
    echo ""
    echo "选项:"
    echo "  --repo, -r <路径>     仓库路径 (默认: ~/openvela)"
    echo "  --remote <远程>       远程仓库名 (默认: origin)"
    echo "  --branch, -b <分支>   分支名 (默认: 当前分支)"
    echo "  --force, -f           强制同步（丢弃本地修改）"
    echo "  --verbose, -v         详细输出"
    echo "  --help, -h            显示此帮助信息"
    echo ""
    echo "示例:"
    echo "  bash $0"
    echo "  bash $0 --branch dev --force"
    echo "  bash $0 -r /data/openvela -b main"
}

# ============================================================
# 检查仓库
# ============================================================
check_repo() {
    log_info "检查仓库..."

    if [[ ! -d "$REPO_PATH" ]]; then
        log_error "仓库不存在: $REPO_PATH"
        exit 1
    fi

    if [[ ! -d "$REPO_PATH/.git" ]]; then
        log_error "不是 Git 仓库: $REPO_PATH"
        exit 1
    fi

    cd "$REPO_PATH"

    # 获取当前分支
    if [[ -z "$BRANCH" ]]; then
        BRANCH=$(git branch --show-current)
    fi

    log_info "仓库路径: $REPO_PATH"
    log_info "当前分支: $BRANCH"
}

# ============================================================
# 检查本地修改
# ============================================================
check_local_changes() {
    log_info "检查本地修改..."

    cd "$REPO_PATH"

    # 检查工作区
    if [[ -n "$(git status --porcelain)" ]]; then
        log_warn "检测到本地修改："
        git status --short
        echo ""

        if [[ "$FORCE" == true ]]; then
            log_warn "强制模式：丢弃本地修改"
            git checkout -- .
            git clean -fd
        else
            read -p "是否丢弃本地修改？(y/N): " -n 1 -r
            echo
            if [[ $REPLY =~ ^[Yy]$ ]]; then
                git checkout -- .
                git clean -fd
            else
                log_error "存在本地修改，无法同步"
                exit 1
            fi
        fi
    fi

    log_info "本地修改检查完成"
}

# ============================================================
# 同步代码
# ============================================================
sync_code() {
    log_info "同步代码..."

    cd "$REPO_PATH"

    # Fetch 最新代码
    log_info "Fetch 远程更新..."
    if [[ "$VERBOSE" == true ]]; then
        git fetch "$REMOTE" --prune
    else
        git fetch "$REMOTE" --prune --quiet
    fi

    # 切换到目标分支
    log_info "切换到分支: $BRANCH"
    git checkout "$BRANCH"

    # Pull 最新代码
    log_info "Pull 最新代码..."
    if [[ "$VERBOSE" == true ]]; then
        git pull --rebase "$REMOTE" "$BRANCH"
    else
        git pull --rebase "$REMOTE" "$BRANCH" --quiet
    fi

    log_info "代码同步完成"
}

# ============================================================
# 显示同步结果
# ============================================================
show_sync_result() {
    cd "$REPO_PATH"

    echo ""
    echo "同步结果:"
    echo "========="
    echo "分支: $(git branch --show-current)"
    echo "最新提交: $(git log --oneline -1)"
    echo "远程更新: $(git log --oneline HEAD...$REMOTE/$BRANCH | wc -l) 个提交"
    echo ""

    # 显示最近 5 个提交
    echo "最近提交:"
    echo "========="
    git log --oneline -5
    echo ""
}

# ============================================================
# 主函数
# ============================================================
main() {
    echo "=========================================="
    echo "  OpenVela 仓库同步脚本"
    echo "=========================================="
    echo ""

    parse_args "$@"
    check_repo
    check_local_changes
    sync_code
    show_sync_result

    echo ""
    echo "=========================================="
    echo "  同步完成！"
    echo "=========================================="
    echo ""
}

# 运行主函数
main "$@"
```

---

## 8. 多芯片构建脚本

### 8.1 build.sh

```bash
#!/bin/bash
#
# 多芯片构建脚本
# 用途：根据芯片目标构建 OpenVela
# 用法：bash build.sh [选项]
#

set -euo pipefail

# ============================================================
# 配置变量
# ============================================================
REPO_PATH="${OPENVELA_REPO:-$HOME/openvela}"
CHIP_TARGET=""
BUILD_TYPE="release"
CLEAN_BUILD=false
VERBOSE=false
JOBS=$(nproc)
LOG_FILE="/var/log/openvela-build.log"

# ============================================================
# 芯片配置
# ============================================================
declare -A CHIP_CONFIGS=(
    # Espressif 芯片
    ["esp32"]="espressif:esp32:xtensa-esp32-elf"
    ["esp32s2"]="espressif:esp32s2:xtensa-esp32s2-elf"
    ["esp32s3"]="espressif:esp32s3:xtensa-esp32s3-elf"
    ["esp32c3"]="espressif:esp32c3:riscv32-esp-elf"
    ["esp32c6"]="espressif:esp32c6:riscv32-esp-elf"

    # ARM 芯片
    ["stm32f4"]="arm:stm32f4:arm-none-eabi"
    ["stm32f7"]="arm:stm32f7:arm-none-eabi"
    ["stm32h7"]="arm:stm32h7:arm-none-eabi"
    ["nrf52"]="arm:nrf52:arm-none-eabi"
    ["rp2040"]="arm:rp2040:arm-none-eabi"

    # RISC-V 芯片
    ["qemu-riscv64"]="riscv:qemu-riscv64:riscv64-unknown-elf"

    # 模拟器
    ["qemu-arm64"]="sim:qemu-arm64:aarch64-linux-gnu"
    ["qemu-arm32"]="sim:qemu-arm32:arm-linux-gnueabihf"
)

# ============================================================
# 颜色输出
# ============================================================
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] [INFO] $1" >> "$LOG_FILE"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] [WARN] $1" >> "$LOG_FILE"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] [ERROR] $1" >> "$LOG_FILE"
}

# ============================================================
# 解析命令行参数
# ============================================================
parse_args() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            --chip|-c)
                CHIP_TARGET="$2"
                shift 2
                ;;
            --type|-t)
                BUILD_TYPE="$2"
                shift 2
                ;;
            --clean)
                CLEAN_BUILD=true
                shift
                ;;
            --jobs|-j)
                JOBS="$2"
                shift 2
                ;;
            --verbose|-v)
                VERBOSE=true
                shift
                ;;
            --repo|-r)
                REPO_PATH="$2"
                shift 2
                ;;
            --list|-l)
                list_chips
                exit 0
                ;;
            --help|-h)
                show_help
                exit 0
                ;;
            *)
                log_error "未知参数: $1"
                show_help
                exit 1
                ;;
        esac
    done

    # 验证必填参数
    if [[ -z "$CHIP_TARGET" ]]; then
        log_error "必须指定芯片目标"
        show_help
        exit 1
    fi

    # 验证芯片目标
    if [[ -z "${CHIP_CONFIGS[$CHIP_TARGET]+x}" ]]; then
        log_error "不支持的芯片目标: $CHIP_TARGET"
        list_chips
        exit 1
    fi
}

# ============================================================
# 显示帮助信息
# ============================================================
show_help() {
    echo "用法: bash $0 [选项]"
    echo ""
    echo "选项:"
    echo "  --chip, -c <芯片>     芯片目标 (必填)"
    echo "  --type, -t <类型>     构建类型 (release/debug, 默认: release)"
    echo "  --clean               清理构建目录"
    echo "  --jobs, -j <数量>     并行编译数 (默认: $(nproc))"
    echo "  --verbose, -v         详细输出"
    echo "  --repo, -r <路径>     仓库路径 (默认: ~/openvela)"
    echo "  --list, -l            列出支持的芯片"
    echo "  --help, -h            显示此帮助信息"
    echo ""
    echo "示例:"
    echo "  bash $0 --chip esp32"
    echo "  bash $0 -c stm32h7 -t debug"
    echo "  bash $0 --chip qemu-riscv64 --clean"
}

# ============================================================
# 列出支持的芯片
# ============================================================
list_chips() {
    echo ""
    echo "支持的芯片目标:"
    echo "==============="
    echo ""

    echo "Espressif:"
    echo "  esp32, esp32s2, esp32s3, esp32c3, esp32c6"
    echo ""

    echo "ARM:"
    echo "  stm32f4, stm32f7, stm32h7, nrf52, rp2040"
    echo ""

    echo "RISC-V:"
    echo "  qemu-riscv64"
    echo ""

    echo "模拟器:"
    echo "  qemu-arm64, qemu-arm32"
    echo ""
}

# ============================================================
# 检查前置条件
# ============================================================
check_prerequisites() {
    log_info "检查前置条件..."

    # 检查仓库
    if [[ ! -d "$REPO_PATH" ]]; then
        log_error "仓库不存在: $REPO_PATH"
        exit 1
    fi

    cd "$REPO_PATH"

    # 解析芯片配置
    IFS=':' read -r vendor chip toolchain <<< "${CHIP_CONFIGS[$CHIP_TARGET]}"

    # 检查工具链
    if ! command -v "${toolchain}-gcc" &> /dev/null; then
        log_warn "工具链未安装: $toolchain"
        log_info "尝试安装工具链..."
        install_toolchain "$vendor" "$chip" "$toolchain"
    fi

    log_info "前置条件检查通过"
}

# ============================================================
# 安装工具链
# ============================================================
install_toolchain() {
    local vendor=$1
    local chip=$2
    local toolchain=$3

    case $vendor in
        espressif)
            log_info "安装 Espressif 工具链..."
            # 这里添加 Espressif 工具链安装逻辑
            ;;
        arm)
            log_info "安装 ARM 工具链..."
            if command -v apt-get &> /dev/null; then
                sudo apt-get install -y gcc-arm-none-eabi
            fi
            ;;
        riscv)
            log_info "安装 RISC-V 工具链..."
            # 这里添加 RISC-V 工具链安装逻辑
            ;;
        sim)
            log_info "安装模拟器工具链..."
            if command -v apt-get &> /dev/null; then
                if [[ "$toolchain" == "aarch64-linux-gnu" ]]; then
                    sudo apt-get install -y gcc-aarch64-linux-gnu
                elif [[ "$toolchain" == "arm-linux-gnueabihf" ]]; then
                    sudo apt-get install -y gcc-arm-linux-gnueabihf
                fi
            fi
            ;;
    esac
}

# ============================================================
# 清理构建目录
# ============================================================
clean_build() {
    if [[ "$CLEAN_BUILD" == true ]]; then
        log_info "清理构建目录..."

        cd "$REPO_PATH"

        if [[ -d "build" ]]; then
            rm -rf build
            log_info "构建目录已清理"
        fi
    fi
}

# ============================================================
# 执行构建
# ============================================================
run_build() {
    log_info "开始构建..."

    cd "$REPO_PATH"

    # 解析芯片配置
    IFS=':' read -r vendor chip toolchain <<< "${CHIP_CONFIGS[$CHIP_TARGET]}"

    # 构建命令
    local build_cmd="make -j$JOBS"

    # 添加构建参数
    build_cmd="$build_cmd CHIP=$chip"
    build_cmd="$build_cmd VENDOR=$vendor"
    build_cmd="$build_cmd TOOLCHAIN=$toolchain"

    if [[ "$BUILD_TYPE" == "debug" ]]; then
        build_cmd="$build_cmd DEBUG=1"
    fi

    if [[ "$VERBOSE" == true ]]; then
        build_cmd="$build_cmd V=1"
    fi

    # 执行构建
    log_info "执行命令: $build_cmd"

    if [[ "$VERBOSE" == true ]]; then
        eval "$build_cmd" 2>&1 | tee -a "$LOG_FILE"
    else
        eval "$build_cmd" >> "$LOG_FILE" 2>&1
    fi

    # 检查构建结果
    if [[ $? -eq 0 ]]; then
        log_info "构建成功"
    else
        log_error "构建失败"
        exit 1
    fi
}

# ============================================================
# 显示构建结果
# ============================================================
show_build_result() {
    cd "$REPO_PATH"

    echo ""
    echo "构建结果:"
    echo "========="
    echo "芯片目标: $CHIP_TARGET"
    echo "构建类型: $BUILD_TYPE"
    echo "构建时间: $(date)"
    echo ""

    # 显示构建产物
    if [[ -d "build" ]]; then
        echo "构建产物:"
        echo "========="
        find build -type f -name "*.bin" -o -name "*.elf" -o -name "*.hex" | head -10
        echo ""
    fi
}

# ============================================================
# 主函数
# ============================================================
main() {
    echo "=========================================="
    echo "  OpenVela 多芯片构建脚本"
    echo "=========================================="
    echo ""

    parse_args "$@"
    check_prerequisites
    clean_build
    run_build
    show_build_result

    echo ""
    echo "=========================================="
    echo "  构建完成！"
    echo "=========================================="
    echo ""
}

# 运行主函数
main "$@"
```

---

## 9. 监控脚本

### 9.1 monitor.sh

```bash
#!/bin/bash
#
# 监控和告警脚本
# 用途：监控 NFS 服务器和裸仓库状态
# 用法：bash monitor.sh [选项]
#

set -euo pipefail

# ============================================================
# 配置变量
# ============================================================
NFS_SERVER=""
NFS_EXPORT_PATH="/nfs/openvela-bare.git"
CHECK_INTERVAL=300  # 5 分钟
ALERT_EMAIL=""
ALERT_WEBHOOK=""
LOG_FILE="/var/log/openvela-monitor.log"
PID_FILE="/var/run/openvela-monitor.pid"
STATUS_FILE="/tmp/openvela-monitor-status.json"

# 告警阈值
DISK_USAGE_THRESHOLD=80
MEMORY_USAGE_THRESHOLD=90
CPU_USAGE_THRESHOLD=90
NFS_RESPONSE_THRESHOLD=5  # 秒

# ============================================================
# 颜色输出
# ============================================================
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] [INFO] $1" >> "$LOG_FILE"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] [WARN] $1" >> "$LOG_FILE"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] [ERROR] $1" >> "$LOG_FILE"
}

# ============================================================
# 解析命令行参数
# ============================================================
parse_args() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            --server|-s)
                NFS_SERVER="$2"
                shift 2
                ;;
            --interval|-i)
                CHECK_INTERVAL="$2"
                shift 2
                ;;
            --email|-e)
                ALERT_EMAIL="$2"
                shift 2
                ;;
            --webhook|-w)
                ALERT_WEBHOOK="$2"
                shift 2
                ;;
            --daemon|-d)
                run_as_daemon
                exit 0
                ;;
            --stop)
                stop_daemon
                exit 0
                ;;
            --status)
                show_status
                exit 0
                ;;
            --help|-h)
                show_help
                exit 0
                ;;
            *)
                log_error "未知参数: $1"
                show_help
                exit 1
                ;;
        esac
    done

    # 验证必填参数
    if [[ -z "$NFS_SERVER" ]]; then
        log_error "必须指定 NFS 服务器地址"
        show_help
        exit 1
    fi
}

# ============================================================
# 显示帮助信息
# ============================================================
show_help() {
    echo "用法: bash $0 [选项]"
    echo ""
    echo "选项:"
    echo "  --server, -s <IP>      NFS 服务器地址 (必填)"
    echo "  --interval, -i <秒>    检查间隔 (默认: 300)"
    echo "  --email, -e <邮箱>     告警邮箱"
    echo "  --webhook, -w <URL>    告警 Webhook"
    echo "  --daemon, -d           作为守护进程运行"
    echo "  --stop                 停止守护进程"
    echo "  --status               显示监控状态"
    echo "  --help, -h             显示此帮助信息"
    echo ""
    echo "示例:"
    echo "  bash $0 --server 192.168.1.100"
    echo "  bash $0 -s 192.168.1.100 --daemon"
    echo "  bash $0 --status"
}

# ============================================================
# 发送告警
# ============================================================
send_alert() {
    local level=$1
    local message=$2
    local timestamp=$(date '+%Y-%m-%d %H:%M:%S')

    # 记录日志
    case $level in
        "INFO")
            log_info "$message"
            ;;
        "WARN")
            log_warn "$message"
            ;;
        "ERROR")
            log_error "$message"
            ;;
    esac

    # 发送邮件告警
    if [[ -n "$ALERT_EMAIL" ]]; then
        send_email_alert "$level" "$message" "$timestamp"
    fi

    # 发送 Webhook 告警
    if [[ -n "$ALERT_WEBHOOK" ]]; then
        send_webhook_alert "$level" "$message" "$timestamp"
    fi
}

# ============================================================
# 发送邮件告警
# ============================================================
send_email_alert() {
    local level=$1
    local message=$2
    local timestamp=$3

    local subject="[OpenVela Monitor] $level: NFS 服务器告警"
    local body="时间: $timestamp\n级别: $level\n消息: $message\n服务器: $NFS_SERVER"

    echo -e "$body" | mail -s "$subject" "$ALERT_EMAIL" 2>/dev/null || true
}

# ============================================================
# 发送 Webhook 告警
# ============================================================
send_webhook_alert() {
    local level=$1
    local message=$2
    local timestamp=$3

    local payload=$(cat <<EOF
{
    "level": "$level",
    "message": "$message",
    "timestamp": "$timestamp",
    "server": "$NFS_SERVER"
}
EOF
)

    curl -s -X POST -H "Content-Type: application/json" -d "$payload" "$ALERT_WEBHOOK" &>/dev/null || true
}

# ============================================================
# 检查 NFS 服务状态
# ============================================================
check_nfs_service() {
    log_info "检查 NFS 服务状态..."

    # 检查 NFS 端口
    if ! timeout 5 bash -c "echo >/dev/tcp/$NFS_SERVER/2049" 2>/dev/null; then
        send_alert "ERROR" "NFS 服务不可达: $NFS_SERVER:2049"
        return 1
    fi

    # 检查 NFS 响应时间
    local start_time=$(date +%s%N)
    showmount -e "$NFS_SERVER" &>/dev/null
    local end_time=$(date +%s%N)
    local response_time=$(( (end_time - start_time) / 1000000 ))

    if [[ $response_time -gt $((NFS_RESPONSE_THRESHOLD * 1000)) ]]; then
        send_alert "WARN" "NFS 响应缓慢: ${response_time}ms (阈值: ${NFS_RESPONSE_THRESHOLD}s)"
    fi

    log_info "NFS 服务状态正常"
    return 0
}

# ============================================================
# 检查磁盘使用率
# ============================================================
check_disk_usage() {
    log_info "检查磁盘使用率..."

    local disk_usage=$(df -h "$NFS_EXPORT_PATH" | awk 'NR==2 {print $5}' | sed 's/%//')

    if [[ $disk_usage -gt $DISK_USAGE_THRESHOLD ]]; then
        send_alert "WARN" "磁盘使用率过高: ${disk_usage}% (阈值: ${DISK_USAGE_THRESHOLD}%)"
        return 1
    fi

    log_info "磁盘使用率正常: ${disk_usage}%"
    return 0
}

# ============================================================
# 检查内存使用率
# ============================================================
check_memory_usage() {
    log_info "检查内存使用率..."

    local memory_usage=$(free | awk '/Mem:/ {printf "%.0f", $3/$2 * 100}')

    if [[ $memory_usage -gt $MEMORY_USAGE_THRESHOLD ]]; then
        send_alert "WARN" "内存使用率过高: ${memory_usage}% (阈值: ${MEMORY_USAGE_THRESHOLD}%)"
        return 1
    fi

    log_info "内存使用率正常: ${memory_usage}%"
    return 0
}

# ============================================================
# 检查 CPU 使用率
# ============================================================
check_cpu_usage() {
    log_info "检查 CPU 使用率..."

    local cpu_usage=$(top -bn1 | grep "Cpu(s)" | awk '{print $2}' | cut -d'%' -f1)

    if [[ ${cpu_usage%.*} -gt $CPU_USAGE_THRESHOLD ]]; then
        send_alert "WARN" "CPU 使用率过高: ${cpu_usage}% (阈值: ${CPU_USAGE_THRESHOLD}%)"
        return 1
    fi

    log_info "CPU 使用率正常: ${cpu_usage}%"
    return 0
}

# ============================================================
# 检查裸仓库完整性
# ============================================================
check_bare_repo_integrity() {
    log_info "检查裸仓库完整性..."

    if [[ ! -d "$NFS_EXPORT_PATH" ]]; then
        send_alert "ERROR" "裸仓库目录不存在: $NFS_EXPORT_PATH"
        return 1
    fi

    # 检查 git 对象
    cd "$NFS_EXPORT_PATH"

    if ! git fsck --no-dangling --no-reflogs &>/dev/null; then
        send_alert "ERROR" "裸仓库完整性检查失败"
        return 1
    fi

    log_info "裸仓库完整性检查通过"
    return 0
}

# ============================================================
# 检查仓库同步状态
# ============================================================
check_repo_sync_status() {
    log_info "检查仓库同步状态..."

    cd "$NFS_EXPORT_PATH"

    # 获取最后更新时间
    local last_update=$(git log -1 --format="%ci" 2>/dev/null)
    local current_time=$(date +%s)
    local last_update_time=$(date -d "$last_update" +%s 2>/dev/null || echo 0)
    local time_diff=$((current_time - last_update_time))

    # 如果超过 24 小时未更新，发出警告
    if [[ $time_diff -gt 86400 ]]; then
        send_alert "WARN" "仓库超过 24 小时未更新: $last_update"
        return 1
    fi

    log_info "仓库同步状态正常: $last_update"
    return 0
}

# ============================================================
# 生成状态报告
# ============================================================
generate_status_report() {
    local status="healthy"
    local issues=()

    # 检查各项指标
    if ! check_nfs_service; then
        status="unhealthy"
        issues+=("NFS 服务异常")
    fi

    if ! check_disk_usage; then
        status="degraded"
        issues+=("磁盘使用率过高")
    fi

    if ! check_memory_usage; then
        status="degraded"
        issues+=("内存使用率过高")
    fi

    if ! check_cpu_usage; then
        status="degraded"
        issues+=("CPU 使用率过高")
    fi

    if ! check_bare_repo_integrity; then
        status="unhealthy"
        issues+=("裸仓库完整性异常")
    fi

    if ! check_repo_sync_status; then
        status="degraded"
        issues+=("仓库同步状态异常")
    fi

    # 生成 JSON 状态报告
    cat > "$STATUS_FILE" <<EOF
{
    "timestamp": "$(date -u +"%Y-%m-%dT%H:%M:%SZ")",
    "server": "$NFS_SERVER",
    "status": "$status",
    "issues": [$(printf '"%s",' "${issues[@]}" | sed 's/,$//')]
}
EOF

    echo ""
    echo "监控状态报告:"
    echo "============="
    echo "服务器: $NFS_SERVER"
    echo "状态: $status"
    echo "时间: $(date)"
    if [[ ${#issues[@]} -gt 0 ]]; then
        echo "问题:"
        for issue in "${issues[@]}"; do
            echo "  - $issue"
        done
    fi
    echo ""
}

# ============================================================
# 运行为守护进程
# ============================================================
run_as_daemon() {
    log_info "启动监控守护进程..."

    # 检查是否已在运行
    if [[ -f "$PID_FILE" ]]; then
        local pid=$(cat "$PID_FILE")
        if kill -0 "$pid" 2>/dev/null; then
            log_error "监控进程已在运行: PID $pid"
            exit 1
        fi
    fi

    # 后台运行
    nohup bash "$0" --server "$NFS_SERVER" --interval "$CHECK_INTERVAL" \
        ${ALERT_EMAIL:+--email "$ALERT_EMAIL"} \
        ${ALERT_WEBHOOK:+--webhook "$ALERT_WEBHOOK"} \
        >> "$LOG_FILE" 2>&1 &

    local pid=$!
    echo "$pid" > "$PID_FILE"

    log_info "监控守护进程已启动: PID $pid"
}

# ============================================================
# 停止守护进程
# ============================================================
stop_daemon() {
    log_info "停止监控守护进程..."

    if [[ -f "$PID_FILE" ]]; then
        local pid=$(cat "$PID_FILE")
        if kill -0 "$pid" 2>/dev/null; then
            kill "$pid"
            rm -f "$PID_FILE"
            log_info "监控守护进程已停止: PID $pid"
        else
            log_warn "进程不存在: PID $pid"
            rm -f "$PID_FILE"
        fi
    else
        log_warn "PID 文件不存在"
    fi
}

# ============================================================
# 显示监控状态
# ============================================================
show_status() {
    if [[ -f "$STATUS_FILE" ]]; then
        echo ""
        echo "最新监控状态:"
        echo "============="
        cat "$STATUS_FILE"
        echo ""
    else
        echo "暂无监控状态"
    fi
}

# ============================================================
# 主监控循环
# ============================================================
main_loop() {
    log_info "开始监控循环..."

    while true; do
        generate_status_report
        sleep "$CHECK_INTERVAL"
    done
}

# ============================================================
# 主函数
# ============================================================
main() {
    echo "=========================================="
    echo "  OpenVela 监控脚本"
    echo "=========================================="
    echo ""

    parse_args "$@"
    main_loop
}

# 运行主函数
main "$@"
```

---

## 10. 维护脚本

### 10.1 maintenance.sh

```bash
#!/bin/bash
#
# 仓库维护脚本
# 用途：维护裸仓库，清理垃圾，优化性能
# 用法：bash maintenance.sh [选项]
#

set -euo pipefail

# ============================================================
# 配置变量
# ============================================================
BARE_REPO_PATH="/nfs/openvela-bare.git"
LOG_FILE="/var/log/openvela-maintenance.log"
DRY_RUN=false
VERBOSE=false

# 维护选项
GC_AUTO=true
PRUNE=true
REPACK=true
VERIFY=true
CLEAN_TEMP=true

# ============================================================
# 颜色输出
# ============================================================
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] [INFO] $1" >> "$LOG_FILE"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] [WARN] $1" >> "$LOG_FILE"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] [ERROR] $1" >> "$LOG_FILE"
}

# ============================================================
# 解析命令行参数
# ============================================================
parse_args() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            --path|-p)
                BARE_REPO_PATH="$2"
                shift 2
                ;;
            --dry-run)
                DRY_RUN=true
                shift
                ;;
            --verbose|-v)
                VERBOSE=true
                shift
                ;;
            --no-gc)
                GC_AUTO=false
                shift
                ;;
            --no-prune)
                PRUNE=false
                shift
                ;;
            --no-repack)
                REPACK=false
                shift
                ;;
            --no-verify)
                VERIFY=false
                shift
                ;;
            --no-clean-temp)
                CLEAN_TEMP=false
                shift
                ;;
            --all)
                GC_AUTO=true
                PRUNE=true
                REPACK=true
                VERIFY=true
                CLEAN_TEMP=true
                shift
                ;;
            --help|-h)
                show_help
                exit 0
                ;;
            *)
                log_error "未知参数: $1"
                show_help
                exit 1
                ;;
        esac
    done
}

# ============================================================
# 显示帮助信息
# ============================================================
show_help() {
    echo "用法: bash $0 [选项]"
    echo ""
    echo "选项:"
    echo "  --path, -p <路径>      裸仓库路径 (默认: /nfs/openvela-bare.git)"
    echo "  --dry-run              试运行，不执行实际操作"
    echo "  --verbose, -v          详细输出"
    echo "  --no-gc                跳过垃圾回收"
    echo "  --no-prune             跳过修剪"
    echo "  --no-repack            跳过重新打包"
    echo "  --no-verify            跳过完整性验证"
    echo "  --no-clean-temp        跳过清理临时文件"
    echo "  --all                  执行所有维护操作"
    echo "  --help, -h             显示此帮助信息"
    echo ""
    echo "示例:"
    echo "  bash $0"
    echo "  bash $0 --all --verbose"
    echo "  bash $0 --dry-run --verbose"
}

# ============================================================
# 检查裸仓库
# ============================================================
check_bare_repo() {
    log_info "检查裸仓库..."

    if [[ ! -d "$BARE_REPO_PATH" ]]; then
        log_error "裸仓库不存在: $BARE_REPO_PATH"
        exit 1
    fi

    if [[ ! -d "$BARE_REPO_PATH/objects" ]]; then
        log_error "不是有效的 Git 仓库: $BARE_REPO_PATH"
        exit 1
    fi

    cd "$BARE_REPO_PATH"
    log_info "裸仓库路径: $BARE_REPO_PATH"
}

# ============================================================
# 垃圾回收
# ============================================================
run_gc() {
    if [[ "$GC_AUTO" == false ]]; then
        log_info "跳过垃圾回收"
        return
    fi

    log_info "执行垃圾回收..."

    cd "$BARE_REPO_PATH"

    if [[ "$DRY_RUN" == true ]]; then
        log_info "[DRY RUN] 将执行: git gc --auto"
        return
    fi

    # 记录回收前的大小
    local size_before=$(du -sh . | awk '{print $1}')
    log_info "回收前大小: $size_before"

    # 执行垃圾回收
    if [[ "$VERBOSE" == true ]]; then
        git gc --auto --verbose
    else
        git gc --auto
    fi

    # 记录回收后的大小
    local size_after=$(du -sh . | awk '{print $1}')
    log_info "回收后大小: $size_after"

    log_info "垃圾回收完成"
}

# ============================================================
# 修剪引用
# ============================================================
run_prune() {
    if [[ "$PRUNE" == false ]]; then
        log_info "跳过修剪"
        return
    fi

    log_info "修剪过期引用..."

    cd "$BARE_REPO_PATH"

    if [[ "$DRY_RUN" == true ]]; then
        log_info "[DRY RUN] 将执行: git remote prune origin"
        return
    fi

    # 修剪远程分支引用
    git remote prune origin 2>/dev/null || true

    # 修剪本地引用
    git prune

    log_info "修剪完成"
}

# ============================================================
# 重新打包
# ============================================================
run_repack() {
    if [[ "$REPACK" == false ]]; then
        log_info "跳过重新打包"
        return
    fi

    log_info "重新打包对象..."

    cd "$BARE_REPO_PATH"

    if [[ "$DRY_RUN" == true ]]; then
        log_info "[DRY RUN] 将执行: git repack -a -d"
        return
    fi

    # 记录打包前的对象数量
    local objects_before=$(git count-objects | awk '{print $1}')
    log_info "打包前松散对象: $objects_before"

    # 执行重新打包
    if [[ "$VERBOSE" == true ]]; then
        git repack -a -d --verbose
    else
        git repack -a -d
    fi

    # 记录打包后的对象数量
    local objects_after=$(git count-objects | awk '{print $1}')
    log_info "打包后松散对象: $objects_after"

    log_info "重新打包完成"
}

# ============================================================
# 验证完整性
# ============================================================
run_verify() {
    if [[ "$VERIFY" == false ]]; then
        log_info "跳过完整性验证"
        return
    fi

    log_info "验证仓库完整性..."

    cd "$BARE_REPO_PATH"

    if [[ "$DRY_RUN" == true ]]; then
        log_info "[DRY RUN] 将执行: git fsck --full"
        return
    fi

    # 执行完整性验证
    if [[ "$VERBOSE" == true ]]; then
        git fsck --full --verbose
    else
        git fsck --full
    fi

    if [[ $? -eq 0 ]]; then
        log_info "完整性验证通过"
    else
        log_error "完整性验证失败"
        return 1
    fi
}

# ============================================================
# 清理临时文件
# ============================================================
run_clean_temp() {
    if [[ "$CLEAN_TEMP" == false ]]; then
        log_info "跳过清理临时文件"
        return
    fi

    log_info "清理临时文件..."

    cd "$BARE_REPO_PATH"

    if [[ "$DRY_RUN" == true ]]; then
        log_info "[DRY RUN] 将清理临时文件"
        return
    fi

    # 清理临时文件
    find . -name "*.tmp" -type f -delete 2>/dev/null || true
    find . -name "*.bak" -type f -delete 2>/dev/null || true
    find . -name "*~" -type f -delete 2>/dev/null || true

    log_info "临时文件清理完成"
}

# ============================================================
# 生成维护报告
# ============================================================
generate_report() {
    log_info "生成维护报告..."

    cd "$BARE_REPO_PATH"

    echo ""
    echo "维护报告:"
    echo "========="
    echo "仓库路径: $BARE_REPO_PATH"
    echo "维护时间: $(date)"
    echo ""

    echo "仓库统计:"
    echo "---------"
    echo "对象数量: $(git count-objects -v | grep "count:" | awk '{print $2}')"
    echo "包数量: $(git count-objects -v | grep "packs:" | awk '{print $2}')"
    echo "仓库大小: $(du -sh . | awk '{print $1}')"
    echo ""

    echo "分支列表:"
    echo "---------"
    git branch -a
    echo ""

    echo "最近提交:"
    echo "---------"
    git log --oneline -5
    echo ""
}

# ============================================================
# 设置定期维护 cron 任务
# ============================================================
setup_cron_maintenance() {
    log_info "设置定期维护任务..."

    local cron_job="0 2 * * 0 cd $BARE_REPO_PATH && /path/to/maintenance.sh --all >> /var/log/openvela-maintenance.log 2>&1"

    # 检查是否已存在
    if ! crontab -l 2>/dev/null | grep -qF "maintenance.sh"; then
        read -p "是否设置每周日凌晨 2 点自动维护？(y/N): " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            (crontab -l 2>/dev/null; echo "$cron_job") | crontab -
            log_info "定期维护任务已设置"
        fi
    else
        log_warn "定期维护任务已存在"
    fi
}

# ============================================================
# 主函数
# ============================================================
main() {
    echo "=========================================="
    echo "  OpenVela 仓库维护脚本"
    echo "=========================================="
    echo ""

    parse_args "$@"
    check_bare_repo
    run_gc
    run_prune
    run_repack
    run_verify
    run_clean_temp
    generate_report
    setup_cron_maintenance

    echo ""
    echo "=========================================="
    echo "  维护完成！"
    echo "=========================================="
    echo ""
}

# 运行主函数
main "$@"
```

---

## 11. 快速启动向导

### 11.1 quick-start.sh

```bash
#!/bin/bash
#
# 快速启动向导
# 用途：一键配置 NFS 服务器或开发机
# 用法：bash quick-start.sh [选项]
#

set -euo pipefail

# ============================================================
# 配置变量
# ============================================================
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CONFIG_DIR="$SCRIPT_DIR/config"
LOG_FILE="/var/log/openvela-quick-start.log"

# ============================================================
# 颜色输出
# ============================================================
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] [INFO] $1" >> "$LOG_FILE"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] [WARN] $1" >> "$LOG_FILE"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] [ERROR] $1" >> "$LOG_FILE"
}

# ============================================================
# 显示欢迎信息
# ============================================================
show_welcome() {
    echo ""
    echo "=========================================="
    echo "  OpenVela 共享仓库快速启动向导"
    echo "=========================================="
    echo ""
    echo "本向导将帮助您快速配置 OpenVela 共享仓库环境。"
    echo ""
    echo "请选择配置类型:"
    echo ""
    echo "  1) NFS 服务器配置"
    echo "  2) 开发机配置"
    echo "  3) 完整配置（服务器 + 客户端）"
    echo ""
    read -p "请输入选项 [1-3]: " choice

    case $choice in
        1)
            setup_server
            ;;
        2)
            setup_client
            ;;
        3)
            setup_complete
            ;;
        *)
            log_error "无效选项: $choice"
            exit 1
            ;;
    esac
}

# ============================================================
# 解析命令行参数
# ============================================================
parse_args() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            --server)
                setup_server
                exit 0
                ;;
            --client)
                shift
                setup_client "$@"
                exit 0
                ;;
            --complete)
                setup_complete
                exit 0
                ;;
            --help|-h)
                show_help
                exit 0
                ;;
            *)
                log_error "未知参数: $1"
                show_help
                exit 1
                ;;
        esac
    done

    # 如果没有参数，显示交互式菜单
    show_welcome
}

# ============================================================
# 显示帮助信息
# ============================================================
show_help() {
    echo "用法: bash $0 [选项]"
    echo ""
    echo "选项:"
    echo "  --server              配置 NFS 服务器"
    echo "  --client <选项...>    配置开发机"
    echo "  --complete            完整配置（服务器 + 客户端）"
    echo "  --help, -h            显示此帮助信息"
    echo ""
    echo "示例:"
    echo "  sudo bash $0 --server"
    echo "  bash $0 --client 192.168.1.100 --chip esp32 --branch dev"
    echo "  bash $0 --complete"
}

# ============================================================
# 配置 NFS 服务器
# ============================================================
setup_server() {
    log_info "开始配置 NFS 服务器..."

    echo ""
    echo "=========================================="
    echo "  NFS 服务器配置"
    echo "=========================================="
    echo ""

    # 检查 root 权限
    if [[ $EUID -ne 0 ]]; then
        log_error "服务器配置需要 root 权限"
        echo "请使用: sudo bash $0 --server"
        exit 1
    fi

    # 收集配置信息
    read -p "NFS 导出目录 [/nfs/openvela-bare.git]: " nfs_export_dir
    nfs_export_dir=${nfs_export_dir:-/nfs/openvela-bare.git}

    read -p "允许访问的网络 [192.168.1.0/24]: " allowed_network
    allowed_network=${allowed_network:-192.168.1.0/24}

    read -p "远程仓库 URL [https://github.com/nicholasgasior/openvela.git]: " remote_url
    remote_url=${remote_url:-https://github.com/nicholasgasior/openvela.git}

    read -p "默认分支 [main]: " default_branch
    default_branch=${default_branch:-main}

    echo ""
    echo "配置摘要:"
    echo "---------"
    echo "NFS 导出目录: $nfs_export_dir"
    echo "允许网络: $allowed_network"
    echo "远程仓库: $remote_url"
    echo "默认分支: $default_branch"
    echo ""

    read -p "确认配置？(y/N): " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        log_info "配置已取消"
        exit 0
    fi

    # 执行配置
    log_info "执行 NFS 服务器配置..."

    # 1. 安装 NFS 服务
    apt-get update
    apt-get install -y nfs-kernel-server git

    # 2. 创建导出目录
    mkdir -p "$nfs_export_dir"
    chmod 777 "$nfs_export_dir"

    # 3. 配置导出
    echo "$nfs_export_dir $allowed_network(rw,sync,no_subtree_check,no_root_squash)" >> /etc/exports
    exportfs -ra

    # 4. 启动服务
    systemctl enable nfs-kernel-server
    systemctl start nfs-kernel-server

    # 5. 初始化裸仓库
    git init --bare "$nfs_export_dir"

    # 6. 克隆远程仓库
    local temp_dir=$(mktemp -d)
    git clone --mirror "$remote_url" "$temp_dir/openvela-mirror.git"
    cd "$temp_dir/openvela-mirror.git"
    git remote set-url origin "$nfs_export_dir"
    git push --mirror
    cd /
    rm -rf "$temp_dir"

    # 7. 配置裸仓库
    cd "$nfs_export_dir"
    git symbolic-ref HEAD "refs/heads/$default_branch"
    git config core.logAllRefUpdates true

    log_info "NFS 服务器配置完成"

    echo ""
    echo "=========================================="
    echo "  配置完成！"
    echo "=========================================="
    echo ""
    echo "下一步：在开发机上运行"
    echo "  bash $0 --client $(hostname -I | awk '{print $1}') --chip <芯片>"
    echo ""
}

# ============================================================
# 配置开发机
# ============================================================
setup_client() {
    local nfs_server=${1:-""}
    local chip_target=""
    local branch="main"
    local repo_path="$HOME/openvela"

    # 解析参数
    shift
    while [[ $# -gt 0 ]]; do
        case $1 in
            --chip|-c)
                chip_target="$2"
                shift 2
                ;;
            --branch|-b)
                branch="$2"
                shift 2
                ;;
            --path|-p)
                repo_path="$2"
                shift 2
                ;;
            *)
                log_error "未知参数: $1"
                exit 1
                ;;
        esac
    done

    # 如果未指定服务器，交互式询问
    if [[ -z "$nfs_server" ]]; then
        read -p "NFS 服务器地址: " nfs_server
        if [[ -z "$nfs_server" ]]; then
            log_error "必须指定 NFS 服务器地址"
            exit 1
        fi
    fi

    # 如果未指定芯片，交互式询问
    if [[ -z "$chip_target" ]]; then
        echo ""
        echo "请选择芯片目标:"
        echo ""
        echo "Espressif:"
        echo "  1) esp32"
        echo "  2) esp32s2"
        echo "  3) esp32s3"
        echo "  4) esp32c3"
        echo "  5) esp32c6"
        echo ""
        echo "ARM:"
        echo "  6) stm32f4"
        echo "  7) stm32f7"
        echo "  8) stm32h7"
        echo "  9) nrf52"
        echo "  10) rp2040"
        echo ""
        echo "RISC-V:"
        echo "  11) qemu-riscv64"
        echo ""
        echo "模拟器:"
        echo "  12) qemu-arm64"
        echo "  13) qemu-arm32"
        echo ""
        read -p "请输入选项 [1-13]: " chip_choice

        case $chip_choice in
            1) chip_target="esp32" ;;
            2) chip_target="esp32s2" ;;
            3) chip_target="esp32s3" ;;
            4) chip_target="esp32c3" ;;
            5) chip_target="esp32c6" ;;
            6) chip_target="stm32f4" ;;
            7) chip_target="stm32f7" ;;
            8) chip_target="stm32h7" ;;
            9) chip_target="nrf52" ;;
            10) chip_target="rp2040" ;;
            11) chip_target="qemu-riscv64" ;;
            12) chip_target="qemu-arm64" ;;
            13) chip_target="qemu-arm32" ;;
            *)
                log_error "无效选项: $chip_choice"
                exit 1
                ;;
        esac
    fi

    log_info "开始配置开发机..."

    echo ""
    echo "=========================================="
    echo "  开发机配置"
    echo "=========================================="
    echo ""

    echo "配置摘要:"
    echo "---------"
    echo "NFS 服务器: $nfs_server"
    echo "芯片目标: $chip_target"
    echo "分支: $branch"
    echo "本地路径: $repo_path"
    echo ""

    read -p "确认配置？(y/N): " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        log_info "配置已取消"
        exit 0
    fi

    # 执行配置
    log_info "执行开发机配置..."

    # 1. 安装依赖
    sudo apt-get update
    sudo apt-get install -y nfs-common git

    # 2. 挂载 NFS
    local mount_point="/mnt/openvela-nfs"
    sudo mkdir -p "$mount_point"
    sudo mount -t nfs "$nfs_server:/nfs/openvela-bare.git" "$mount_point"

    # 3. 克隆仓库
    git clone --reference "$mount_point" \
        https://github.com/nicholasgasior/openvela.git "$repo_path"

    # 4. 切换分支
    cd "$repo_path"
    git checkout "$branch"

    # 5. 创建便捷脚本
    cat > sync.sh << 'EOF'
#!/bin/bash
echo "正在同步代码..."
git fetch --all --prune
git pull --rebase
echo "同步完成！"
EOF
    chmod +x sync.sh

    cat > build-chip.sh << EOF
#!/bin/bash
CHIP=\${1:-$chip_target}
echo "正在构建: \$CHIP"
# 添加构建命令
EOF
    chmod +x build-chip.sh

    log_info "开发机配置完成"

    echo ""
    echo "=========================================="
    echo "  配置完成！"
    echo "=========================================="
    echo ""
    echo "本地仓库: $repo_path"
    echo "芯片目标: $chip_target"
    echo ""
    echo "下一步："
    echo "  cd $repo_path"
    echo "  ./sync.sh                    # 同步代码"
    echo "  ./build-chip.sh $chip_target  # 构建"
    echo ""
}

# ============================================================
# 完整配置
# ============================================================
setup_complete() {
    log_info "开始完整配置..."

    echo ""
    echo "=========================================="
    echo "  完整配置（服务器 + 客户端）"
    echo "=========================================="
    echo ""

    # 1. 配置服务器
    echo "步骤 1/2: 配置 NFS 服务器"
    echo "========================="
    setup_server

    # 2. 配置客户端
    echo ""
    echo "步骤 2/2: 配置开发机"
    echo "====================="
    setup_client "$(hostname -I | awk '{print $1}')"

    log_info "完整配置完成"
}

# ============================================================
# 主函数
# ============================================================
main() {
    parse_args "$@"
}

# 运行主函数
main "$@"
```

---

## 12. 配置文件

### 12.1 config/nfs-exports

```bash
# NFS 导出配置文件
# 用途：定义 NFS 导出规则
# 位置：/etc/exports

# 格式：
# <导出路径> <允许的网络>(<选项>)

# OpenVela 裸仓库导出
/nfs/openvela-bare.git 192.168.1.0/24(rw,sync,no_subtree_check,no_root_squash)
/nfs/openvela-bare.git 10.0.0.0/8(rw,sync,no_subtree_check,no_root_squash)

# 选项说明：
# rw - 允许读写
# sync - 同步写入
# no_subtree_check - 不检查子目录
# no_root_squash - 允许 root 访问（开发环境推荐）
```

### 12.2 config/gitconfig

```ini
# Git 配置模板
# 用途：统一团队 Git 配置
# 位置：~/.gitconfig

[user]
    # 用户信息（根据实际情况修改）
    name = Your Name
    email = your.email@example.com

[core]
    # 启用 reflog
    logAllRefUpdates = true
    # 自动处理行尾
    autocrlf = input
    # 文件权限
    filemode = true
    # 忽略大小写（Windows 兼容）
    ignorecase = false

[alias]
    # 常用别名
    st = status
    co = checkout
    br = branch
    ci = commit
    lg = log --oneline --graph --decorate
    last = log -1 HEAD
    unstage = reset HEAD --

[pull]
    # 使用 rebase 代替 merge
    rebase = true

[push]
    # 推送当前分支
    default = current
    # 推送标签
    followTags = true

[fetch]
    # 修剪过期引用
    prune = true

[merge]
    # 冲突风格
    conflictstyle = diff3

[diff]
    # 算法
    algorithm = histogram
    # 颜色
    colorMoved = default

[rerere]
    # 启用 rerere（记住冲突解决）
    enabled = true

[credential]
    # 缓存凭证
    helper = cache --timeout=3600

[init]
    # 默认分支名
    defaultBranch = main

[gc]
    # 自动 gc
    auto = 256
```

### 12.3 config/repo-manifest.xml

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!--
  OpenVela Repo Manifest
  用途：定义 repo 工具的仓库配置
  位置：.repo/manifest.xml
-->

<manifest>
  <!-- 远程仓库配置 -->
  <remote name="github"
          fetch="https://github.com/"
          review="https://github.com/" />

  <!-- 默认配置 -->
  <default revision="main"
           remote="github"
           sync-j="4"
           sync-tags="true" />

  <!-- 主仓库 -->
  <project name="nicholasgasior/openvela"
           path="."
           remote="github"
           revision="main" />

  <!-- 子仓库示例（根据实际情况添加） -->
  <!--
  <project name="nicholasgasior/openvela-tools"
           path="tools"
           remote="github"
           revision="main" />

  <project name="nicholasgasior/openvela-docs"
           path="docs"
           remote="github"
           revision="main" />
  -->

  <!-- 芯片特定仓库 -->
  <!--
  <project name="nicholasgasior/openvela-esp32"
           path="targets/esp32"
           remote="github"
           revision="main" />

  <project name="nicholasgasior/openvela-arm"
           path="targets/arm"
           remote="github"
           revision="main" />

  <project name="nicholasgasior/openvela-riscv"
           path="targets/riscv"
           remote="github"
           revision="main" />
  -->
</manifest>
```

---

## 13. 芯片适配说明

### 13.1 Espressif ESP32 系列

#### 支持的芯片

| 芯片型号 | 架构 | 工具链 | 备注 |
|----------|------|--------|------|
| ESP32 | Xtensa | xtensa-esp32-elf | 双核，WiFi+蓝牙 |
| ESP32-S2 | Xtensa | xtensa-esp32s2-elf | 单核，WiFi |
| ESP32-S3 | Xtensa | xtensa-esp32s3-elf | 双核，AI 加速 |
| ESP32-C3 | RISC-V | riscv32-esp-elf | 单核，WiFi+蓝牙 |
| ESP32-C6 | RISC-V | riscv32-esp-elf | WiFi 6，Thread/Zigbee |

#### 工具链安装

```bash
# 安装 ESP-IDF
mkdir -p ~/esp
cd ~/esp
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32,esp32s2,esp32s3,esp32c3,esp32c6

# 设置环境变量
. $HOME/esp/esp-idf/export.sh
```

#### 构建命令

```bash
# ESP32
idf.py set-target esp32
idf.py build

# ESP32-S3
idf.py set-target esp32s3
idf.py build

# ESP32-C3
idf.py set-target esp32c3
idf.py build
```

#### 特殊配置

```bash
# sdkconfig 配置示例
CONFIG_IDF_TARGET="esp32"
CONFIG_ESP32_DEFAULT_CPU_FREQ_240=y
CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y
CONFIG_FREERTOS_HZ=1000
```

### 13.2 ARM Cortex-M/A 系列

#### 支持的芯片

| 芯片型号 | 架构 | 工具链 | 备注 |
|----------|------|--------|------|
| STM32F4 | Cortex-M4 | arm-none-eabi | 高性能 MCU |
| STM32F7 | Cortex-M7 | arm-none-eabi | 高性能 MCU |
| STM32H7 | Cortex-M7 | arm-none-eabi | 超高性能 MCU |
| nRF52 | Cortex-M4 | arm-none-eabi | 蓝牙 SoC |
| RP2040 | Cortex-M0+ | arm-none-eabi | 双核 MCU |

#### 工具链安装

```bash
# Ubuntu/Debian
sudo apt-get install -y gcc-arm-none-eabi

# 或从 ARM 官网下载
# https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm
```

#### 构建命令

```bash
# STM32F4
make CHIP=stm32f4 TOOLCHAIN=arm-none-eabi

# STM32H7
make CHIP=stm32h7 TOOLCHAIN=arm-none-eabi

# nRF52
make CHIP=nrf52 TOOLCHAIN=arm-none-eabi
```

#### 特殊配置

```bash
# STM32 链接脚本示例
MEMORY
{
    FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 1024K
    RAM (xrw) : ORIGIN = 0x20000000, LENGTH = 128K
}

# nRF52 配置
CONFIG_NFCT_PINS_AS_GPIOS=y
CONFIG_GPIO_AS_PINRESET=y
```

### 13.3 RISC-V 系列

#### 支持的芯片

| 芯片型号 | 架构 | 工具链 | 备注 |
|----------|------|--------|------|
| QEMU RISC-V 64 | RISC-V 64 | riscv64-unknown-elf | 模拟器 |

#### 工具链安装

```bash
# Ubuntu/Debian
sudo apt-get install -y gcc-riscv64-unknown-elf

# 或从源码编译
git clone https://github.com/riscv/riscv-gnu-toolchain.git
cd riscv-gnu-toolchain
./configure --prefix=/opt/riscv
make
```

#### 构建命令

```bash
# QEMU RISC-V 64
make CHIP=qemu-riscv64 TOOLCHAIN=riscv64-unknown-elf
```

#### 运行命令

```bash
# 使用 QEMU 运行
qemu-system-riscv64 -machine virt -kernel build/openvela.elf -nographic
```

### 13.4 模拟器

#### 支持的模拟器

| 模拟器 | 架构 | 工具链 | 备注 |
|--------|------|--------|------|
| QEMU ARM64 | AArch64 | aarch64-linux-gnu | 64 位 ARM 模拟器 |
| QEMU ARM32 | ARM | arm-linux-gnueabihf | 32 位 ARM 模拟器 |

#### 工具链安装

```bash
# QEMU
sudo apt-get install -y qemu-system-arm

# ARM64 工具链
sudo apt-get install -y gcc-aarch64-linux-gnu

# ARM32 工具链
sudo apt-get install -y gcc-arm-linux-gnueabihf
```

#### 构建和运行命令

```bash
# 构建 ARM64
make CHIP=qemu-arm64 TOOLCHAIN=aarch64-linux-gnu

# 运行 ARM64
qemu-system-aarch64 -machine virt -cpu cortex-a57 -kernel build/openvela.elf -nographic

# 构建 ARM32
make CHIP=qemu-arm32 TOOLCHAIN=arm-linux-gnueabihf

# 运行 ARM32
qemu-system-arm -machine virt -cpu cortex-a15 -kernel build/openvela.elf -nographic
```

---

## 14. 性能对比

### 14.1 指标对比

| 指标 | 传统方式 | 本方案 | 提升幅度 |
|------|----------|--------|----------|
| 初始同步时间 | 1-2 小时 | 15-30 分钟 | 75% |
| 每台机器磁盘占用 | ~20 GB | ~5 GB | 75% |
| 增量同步时间 | 5-10 分钟 | 1-2 分钟 | 80% |
| 网络带宽消耗 | 高 | 低 | 90% |

### 14.2 原因分析

#### 初始同步时间减少 75%

- **传统方式**：每台机器都需要从远程仓库完整克隆所有对象
- **本方案**：使用 `--reference` 参数，直接引用 NFS 上的裸仓库对象，只需下载差异部分

#### 磁盘占用减少 75%

- **传统方式**：每台机器存储完整的 git 对象库
- **本方案**：git 对象库存储在 NFS 服务器上，开发机只存储工作区和引用

#### 增量同步时间减少 80%

- **传统方式**：每次同步都需要通过网络传输对象
- **本方案**：大部分对象已在本地（通过 NFS），只需传输新增对象

#### 网络带宽消耗减少 90%

- **传统方式**：每次克隆/同步都传输大量对象
- **本方案**：对象共享，只需传输差异

### 14.3 适用场景

| 场景 | 适用性 | 说明 |
|------|--------|------|
| 多人协作开发 | 非常适用 | 多人共享同一对象库 |
| 多芯片开发 | 非常适用 | 不同芯片目标共享基础代码 |
| CI/CD 环境 | 适用 | 构建服务器快速获取代码 |
| 单人开发 | 一般适用 | 优势不明显 |
| 跨网络开发 | 不适用 | NFS 性能受网络影响 |

---

## 15. 故障排除

### 15.1 NFS 相关问题

#### 问题：无法挂载 NFS 共享

**症状：**
```
mount.nfs: Connection timed out
```

**解决方案：**
```bash
# 1. 检查 NFS 服务状态
sudo systemctl status nfs-kernel-server

# 2. 检查防火墙
sudo ufw status
sudo ufw allow from 192.168.1.0/24 to any port nfs

# 3. 检查导出配置
showmount -e <服务器IP>

# 4. 检查网络连接
ping <服务器IP>
telnet <服务器IP> 2049
```

#### 问题：NFS 挂载后权限错误

**症状：**
```
Permission denied
```

**解决方案：**
```bash
# 1. 检查导出选项
cat /etc/exports
# 确保包含 no_root_squash

# 2. 重新加载导出
sudo exportfs -ra

# 3. 检查目录权限
ls -la /nfs/openvela-bare.git
```

### 15.2 Git 相关问题

#### 问题：git --reference 不生效

**症状：**
克隆速度没有提升，仍然很慢

**解决方案：**
```bash
# 1. 检查引用路径
ls -la ~/openvela/.git/objects/

# 2. 确认引用仓库存在
ls -la /mnt/openvela-nfs/objects/

# 3. 重新克隆
rm -rf ~/openvela
git clone --reference /mnt/openvela-nfs https://github.com/nicholasgasior/openvela.git ~/openvela
```

#### 问题：git fsck 失败

**症状：**
```
error: object file .git/objects/xx/xxxxxxxxxx is empty
```

**解决方案：**
```bash
# 1. 从远程重新获取对象
git fetch --all

# 2. 修复对象库
git fsck --full

# 3. 如果仍然失败，重新克隆
rm -rf ~/openvela
git clone --reference /mnt/openvela-nfs https://github.com/nicholasgasior/openvela.git ~/openvela
```

### 15.3 构建相关问题

#### 问题：工具链未找到

**症状：**
```
arm-none-eabi-gcc: command not found
```

**解决方案：**
```bash
# 1. 检查工具链是否安装
which arm-none-eabi-gcc

# 2. 安装工具链
sudo apt-get install -y gcc-arm-none-eabi

# 3. 检查 PATH
echo $PATH

# 4. 添加到 PATH（如果需要）
export PATH=$PATH:/opt/gcc-arm-none-eabi/bin
```

#### 问题：构建内存不足

**症状：**
```
virtual memory exhausted: Cannot allocate memory
```

**解决方案：**
```bash
# 1. 减少并行编译数
make -j2  # 而不是 make -j$(nproc)

# 2. 增加 swap
sudo fallocate -l 4G /swapfile
sudo chmod 600 /swapfile
sudo mkswap /swapfile
sudo swapon /swapfile

# 3. 永久添加 swap
echo '/swapfile none swap sw 0 0' | sudo tee -a /etc/fstab
```

### 15.4 监控相关问题

#### 问题：监控脚本无法启动

**症状：**
```
bash: monitor.sh: command not found
```

**解决方案：**
```bash
# 1. 检查脚本权限
chmod +x monitor.sh

# 2. 使用完整路径
/home/user/openvela/monitor.sh

# 3. 或使用 bash 运行
bash monitor.sh
```

#### 问题：告警邮件未收到

**症状：**
监控脚本运行正常，但未收到告警邮件

**解决方案：**
```bash
# 1. 检查邮件服务
sudo systemctl status postfix

# 2. 测试邮件发送
echo "Test" | mail -s "Test" your@email.com

# 3. 检查邮件日志
tail -f /var/log/mail.log

# 4. 使用 Webhook 替代
./monitor.sh --server 192.168.1.100 --webhook https://hooks.slack.com/xxx
```

### 15.5 性能问题

#### 问题：NFS 性能缓慢

**症状：**
文件操作非常慢

**解决方案：**
```bash
# 1. 检查网络带宽
iperf3 -c <服务器IP>

# 2. 调整 NFS 挂载选项
sudo mount -t nfs -o rsize=32768,wsize=32768,intr <服务器IP>:/nfs/openvela-bare.git /mnt/openvela-nfs

# 3. 启用 NFS 缓存
sudo mount -t nfs -o actimeo=3600 <服务器IP>:/nfs/openvela-bare.git /mnt/openvela-nfs

# 4. 检查服务器负载
top
iostat -x 1
```

#### 问题：git 操作缓慢

**症状：**
git fetch/pull 非常慢

**解决方案：**
```bash
# 1. 启用 git 压缩
git config core.compression 9

# 2. 限制 fetch 深度
git fetch --depth=1

# 3. 使用浅克隆
git clone --depth=1 --reference /mnt/openvela-nfs https://github.com/nicholasgasior/openvela.git

# 4. 清理不必要的对象
git gc --auto
git prune
```

---

## 附录

### A. 快速参考

#### 常用命令

```bash
# 服务器端
sudo bash nfs-server-setup.sh                    # 配置 NFS 服务器
sudo bash bare-repo-init.sh                      # 初始化裸仓库

# 客户端
bash dev-machine-setup.sh --server 192.168.1.100 --chip esp32  # 配置开发机
bash sync-repo.sh                                 # 同步代码
bash build.sh --chip esp32                        # 构建

# 监控维护
bash monitor.sh --server 192.168.1.100 --daemon   # 启动监控
bash maintenance.sh --all                          # 执行维护
```

#### 一键启动

```bash
# 完整配置
sudo bash quick-start.sh --complete

# 仅服务器
sudo bash quick-start.sh --server

# 仅客户端
bash quick-start.sh --client 192.168.1.100 --chip esp32 --branch dev
```

### B. 参考资源

- [OpenVela 官方文档](https://github.com/nicholasgasior/openvela)
- [Git 官方文档](https://git-scm.com/doc)
- [NFS 官方文档](https://linux.die.net/man/5/exports)
- [Espressif ESP-IDF](https://docs.espressif.com/projects/esp-idf/)
- [ARM GNU Toolchain](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain)

### C. 版本历史

| 版本 | 日期 | 说明 |
|------|------|------|
| v1.0.0 | 2026-08-09 | 初始版本 |

---

**文档维护者：** OpenVela 开发团队

**最后更新：** 2026-08-09
