#!/bin/bash
# openvela 多芯片构建脚本
# 用于构建不同芯片目标的 openvela 固件
# Usage: bash build.sh <chip> [options]
#
# Options:
#   --clean        清理后重新构建
#   --menuconfig   运行 menuconfig
#   --j <num>      并行编译数
#   --output <dir> 输出目录
#   --debug        启用调试模式

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
CHIP=""
CLEAN=false
MENUCONFIG=false
JOBS=$(nproc)
OUTPUT_DIR=""
DEBUG_MODE=false
BUILD_LOG="build.log"

# 解析命令行参数
if [ $# -lt 1 ]; then
    echo "Usage: bash build.sh <chip> [options]"
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
    echo ""
    echo "选项:"
    echo "  --clean        清理后重新构建"
    echo "  --menuconfig   运行 menuconfig"
    echo "  --j <num>      并行编译数 (默认: $(nproc))"
    echo "  --output <dir> 输出目录"
    echo "  --debug        启用调试模式"
    exit 1
fi

CHIP="$1"
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
        --output)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        --debug)
            DEBUG_MODE=true
            shift
            ;;
        --help|-h)
            echo "Usage: bash build.sh <chip> [options]"
            exit 0
            ;;
        *)
            error "未知参数: $1"
            ;;
    esac
done

info "=== openvela 构建 ==="
info "芯片: $CHIP"
info "并行任务: $JOBS"
info "调试模式: $DEBUG_MODE"

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

# 获取输出目录
get_output_dir() {
    local config_path=$1
    local output_dir=""

    if [ -n "$OUTPUT_DIR" ]; then
        output_dir="$OUTPUT_DIR"
    else
        output_dir="cmake_out/vela_$(basename $config_path)"
    fi

    echo "$output_dir"
}

# 检查工具链
check_toolchain() {
    local chip=$1

    info "检查工具链..."

    case $chip in
        esp32|esp32s2|esp32s3)
            if ! command -v xtensa-esp32-elf-gcc &> /dev/null; then
                warn "未找到 ESP32 工具链，尝试使用 ESP-IDF..."
                if [ -f ~/esp/esp-idf/export.sh ]; then
                    source ~/esp/esp-idf/export.sh
                else
                    error "请安装 ESP-IDF 或 ESP32 工具链"
                fi
            fi
            ;;
        esp32c3|esp32c6)
            if ! command -v riscv32-esp-elf-gcc &> /dev/null; then
                warn "未找到 ESP32-C3/C6 工具链，尝试使用 ESP-IDF..."
                if [ -f ~/esp/esp-idf/export.sh ]; then
                    source ~/esp/esp-idf/export.sh
                else
                    error "请安装 ESP-IDF 或 ESP32-C3/C6 工具链"
                fi
            fi
            ;;
        stm32f4|stm32f7|stm32h7|nrf52|rp2040)
            if ! command -v arm-none-eabi-gcc &> /dev/null; then
                error "请安装 ARM 工具链: sudo apt-get install -y gcc-arm-none-eabi"
            fi
            ;;
        qemu-arm64|qemu-arm32)
            if ! command -v aarch64-linux-gnu-gcc &> /dev/null && ! command -v arm-linux-gnueabihf-gcc &> /dev/null; then
                warn "未找到交叉编译工具链，使用本地编译器..."
            fi
            ;;
        qemu-riscv)
            if ! command -v riscv64-linux-gnu-gcc &> /dev/null && ! command -v riscv64-unknown-elf-gcc &> /dev/null; then
                warn "未找到 RISC-V 工具链，使用本地编译器..."
            fi
            ;;
    esac

    info "工具链检查完成"
}

# 清理构建目录
clean_build() {
    local output_dir=$1

    if [ "$CLEAN" = true ]; then
        info "清理构建目录..."
        rm -rf "$output_dir"
        info "清理完成"
    fi
}

# 运行 menuconfig
run_menuconfig() {
    local config_path=$1

    if [ "$MENUCONFIG" = true ]; then
        info "运行 menuconfig..."
        ./build.sh "$config_path" --cmake menuconfig
        exit 0
    fi
}

# 构建固件
build_firmware() {
    local config_path=$1
    local output_dir=$2

    info "开始构建..."
    info "配置路径: $config_path"
    info "输出目录: $output_dir"

    # 记录开始时间
    local start_time=$(date +%s)

    # 构建命令
    local build_cmd="./build.sh $config_path --cmake -j$JOBS"

    # 如果是调试模式
    if [ "$DEBUG_MODE" = true ]; then
        build_cmd="$build_cmd --debug"
    fi

    info "执行: $build_cmd"

    # 执行构建
    if [ "$DEBUG_MODE" = true ]; then
        # 调试模式：直接输出到终端
        eval "$build_cmd"
    else
        # 正常模式：输出到日志文件
        eval "$build_cmd" 2>&1 | tee "$BUILD_LOG"
    fi

    # 检查构建结果
    if [ $? -eq 0 ]; then
        local end_time=$(date +%s)
        local duration=$((end_time - start_time))

        info "构建成功"
        info "耗时: ${duration}秒"
        info "产物位置: $output_dir/nuttx"

        # 显示产物信息
        if [ -f "$output_dir/nuttx" ]; then
            info "固件大小: $(du -sh $output_dir/nuttx | awk '{print $1}')"
        fi
    else
        error "构建失败，请检查 $BUILD_LOG"
    fi
}

# 显示构建信息
show_build_info() {
    local chip=$1
    local config_path=$2
    local output_dir=$3

    echo ""
    echo "=========================================="
    echo "  构建完成"
    echo "=========================================="
    echo ""
    echo "芯片: $chip"
    echo "配置: $config_path"
    echo "输出: $output_dir"
    echo ""
    echo "后续操作:"
    echo "  运行模拟器: ./emulator.sh $output_dir"
    echo "  查看固件: file $output_dir/nuttx"
    echo "  调试固件: gdb $output_dir/nuttx"
    echo ""
    echo "=========================================="
}

# 主流程
main() {
    info "=== openvela 构建 ==="

    # 获取配置路径
    local config_path=$(get_chip_config "$CHIP")

    # 获取输出目录
    local output_dir=$(get_output_dir "$config_path")

    # 检查工具链
    check_toolchain "$CHIP"

    # 清理构建目录
    clean_build "$output_dir"

    # 运行 menuconfig
    run_menuconfig "$config_path"

    # 构建固件
    build_firmware "$config_path" "$output_dir"

    # 显示构建信息
    show_build_info "$CHIP" "$config_path" "$output_dir"

    info "=== 构建完成 ==="
}

# 执行主流程
main
