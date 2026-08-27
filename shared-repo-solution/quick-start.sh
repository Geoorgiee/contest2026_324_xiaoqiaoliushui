#!/bin/bash
# openvela 共享仓库快速开始脚本
# 一键配置 NFS 服务器和开发环境
# Usage: bash quick-start.sh [options]
#
# Options:
#   --server           配置为 NFS 服务器
#   --client <ip>      配置为开发机，连接到指定 NFS 服务器
#   --chip <chip>      芯片目标
#   --branch <branch>  分支 (dev|trunk)

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
MODE=""
NFS_SERVER_IP=""
CHIP="qemu-arm64"
BRANCH="dev"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# 解析命令行参数
while [[ $# -gt 0 ]]; do
    case $1 in
        --server)
            MODE="server"
            shift
            ;;
        --client)
            MODE="client"
            NFS_SERVER_IP="$2"
            shift 2
            ;;
        --chip)
            CHIP="$2"
            shift 2
            ;;
        --branch)
            BRANCH="$2"
            shift 2
            ;;
        --help|-h)
            echo "Usage: bash quick-start.sh [options]"
            echo ""
            echo "Options:"
            echo "  --server           配置为 NFS 服务器"
            echo "  --client <ip>      配置为开发机，连接到指定 NFS 服务器"
            echo "  --chip <chip>      芯片目标 (默认: qemu-arm64)"
            echo "  --branch <branch>  分支 (dev|trunk) (默认: dev)"
            echo ""
            echo "示例:"
            echo "  # 配置 NFS 服务器"
            echo "  sudo bash quick-start.sh --server"
            echo ""
            echo "  # 配置开发机"
            echo "  bash quick-start.sh --client 192.168.1.100 --chip esp32 --branch dev"
            exit 0
            ;;
        *)
            error "未知参数: $1"
            ;;
    esac
done

# 显示欢迎信息
show_welcome() {
    echo ""
    echo "=========================================="
    echo "  openvela 共享仓库快速开始"
    echo "=========================================="
    echo ""
    echo "本脚本将帮助您快速配置 openvela 共享仓库环境"
    echo ""
    echo "支持的模式:"
    echo "  --server     配置为 NFS 服务器"
    echo "  --client     配置为开发机"
    echo ""
    echo "支持的芯片:"
    echo "  esp32, esp32s2, esp32s3, esp32c3, esp32c6"
    echo "  stm32f4, stm32f7, stm32h7"
    echo "  nrf52, rp2040"
    echo "  qemu-arm64, qemu-arm32, qemu-riscv"
    echo ""
    echo "=========================================="
    echo ""
}

