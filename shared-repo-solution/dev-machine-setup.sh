#!/bin/bash
# 开发机配置脚本
# 用于配置 openvela 开发环境，使用 NFS 共享仓库
# Usage: bash dev-machine-setup.sh <nfs-server-ip> [options]
#
# Options:
#   --work-dir <path>      工作目录 (默认: ~/openvela)
#   --chip <chip>          芯片目标 (esp32|stm32|nrf52|qemu-arm64|qemu-arm32|qemu-riscv)
#   --branch <branch>      分支 (dev|trunk) (默认: dev)
#   --skip-deps            跳过依赖安装
#   --skip-mount           跳过 NFS 挂载

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
NFS_SERVER_IP=""
WORK_DIR="$HOME/openvela"
CHIP=""
BRANCH="dev"
SKIP_DEPS=false
SKIP_MOUNT=false
NFS_DIR="/nfs/openvela"
MOUNT_POINT="/mnt/openvela-bare"

# 解析命令行参数
if [ $# -lt 1 ]; then
    echo "Usage: bash dev-machine-setup.sh <nfs-server-ip> [options]"
    echo ""
    echo "Options:"
    echo "  --work-dir <path>      工作目录 (默认: ~/openvela)"
    echo "  --chip <chip>          芯片目标"
    echo "  --branch <branch>      分支 (dev|trunk) (默认: dev)"
    echo "  --skip-deps            跳过依赖安装"
    echo "  --skip-mount           跳过 NFS 挂载"
    echo ""
    echo "支持的芯片:"
    echo "  esp32       - 乐鑫 ESP32 (Xtensa)"
    echo "  esp32s2     - 乐鑫 ESP32-S2 (Xtensa)"
    echo "  esp32s3     - 乐鑫 ESP32-S3 (Xtensa)"
    echo "  esp32c3     - 乐鑫 ESP32-C3 (RISC-V)"
    echo "  esp32c6     - 乐鑫 ESP32-C6 (RISC-V)"
    echo "  stm32f4     - STM32F4 (ARM Cortex-M4)"
    echo "  stm32f7     - STM32F7 (ARM Cortex-M7)"
    echo "  stm32h7     - STM32H7 (ARM Cortex-M7)"
    echo "  nrf52       - Nordic nRF52 (ARM Cortex-M4)"
    echo "  rp2040      - Raspberry Pi RP2040 (ARM Cortex-M0+)"
    echo "  qemu-arm64  - QEMU ARM64 模拟器"
    echo "  qemu-arm32  - QEMU ARM32 模拟器"
    echo "  qemu-riscv  - QEMU RISC-V 模拟器"
    exit 1
fi

NFS_SERVER_IP="$1"
shift

while [[ $# -gt 0 ]]; do
    case $1 in
        --work-dir)
            WORK_DIR="$2"
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
        --skip-deps)
            SKIP_DEPS=true
            shift
            ;;
        --skip-mount)
            SKIP_MOUNT=true
            shift
            ;;
        --help|-h)
            echo "Usage: bash dev-machine-setup.sh <nfs-server-ip> [options]"
            exit 0
            ;;
        *)
            error "未知参数: $1"
            ;;
    esac
done

info "=== openvela 开发机配置 ==="
info "NFS 服务器: $NFS_SERVER_IP"
info "工作目录: $WORK_DIR"
info "芯片目标: $CHIP"
info "分支: $BRANCH"

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

