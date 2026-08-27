#!/bin/bash
# NFS 服务器配置脚本
# 用于配置 openvela 共享仓库的 NFS 服务器
# Usage: sudo bash nfs-server-setup.sh [options]
#
# Options:
#   --nfs-dir <path>       NFS 共享目录 (默认: /nfs/openvela)
#   --subnet <subnet>      允许访问的子网 (默认: 192.168.1.0/24)
#   --port <port>          NFS 端口 (默认: 2049)
#   --workers <num>        NFS 工作线程数 (默认: 8)

set -e

# 颜色定义
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

info() { echo -e "${GREEN}[INFO]${NC} $1"; }
warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
error() { echo -e "${RED}[ERROR]${NC} $1"; exit 1; }
debug() { echo -e "${BLUE}[DEBUG]${NC} $1"; }

# 默认配置
NFS_DIR="/nfs/openvela"
SUBNET="192.168.1.0/24"
NFS_PORT=2049
WORKERS=8
MOUNT_POINT="/mnt/openvela-bare"

# 解析命令行参数
while [[ $# -gt 0 ]]; do
    case $1 in
        --nfs-dir)
            NFS_DIR="$2"
            shift 2
            ;;
        --subnet)
            SUBNET="$2"
            shift 2
            ;;
        --port)
            NFS_PORT="$2"
            shift 2
            ;;
        --workers)
            WORKERS="$2"
            shift 2
            ;;
        --help|-h)
            echo "Usage: sudo bash nfs-server-setup.sh [options]"
            echo ""
            echo "Options:"
            echo "  --nfs-dir <path>       NFS 共享目录 (默认: /nfs/openvela)"
            echo "  --subnet <subnet>      允许访问的子网 (默认: 192.168.1.0/24)"
            echo "  --port <port>          NFS 端口 (默认: 2049)"
            echo "  --workers <num>        NFS 工作线程数 (默认: 8)"
            exit 0
            ;;
        *)
            error "未知参数: $1"
            ;;
    esac
done

# 检查是否以 root 权限运行
if [ "$EUID" -ne 0 ]; then
    error "请使用 sudo 运行此脚本"
fi

info "开始配置 NFS 服务器..."
info "NFS 目录: $NFS_DIR"
info "允许子网: $SUBNET"

# 检测操作系统
detect_os() {
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        OS=$ID
        OS_VERSION=$VERSION_ID
    else
        error "无法检测操作系统"
    fi
    info "检测到操作系统: $OS $OS_VERSION"
}

# 安装 NFS 服务
install_nfs() {
    info "安装 NFS 服务..."

    case $OS in
        ubuntu|debian)
            apt-get update
            apt-get install -y nfs-kernel-server nfs-common
            ;;
        centos|rhel|rocky|alma)
            yum install -y nfs-utils
            ;;
        fedora)
            dnf install -y nfs-utils
            ;;
        *)
            error "不支持的操作系统: $OS"
            ;;
    esac

    info "NFS 服务安装完成"
}

# 创建 NFS 目录
create_nfs_dir() {
    info "创建 NFS 目录..."

    mkdir -p "$NFS_DIR"
    chmod 755 "$NFS_DIR"

    # 创建子目录
    mkdir -p "$NFS_DIR/bare-repo"
    mkdir -p "$NFS_DIR/lfs-storage"
    mkdir -p "$NFS_DIR/logs"
    mkdir -p "$NFS_DIR/backup"

    # 设置权限
    chown -R nobody:nogroup "$NFS_DIR"
    chmod -R 777 "$NFS_DIR"

    info "NFS 目录创建完成"
}