# 检查依赖
check_dependencies() {
    info "检查依赖..."

    # 检查是否以 root 权限运行（服务器模式）
    if [ "$MODE" = "server" ] && [ "$EUID" -ne 0 ]; then
        error "服务器模式需要 root 权限，请使用 sudo"
    fi

    # 检查基本工具
    local missing_tools=()

    for tool in git curl wget; do
        if ! command -v $tool &> /dev/null; then
            missing_tools+=($tool)
        fi
    done

    if [ ${#missing_tools[@]} -gt 0 ]; then
        warn "缺少以下工具: ${missing_tools[*]}"
        info "正在安装..."

        if command -v apt-get &> /dev/null; then
            sudo apt-get update
            sudo apt-get install -y ${missing_tools[*]}
        elif command -v yum &> /dev/null; then
            sudo yum install -y ${missing_tools[*]}
        elif command -v dnf &> /dev/null; then
            sudo dnf install -y ${missing_tools[*]}
        fi
    fi

    info "依赖检查完成"
}

# 配置 NFS 服务器
setup_server() {
    info "配置 NFS 服务器..."

    # 运行 NFS 服务器配置脚本
    bash "$SCRIPT_DIR/nfs-server-setup.sh"

    # 初始化裸仓库
    info "初始化裸仓库..."
    bash "$SCRIPT_DIR/bare-repo-init.sh" --source gitee --branch "$BRANCH" --protocol ssh --lfs

    # 配置监控
    info "配置监控..."
    bash "$SCRIPT_DIR/monitor.sh" --nfs-server localhost --daemon

    # 配置自动维护
    info "配置自动维护..."
    (crontab -l 2>/dev/null; echo "0 2 * * * $SCRIPT_DIR/maintenance.sh --action update --auto") | crontab -
    (crontab -l 2>/dev/null; echo "0 3 * * 0 $SCRIPT_DIR/maintenance.sh --action all --auto") | crontab -

    echo ""
    echo "=========================================="
    echo "  NFS 服务器配置完成"
    echo "=========================================="
    echo ""
    echo "服务器 IP: $(hostname -I | awk '{print $1}')"
    echo "共享目录: /nfs/openvela"
    echo "裸仓库: /nfs/openvela/bare-repo/openvela.git"
    echo ""
    echo "在开发机上执行:"
    echo "  bash quick-start.sh --client $(hostname -I | awk '{print $1}') --chip $CHIP --branch $BRANCH"
    echo ""
    echo "=========================================="
}

# 配置开发机
setup_client() {
    info "配置开发机..."

    if [ -z "$NFS_SERVER_IP" ]; then
        error "请指定 NFS 服务器 IP"
    fi

    # 运行开发机配置脚本
    bash "$SCRIPT_DIR/dev-machine-setup.sh" "$NFS_SERVER_IP" --chip "$CHIP" --branch "$BRANCH"

    # 创建快速开始脚本
    cat > ~/openvela/quick-start.sh << 'EOF'
#!/bin/bash
# openvela 快速开始脚本

GREEN='\033[0;32m'
NC='\033[0m'

info() { echo -e "${GREEN}[INFO]${NC} $1"; }

info "=== openvela 快速开始 ==="

cd "$(dirname "$0")"

# 同步代码
info "同步代码..."
./sync.sh

# 构建固件
info "构建固件..."
./build-chip.sh qemu-arm64

# 运行模拟器
info "运行模拟器..."
./emulator.sh cmake_out/vela_goldfish-arm64-v8a-ap/

info "=== 快速开始完成 ==="
EOF

    chmod +x ~/openvela/quick-start.sh

    echo ""
    echo "=========================================="
    echo "  开发机配置完成"
    echo "=========================================="
    echo ""
    echo "工作目录: ~/openvela"
    echo "NFS 服务器: $NFS_SERVER_IP"
    echo "芯片目标: $CHIP"
    echo "分支: $BRANCH"
    echo ""
    echo "快速开始:"
    echo "  cd ~/openvela && ./quick-start.sh"
    echo ""
    echo "常用命令:"
    echo "  同步代码: cd ~/openvela && ./sync.sh"
    echo "  构建固件: cd ~/openvela && ./build-chip.sh $CHIP"
    echo "  运行模拟器: cd ~/openvela && ./emulator.sh <output_dir>"
    echo ""
    echo "=========================================="
}

# 显示交互式菜单
show_interactive_menu() {
    echo ""
    echo "=========================================="
    echo "  openvela 共享仓库配置向导"
    echo "=========================================="
    echo ""
    echo "请选择配置模式:"
    echo ""
    echo "1. 配置为 NFS 服务器"
    echo "2. 配置为开发机"
    echo "3. 查看帮助"
    echo "0. 退出"
    echo ""
    echo "=========================================="
    echo ""
}

# 交互式配置
interactive_setup() {
    while true; do
        show_interactive_menu
        read -p "请选择 [0-3]: " choice

        case $choice in
            1)
                MODE="server"
                setup_server
                break
                ;;
            2)
                MODE="client"
                read -p "请输入 NFS 服务器 IP: " NFS_SERVER_IP
                if [ -z "$NFS_SERVER_IP" ]; then
                    error "NFS 服务器 IP 不能为空"
                fi
                setup_client
                break
                ;;
            3)
                bash "$0" --help
                ;;
            0)
                info "退出配置向导"
                exit 0
                ;;
            *)
                echo "无效选择，请重新输入"
                ;;
        esac
    done
}

# 主流程
main() {
    show_welcome

    # 检查依赖
    check_dependencies

    # 如果没有指定模式，显示交互式菜单
    if [ -z "$MODE" ]; then
        interactive_setup
        return
    fi

    # 根据模式执行配置
    case $MODE in
        server)
            setup_server
            ;;
        client)
            setup_client
            ;;
        *)
            error "未知模式: $MODE"
            ;;
    esac
}

# 执行主流程
main