# 安装依赖
install_dependencies() {
    if [ "$SKIP_DEPS" = true ]; then
        info "跳过依赖安装"
        return
    fi

    info "安装依赖..."

    case $OS in
        ubuntu|debian)
            sudo apt-get update
            sudo apt-get install -y \
                git git-lfs cmake python3 python3-pip \
                build-essential curl wget \
                nfs-common net-tools \
                gcc-arm-none-eabi gcc-riscv64-unknown-elf \
                qemu-system-arm qemu-system-misc \
                libpulse-dev libssl-dev \
                kconfig-frontends
            ;;
        centos|rhel|rocky|alma)
            sudo yum install -y \
                git git-lfs cmake python3 python3-pip \
                gcc gcc-c++ make curl wget \
                nfs-utils net-tools \
                arm-none-eabi-gcc-cs riscv64-unknown-elf-gcc \
                qemu-system-arm qemu-system-misc \
                pulseaudio-libs-devel openssl-devel
            ;;
        fedora)
            sudo dnf install -y \
                git git-lfs cmake python3 python3-pip \
                gcc gcc-c++ make curl wget \
                nfs-utils net-tools \
                arm-none-eabi-gcc-cs riscv64-unknown-elf-gcc \
                qemu-system-arm qemu-system-misc \
                pulseaudio-libs-devel openssl-devel
            ;;
    esac

    # 安装 repo 工具
    if ! command -v repo &> /dev/null; then
        info "安装 repo 工具..."
        curl -sSL "https://storage.googleapis.com/git-repo-downloads/repo" > /tmp/repo
        chmod +x /tmp/repo
        sudo mv /tmp/repo /usr/local/bin/
    fi

    # 初始化 Git LFS
    git lfs install

    # 安装 kconfiglib
    pip3 install kconfiglib

    info "依赖安装完成"
}

# 挂载 NFS
mount_nfs() {
    if [ "$SKIP_MOUNT" = true ]; then
        info "跳过 NFS 挂载"
        return
    fi

    info "挂载 NFS..."

    # 创建挂载点
    sudo mkdir -p "$MOUNT_POINT"

    # 检查是否已挂载
    if mountpoint -q "$MOUNT_POINT"; then
        warn "NFS 已挂载，跳过"
        return
    fi

    # 挂载 NFS
    sudo mount -t nfs "$NFS_SERVER_IP:$NFS_DIR" "$MOUNT_POINT"

    # 配置自动挂载
    if ! grep -q "$MOUNT_POINT" /etc/fstab; then
        echo "$NFS_SERVER_IP:$NFS_DIR $MOUNT_POINT nfs defaults,_netdev 0 0" | sudo tee -a /etc/fstab
    fi

    # 测试挂载
    if [ -d "$MOUNT_POINT/bare-repo" ]; then
        info "NFS 挂载成功"
    else
        error "NFS 挂载失败"
    fi
}

# 获取芯片配置路径
get_chip_config() {
    local chip=$1
    local config_path=""

    case $chip in
        esp32)
            config_path="vendor/espressif/boards/esp32/configs/default"
            ;;
        esp32s2)
            config_path="vendor/espressif/boards/esp32s2/configs/default"
            ;;
        esp32s3)
            config_path="vendor/espressif/boards/esp32s3/configs/default"
            ;;
        esp32c3)
            config_path="vendor/espressif/boards/esp32c3/configs/default"
            ;;
        esp32c6)
            config_path="vendor/espressif/boards/esp32c6/configs/default"
            ;;
        stm32f4)
            config_path="vendor/stm32/boards/stm32f4/configs/nsh"
            ;;
        stm32f7)
            config_path="vendor/stm32/boards/stm32f7/configs/nsh"
            ;;
        stm32h7)
            config_path="vendor/stm32/boards/stm32h7/configs/nsh"
            ;;
        nrf52)
            config_path="vendor/nordic/boards/nrf52/configs/nsh"
            ;;
        rp2040)
            config_path="vendor/raspberrypi/boards/rp2040/configs/nsh"
            ;;
        qemu-arm64)
            config_path="vendor/openvela/boards/vela/configs/goldfish-arm64-v8a-ap"
            ;;
        qemu-arm32)
            config_path="vendor/openvela/boards/vela/configs/goldfish-armeabi-v7a-ap"
            ;;
        qemu-riscv)
            config_path="vendor/openvela/boards/vela/configs/goldfish-riscv64"
            ;;
        *)
            error "不支持的芯片: $chip"
            ;;
    esac

    echo "$config_path"
}