# 配置 NFS 导出
configure_exports() {
    info "配置 NFS 导出..."

    # 备份原有配置
    if [ -f /etc/exports ]; then
        cp /etc/exports /etc/exports.backup.$(date +%Y%m%d%H%M%S)
    fi

    # 创建新的导出配置
    cat > /etc/exports << EOF
# openvela 共享仓库 NFS 导出配置
# 生成时间: $(date)

# 主共享目录
$NFS_DIR $SUBNET(rw,sync,no_subtree_check,no_root_squash,fsid=0)

# 裸仓库目录 (只读，用于 git --reference)
$NFS_DIR/bare-repo $SUBNET(ro,sync,no_subtree_check,no_root_squash)

# LFS 存储目录
$NFS_DIR/lfs-storage $SUBNET(rw,sync,no_subtree_check,no_root_squash)

# 日志目录
$NFS_DIR/logs $SUBNET(rw,sync,no_subtree_check,no_root_squash)

# 备份目录
$NFS_DIR/backup $SUBNET(rw,sync,no_subtree_check,no_root_squash)
EOF

    info "NFS 导出配置完成"
}

# 配置 NFS 服务
configure_nfs_service() {
    info "配置 NFS 服务..."

    # 配置 NFS 服务端口
    cat > /etc/default/nfs-kernel-server << EOF
# NFS 服务配置
RPCNFSDCOUNT=$WORKERS
RPCNFSDPRIORITY=0
RPCMOUNTDOPTS="--manage-gids"
NEED_SVCGSSD=""
SVCGSSD_OPTS=""
EOF

    # 配置 NFS 端口
    cat > /etc/sysctl.d/99-nfs.conf << EOF
# NFS 端口配置
fs.nfs.nlm_tcpport=$NFS_PORT
fs.nfs.nlm_udpport=$NFS_PORT
EOF

    # 应用 sysctl 配置
    sysctl -p /etc/sysctl.d/99-nfs.conf

    info "NFS 服务配置完成"
}

# 启动 NFS 服务
start_nfs_service() {
    info "启动 NFS 服务..."

    # 导出共享目录
    exportfs -ra

    # 启动 NFS 服务
    case $OS in
        ubuntu|debian)
            systemctl enable nfs-kernel-server
            systemctl restart nfs-kernel-server
            ;;
        centos|rhel|rocky|alma|fedora)
            systemctl enable nfs-server
            systemctl restart nfs-server
            ;;
    esac

    # 检查服务状态
    if systemctl is-active --quiet nfs-kernel-server || systemctl is-active --quiet nfs-server; then
        info "NFS 服务启动成功"
    else
        error "NFS 服务启动失败"
    fi
}

# 配置防火墙
configure_firewall() {
    info "配置防火墙..."

    # 检查防火墙状态
    if command -v ufw &> /dev/null; then
        # Ubuntu/Debian
        ufw allow from $SUBNET to any port $NFS_PORT
        ufw allow from $SUBNET to any port 111
        ufw allow from $SUBNET to any port 20048
        ufw reload
        info "UFW 防火墙配置完成"
    elif command -v firewall-cmd &> /dev/null; then
        # CentOS/RHEL
        firewall-cmd --permanent --add-service=nfs
        firewall-cmd --permanent --add-service=rpc-bind
        firewall-cmd --permanent --add-service=mountd
        firewall-cmd --reload
        info "Firewalld 防火墙配置完成"
    else
        warn "未检测到防火墙，请手动配置"
    fi
}

# 显示配置信息
show_config() {
    echo ""
    echo "=========================================="
    echo "  NFS 服务器配置完成"
    echo "=========================================="
    echo ""
    echo "NFS 共享目录: $NFS_DIR"
    echo "允许访问子网: $SUBNET"
    echo "NFS 端口: $NFS_PORT"
    echo "工作线程数: $WORKERS"
    echo ""
    echo "在开发机上挂载:"
    echo "  sudo mkdir -p /mnt/openvela-bare"
    echo "  sudo mount -t nfs $(hostname -I | awk '{print $1}'):$NFS_DIR /mnt/openvela-bare"
    echo ""
    echo "或使用自动挂载:"
    echo "  echo '$(hostname -I | awk '{print $1}'):$NFS_DIR /mnt/openvela-bare nfs defaults 0 0' | sudo tee -a /etc/fstab"
    echo ""
    echo "=========================================="
}

# 主流程
main() {
    info "=== openvela NFS 服务器配置 ==="

    detect_os
    install_nfs
    create_nfs_dir
    configure_exports
    configure_nfs_service
    start_nfs_service
    configure_firewall
    show_config

    info "=== NFS 服务器配置完成 ==="
}

# 执行主流程
main