# 初始化仓库
init_repo() {
    info "初始化仓库..."

    # 创建工作目录
    mkdir -p "$WORK_DIR"
    cd "$WORK_DIR"

    # 检查是否已初始化
    if [ -d ".repo" ]; then
        warn "检测到已存在的 .repo 目录"
        read -p "是否重新初始化? (y/N): " confirm
        if [ "$confirm" != "y" ] && [ "$confirm" != "Y" ]; then
            info "跳过初始化"
            return
        fi
        rm -rf .repo
    fi

    # 初始化 repo
    repo init -u "$MOUNT_POINT/bare-repo/openvela.git" \
        -b "$BRANCH" \
        -m openvela.xml \
        --reference="$MOUNT_POINT/bare-repo/openvela.git" \
        --git-lfs

    info "仓库初始化完成"
}

# 同步代码
sync_repo() {
    info "同步代码..."

    cd "$WORK_DIR"

    # 同步代码
    repo sync -c -j8

    info "代码同步完成"
}

# 配置 Git
configure_git() {
    info "配置 Git..."

    # 配置 Git 用户信息
    if [ -z "$(git config --global user.name)" ]; then
        read -p "请输入 Git 用户名: " git_name
        git config --global user.name "$git_name"
    fi

    if [ -z "$(git config --global user.email)" ]; then
        read -p "请输入 Git 邮箱: " git_email
        git config --global user.email "$git_email"
    fi

    # 配置 Git 性能优化
    git config --global core.preloadindex true
    git config --global core.fscache true
    git config --global gc.auto 256

    # 配置 Git LFS
    git config --global lfs.fetchinclude "*"
    git config --global lfs.fetchexclude ""

    info "Git 配置完成"
}

# 创建构建脚本
create_build_script() {
    info "创建构建脚本..."

    cat > "$WORK_DIR/build-chip.sh" << 'BUILDSCRIPT'
#!/bin/bash
# openvela 芯片构建脚本
# Usage: bash build-chip.sh <chip> [options]
#
# Options:
#   --clean        清理后重新构建
#   --menuconfig   运行 menuconfig
#   --j <num>      并行编译数

set -e

GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

info() { echo -e "${GREEN}[INFO]${NC} $1"; }
error() { echo -e "${RED}[ERROR]${NC} $1"; exit 1; }

CHIP="${1:-}"
CLEAN=false
MENUCONFIG=false
JOBS=$(nproc)

shift

while [[ $# -gt 0 ]]; do
    case $1 in
        --clean)
            CLEAN=true
            shift
            ;;
        --menuconfig)
            MENUCONFIG=true
            shift
            ;;
        --j)
            JOBS="$2"
            shift 2
            ;;
        *)
            error "未知参数: $1"
            ;;
    esac
done

if [ -z "$CHIP" ]; then
    echo "Usage: bash build-chip.sh <chip> [options]"
    echo ""
    echo "支持的芯片:"
    echo "  esp32, esp32s2, esp32s3, esp32c3, esp32c6"
    echo "  stm32f4, stm32f7, stm32h7"
    echo "  nrf52, rp2040"
    echo "  qemu-arm64, qemu-arm32, qemu-riscv"
    exit 1
fi

# 获取配置路径
get_config_path() {
    local chip=$1
    case $chip in
        esp32) echo "vendor/espressif/boards/esp32/configs/default" ;;
        esp32s2) echo "vendor/espressif/boards/esp32s2/configs/default" ;;
        esp32s3) echo "vendor/espressif/boards/esp32s3/configs/default" ;;
        esp32c3) echo "vendor/espressif/boards/esp32c3/configs/default" ;;
        esp32c6) echo "vendor/espressif/boards/esp32c6/configs/default" ;;
        stm32f4) echo "vendor/stm32/boards/stm32f4/configs/nsh" ;;
        stm32f7) echo "vendor/stm32/boards/stm32f7/configs/nsh" ;;
        stm32h7) echo "vendor/stm32/boards/stm32h7/configs/nsh" ;;
        nrf52) echo "vendor/nordic/boards/nrf52/configs/nsh" ;;
        rp2040) echo "vendor/raspberrypi/boards/rp2040/configs/nsh" ;;
        qemu-arm64) echo "vendor/openvela/boards/vela/configs/goldfish-arm64-v8a-ap" ;;
        qemu-arm32) echo "vendor/openvela/boards/vela/configs/goldfish-armeabi-v7a-ap" ;;
        qemu-riscv) echo "vendor/openvela/boards/vela/configs/goldfish-riscv64" ;;
        *) error "不支持的芯片: $chip" ;;
    esac
}

CONFIG_PATH=$(get_config_path "$CHIP")
OUTPUT_DIR="cmake_out/vela_$(basename $CONFIG_PATH)"

info "芯片: $CHIP"
info "配置路径: $CONFIG_PATH"
info "输出目录: $OUTPUT_DIR"

# 清理
if [ "$CLEAN" = true ]; then
    info "清理构建目录..."
    rm -rf "$OUTPUT_DIR"
fi

# menuconfig
if [ "$MENUCONFIG" = true ]; then
    info "运行 menuconfig..."
    ./build.sh "$CONFIG_PATH" --cmake menuconfig
    exit 0
fi

# 构建
info "开始构建..."
nohup ./build.sh "$CONFIG_PATH" --cmake -j"$JOBS" > build.log 2>&1 &
BUILD_PID=$!

info "构建进程 PID: $BUILD_PID"
info "查看构建日志: tail -f build.log"

# 等待构建完成
wait $BUILD_PID
BUILD_STATUS=$?

if [ $BUILD_STATUS -eq 0 ]; then
    info "构建成功"
    info "产物位置: $OUTPUT_DIR/nuttx"
else
    error "构建失败，请检查 build.log"
fi
BUILDSCRIPT

    chmod +x "$WORK_DIR/build-chip.sh"

    info "构建脚本创建完成"
}

# 创建同步脚本
create_sync_script() {
    info "创建同步脚本..."

    cat > "$WORK_DIR/sync.sh" << 'SYNCSCRIPT'
#!/bin/bash
# openvela 代码同步脚本

set -e

GREEN='\033[0;32m'
NC='\033[0m'

info() { echo -e "${GREEN}[INFO]${NC} $1"; }

info "开始同步代码..."

cd "$(dirname "$0")"

# 同步代码
repo sync -c -j8

info "代码同步完成"

# 显示最新更新
info "最新更新:"
repo info -o
SYNCSCRIPT

    chmod +x "$WORK_DIR/sync.sh"

    info "同步脚本创建完成"
}

# 显示完成信息
show_completion() {
    echo ""
    echo "=========================================="
    echo "  开发机配置完成"
    echo "=========================================="
    echo ""
    echo "工作目录: $WORK_DIR"
    echo "NFS 挂载点: $MOUNT_POINT"
    echo "分支: $BRANCH"
    echo ""
    echo "常用命令:"
    echo "  同步代码: cd $WORK_DIR && ./sync.sh"
    echo "  构建固件: cd $WORK_DIR && ./build-chip.sh <chip>"
    echo "  运行模拟器: cd $WORK_DIR && ./emulator.sh <output_dir>"
    echo ""
    echo "支持的芯片:"
    echo "  esp32, esp32s2, esp32s3, esp32c3, esp32c6"
    echo "  stm32f4, stm32f7, stm32h7"
    echo "  nrf52, rp2040"
    echo "  qemu-arm64, qemu-arm32, qemu-riscv"
    echo ""
    echo "=========================================="
}

# 主流程
main() {
    info "=== openvela 开发机配置 ==="

    detect_os
    install_dependencies
    mount_nfs
    configure_git
    init_repo
    sync_repo
    create_build_script
    create_sync_script
    show_completion

    info "=== 开发机配置完成 ==="
}

# 执行主流程
main
