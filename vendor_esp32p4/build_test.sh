#!/usr/bin/env bash
############################################################################
# vendor_esp32p4/build_test.sh
#
# ESP32-P4 EVB 编译测试脚本
# 用于验证所有 defconfig 配置的编译正确性
#
# Usage:
#   bash build_test.sh [options]
#
# Options:
#   --config <name>    只测试指定配置 (nsh|default|ble|display|storage|tinyml)
#   --list             列出所有可用配置
#   --clean            编译前清理
#   --dry-run          只验证配置，不实际编译
#   --jobs <num>       并行编译数 (默认: nproc)
#   --verbose          显示详细输出
#   --help             显示帮助信息
#
# 环境要求:
#   - RISC-V 工具链: riscv-none-elf-gcc (xPack 14.2.0+)
#   - NuttX 构建系统: /home/geo/openvela-build/
#   - 内存: 建议 16GB RAM
#
############################################################################

set -euo pipefail

#===========================================================================
# 颜色和日志函数
#===========================================================================

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

log_info()    { echo -e "${GREEN}[INFO]${NC} $1"; }
log_warn()    { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error()   { echo -e "${RED}[ERROR]${NC} $1"; }
log_debug()   { echo -e "${BLUE}[DEBUG]${NC} $1"; }
log_success() { echo -e "${CYAN}[PASS]${NC} $1"; }
log_fail()    { echo -e "${RED}[FAIL]${NC} $1"; }

#===========================================================================
# 配置常量
#===========================================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CONFIGS_DIR="${SCRIPT_DIR}/boards/risc-v/esp32p4/esp32p4-evb/configs"
CONTEST_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
NUTTX_DIR="${CONTEST_DIR}/nuttx"
TOOLS_DIR="${NUTTX_DIR}/tools"
VENDOR_DIR="${SCRIPT_DIR}"

# 工具链路径
TOOLCHAIN_PATH="/opt/openvela/prebuilts/gcc/linux-x86_64/riscv-none-elf/bin"
KCONFIG_PATH="/opt/openvela/prebuilts/build-tools/linux-x86_64/bin"

# 所有可用配置
ALL_CONFIGS=("nsh" "default" "ble" "display" "storage" "tinyml")

# 配置描述
declare -A CONFIG_DESCRIPTIONS=(
    ["nsh"]="NSH 最小系统 - UART0 串口控制台, GPIO, Timer, SD卡, FAT文件系统"
    ["default"]="默认配置 - 基础 NSH 系统, 无 PSRAM, SD卡支持"
    ["ble"]="蓝牙配置 - NimBLE BLE 协议栈, 需要外部 BLE 协处理器"
    ["display"]="显示配置 - MIPI-DSI 显示, ILI9881C 800x1280 面板"
    ["storage"]="存储配置 - SDMMC SD卡, 详细存储配置选项"
    ["tinyml"]="TinyML 配置 - TensorFlow Lite Micro, 神经网络算子"
)

# 配置关键特性
declare -A CONFIG_FEATURES=(
    ["nsh"]="PSRAM=32MB, MM_REGIONS=2, SDMMC, FAT+LFN, ProcFS, ROMFS"
    ["default"]="PSRAM=disabled, MM_REGIONS=1, SDMMC, FAT+LFN"
    ["ble"]="PSRAM=32MB, BLE_NimBLE, ESP-IDF, INIT_STACK=8KB"
    ["display"]="PSRAM=32MB OPI, MIPI-DSI 2-lane, LCD_ILI9881C, CPU=400MHz"
    ["storage"]="MM_REGIONS=1, SDMMC 4-bit, CPU=400MHz, SDCARD_DEVOPT"
    ["tinyml"]="PSRAM=32MB, TFLite_Micro 128KB, 9种算子, INIT_STACK=8KB"
)

#===========================================================================
# 命令行参数解析
#===========================================================================

CONFIG_FILTER=""
LIST_ONLY=false
CLEAN_BUILD=false
DRY_RUN=false
JOBS=$(nproc)
VERBOSE=false
SHOW_HELP=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --config)
            CONFIG_FILTER="$2"
            shift 2
            ;;
        --list)
            LIST_ONLY=true
            shift
            ;;
        --clean)
            CLEAN_BUILD=true
            shift
            ;;
        --dry-run)
            DRY_RUN=true
            shift
            ;;
        --jobs)
            JOBS="$2"
            shift 2
            ;;
        --verbose)
            VERBOSE=true
            shift
            ;;
        --help|-h)
            SHOW_HELP=true
            shift
            ;;
        *)
            log_error "未知参数: $1"
            exit 1
            ;;
    esac
done

#===========================================================================
# 帮助信息
#===========================================================================

show_help() {
    cat << 'EOF'
ESP32-P4 EVB 编译测试脚本
========================

Usage: bash build_test.sh [options]

Options:
  --config <name>    只测试指定配置 (nsh|default|ble|display|storage|tinyml)
  --list             列出所有可用配置
  --clean            编译前清理构建目录
  --dry-run          只验证配置和环境，不实际编译
  --jobs <num>       并行编译数 (默认: CPU核心数)
  --verbose          显示详细输出
  --help             显示此帮助信息

Examples:
  # 列出所有配置
  bash build_test.sh --list

  # 测试单个配置
  bash build_test.sh --config nsh

  # 测试所有配置
  bash build_test.sh

  # 只验证环境，不编译
  bash build_test.sh --dry-run

  # 清理后重新编译
  bash build_test.sh --config nsh --clean

Configurations:
  nsh        NSH 最小系统 (推荐入门)
  default    默认配置 (基础功能)
  ble        蓝牙配置 (需要外部BLE芯片)
  display    显示配置 (需要MIPI-DSI屏幕)
  storage    存储配置 (SD卡详细配置)
  tinyml     TinyML 配置 (机器学习)

EOF
}

#===========================================================================
# 列出配置
#===========================================================================

list_configs() {
    echo ""
    echo "=========================================="
    echo "  ESP32-P4 EVB 可用配置列表"
    echo "=========================================="
    echo ""

    for config in "${ALL_CONFIGS[@]}"; do
        local defconfig="${CONFIGS_DIR}/${config}/defconfig"

        echo -e "${CYAN}[${config}]${NC}"
        echo -e "  描述: ${CONFIG_DESCRIPTIONS[$config]}"
        echo -e "  特性: ${CONFIG_FEATURES[$config]}"

        if [ -f "$defconfig" ]; then
            echo -e "  状态: ${GREEN}defconfig 存在${NC}"
            echo -e "  路径: ${defconfig}"
        else
            echo -e "  状态: ${RED}defconfig 缺失${NC}"
        fi
        echo ""
    done
}

#===========================================================================
# 环境检查
#===========================================================================

check_environment() {
    log_info "检查编译环境..."
    local errors=0

    # 检查工具链
    if [ -x "${TOOLCHAIN_PATH}/riscv-none-elf-gcc" ]; then
        local gcc_version=$("${TOOLCHAIN_PATH}/riscv-none-elf-gcc" --version | head -1)
        log_success "RISC-V 工具链: ${gcc_version}"
    else
        log_error "RISC-V 工具链未找到: ${TOOLCHAIN_PATH}/riscv-none-elf-gcc"
        errors=$((errors + 1))
    fi

    # 检查 kconfig-frontends
    if [ -x "${KCONFIG_PATH}/kconfig-conf" ]; then
        log_success "kconfig-conf: 已安装"
    elif [ -x "${KCONFIG_PATH}/conf" ]; then
        log_success "kconfig-frontends: 已安装"
    else
        log_error "kconfig-conf 未找到: ${KCONFIG_PATH}/kconfig-conf"
        errors=$((errors + 1))
    fi

    # 检查 NuttX 目录
    if [ -d "${NUTTX_DIR}" ]; then
        log_success "NuttX 目录: ${NUTTX_DIR}"
    else
        log_error "NuttX 目录不存在: ${NUTTX_DIR}"
        errors=$((errors + 1))
    fi

    # 检查 configure.sh
    if [ -x "${TOOLS_DIR}/configure.sh" ]; then
        log_success "configure.sh: 已就绪"
    else
        log_error "configure.sh 未找到: ${TOOLS_DIR}/configure.sh"
        errors=$((errors + 1))
    fi

    # 检查内存
    local mem_mb=$(free -m | awk 'NR==2{print $2}')
    local mem_gb=$(( (mem_mb + 512) / 1024 ))
    if [ "$mem_mb" -ge 15360 ]; then
        log_success "内存: ${mem_gb}GB (${mem_mb}MB)"
    elif [ "$mem_mb" -ge 7680 ]; then
        log_warn "内存: ${mem_gb}GB (${mem_mb}MB, 推荐 16GB)"
    else
        log_error "内存不足: ${mem_gb}GB (${mem_mb}MB, 需要至少 8GB)"
        errors=$((errors + 1))
    fi

    # 检查磁盘空间
    local disk_avail=$(df -BG "${SCRIPT_DIR}" | awk 'NR==2{print $4}' | tr -d 'G')
    if [ "$disk_avail" -ge 20 ]; then
        log_success "磁盘空间: ${disk_avail}GB 可用"
    else
        log_warn "磁盘空间不足: ${disk_avail}GB (建议 20GB+)"
    fi

    echo ""
    return $errors
}

#===========================================================================
# 验证配置
#===========================================================================

validate_config() {
    local config=$1
    local defconfig="${CONFIGS_DIR}/${config}/defconfig"

    log_info "验证配置: ${config}"

    # 检查 defconfig 文件
    if [ ! -f "$defconfig" ]; then
        log_fail "defconfig 不存在: ${defconfig}"
        return 1
    fi

    # 检查关键配置项
    local required_configs=(
        "CONFIG_ARCH"
        "CONFIG_ARCH_BOARD"
        "CONFIG_ARCH_CHIP"
        "CONFIG_ARCH_RISCV"
        "CONFIG_INIT_ENTRYPOINT"
    )

    local missing=0
    for cfg in "${required_configs[@]}"; do
        if ! grep -q "^${cfg}=" "$defconfig"; then
            log_warn "缺少配置项: ${cfg}"
            missing=$((missing + 1))
        fi
    done

    # 检查架构一致性
    if grep -q 'CONFIG_ARCH="risc-v"' "$defconfig"; then
        log_success "架构: RISC-V"
    else
        log_fail "架构配置错误"
        return 1
    fi

    # 检查芯片一致性
    if grep -q 'CONFIG_ARCH_CHIP="esp32p4"' "$defconfig"; then
        log_success "芯片: ESP32-P4"
    else
        log_fail "芯片配置错误"
        return 1
    fi

    # 检查板级一致性
    if grep -q 'CONFIG_ARCH_BOARD="esp32p4-evb"' "$defconfig"; then
        log_success "板级: ESP32-P4 EVB"
    else
        log_fail "板级配置错误"
        return 1
    fi

    if [ $missing -eq 0 ]; then
        log_success "配置验证通过: ${config}"
    else
        log_warn "配置验证完成，有 ${missing} 个警告"
    fi

    return 0
}

#===========================================================================
# 编译单个配置
#===========================================================================

build_config() {
    local config=$1
    local defconfig="${CONFIGS_DIR}/${config}/defconfig"
    local board_config="${CONFIGS_DIR}/${config}"
    local build_log="${SCRIPT_DIR}/build_${config}.log"
    local start_time=$(date +%s)

    echo ""
    echo "=========================================="
    echo "  编译配置: ${config}"
    echo "  描述: ${CONFIG_DESCRIPTIONS[$config]}"
    echo "=========================================="
    echo ""

    # 验证配置
    if ! validate_config "$config"; then
        log_fail "配置验证失败: ${config}"
        return 1
    fi

    # Dry-run 模式只验证配置
    if [ "$DRY_RUN" = true ]; then
        log_info "Dry-run 模式，跳过实际编译"
        return 0
    fi

    # 设置环境变量
    export PATH="${TOOLCHAIN_PATH}:${KCONFIG_PATH}:${PATH}"

    # 清理构建
    if [ "$CLEAN_BUILD" = true ]; then
        log_info "清理构建目录..."
        make -C "${NUTTX_DIR}" distclean 2>/dev/null || true
    fi

    # 配置
    log_info "配置 ${config}..."
    if [ "$VERBOSE" = true ]; then
        "${TOOLS_DIR}/configure.sh" -e "${board_config}"
    else
        "${TOOLS_DIR}/configure.sh" -e "${board_config}" > /dev/null 2>&1
    fi

    if [ $? -ne 0 ]; then
        log_fail "配置失败: ${config}"
        return 1
    fi
    log_success "配置完成: ${config}"

    # 编译
    log_info "开始编译 (jobs=${JOBS})..."
    local extra_flags="-Wno-cpp -Wno-deprecated-declarations"

    if [ "$VERBOSE" = true ]; then
        make -C "${NUTTX_DIR}" EXTRAFLAGS="${extra_flags}" -j${JOBS} 2>&1 | tee "${build_log}"
    else
        make -C "${NUTTX_DIR}" EXTRAFLAGS="${extra_flags}" -j${JOBS} > "${build_log}" 2>&1
    fi

    local build_result=$?
    local end_time=$(date +%s)
    local duration=$((end_time - start_time))

    # 检查结果
    if [ $build_result -eq 0 ]; then
        log_success "编译成功: ${config} (耗时: ${duration}秒)"

        # 检查产物
        if [ -f "${NUTTX_DIR}/nuttx" ]; then
            local size=$(du -sh "${NUTTX_DIR}/nuttx" | awk '{print $1}')
            log_info "固件大小: ${size}"
            log_info "产物路径: ${NUTTX_DIR}/nuttx"
        fi

        # 检查 ELF 文件
        if [ -x "${TOOLCHAIN_PATH}/riscv-none-elf-size" ]; then
            echo ""
            log_info "固件段信息:"
            "${TOOLCHAIN_PATH}/riscv-none-elf-size" "${NUTTX_DIR}/nuttx" 2>/dev/null || true
        fi

        return 0
    else
        log_fail "编译失败: ${config} (耗时: ${duration}秒)"
        log_error "查看日志: ${build_log}"

        # 显示最后 20 行错误
        echo ""
        log_error "最后 20 行编译输出:"
        tail -20 "${build_log}" 2>/dev/null || true

        return 1
    fi
}

#===========================================================================
# 测试所有配置
#===========================================================================

test_all_configs() {
    local total=0
    local passed=0
    local failed=0
    local skipped=0
    local results=()

    echo ""
    echo "=========================================="
    echo "  ESP32-P4 EVB 全配置编译测试"
    echo "  配置数: ${#ALL_CONFIGS[@]}"
    echo "  并行数: ${JOBS}"
    echo "=========================================="
    echo ""

    # 检查环境
    if ! check_environment; then
        log_error "环境检查失败，无法继续"
        return 1
    fi

    # 测试每个配置
    for config in "${ALL_CONFIGS[@]}"; do
        total=$((total + 1))

        # 如果指定了过滤条件
        if [ -n "$CONFIG_FILTER" ] && [ "$config" != "$CONFIG_FILTER" ]; then
            skipped=$((skipped + 1))
            results+=("${config}: SKIPPED")
            continue
        fi

        if build_config "$config"; then
            passed=$((passed + 1))
            results+=("${config}: PASS")
        else
            failed=$((failed + 1))
            results+=("${config}: FAIL")
        fi
    done

    # 显示总结
    echo ""
    echo "=========================================="
    echo "  测试结果总结"
    echo "=========================================="
    echo ""

    for result in "${results[@]}"; do
        if [[ "$result" == *"PASS"* ]]; then
            log_success "$result"
        elif [[ "$result" == *"FAIL"* ]]; then
            log_fail "$result"
        else
            log_warn "$result"
        fi
    done

    echo ""
    echo "总计: ${total} | 通过: ${passed} | 失败: ${failed} | 跳过: ${skipped}"
    echo ""

    if [ $failed -eq 0 ]; then
        log_success "所有测试通过!"
        return 0
    else
        log_error "${failed} 个配置编译失败"
        return 1
    fi
}

#===========================================================================
# 主函数
#===========================================================================

main() {
    echo ""
    echo "=========================================="
    echo "  ESP32-P4 EVB 编译测试工具"
    echo "  版本: 1.0.0"
    echo "=========================================="
    echo ""

    # 显示帮助
    if [ "$SHOW_HELP" = true ]; then
        show_help
        exit 0
    fi

    # 列出配置
    if [ "$LIST_ONLY" = true ]; then
        list_configs
        exit 0
    fi

    # 检查配置目录
    if [ ! -d "$CONFIGS_DIR" ]; then
        log_error "配置目录不存在: ${CONFIGS_DIR}"
        exit 1
    fi

    # 测试单个配置
    if [ -n "$CONFIG_FILTER" ]; then
        # 验证配置名
        local valid=false
        for config in "${ALL_CONFIGS[@]}"; do
            if [ "$config" = "$CONFIG_FILTER" ]; then
                valid=true
                break
            fi
        done

        if [ "$valid" = false ]; then
            log_error "无效的配置名: ${CONFIG_FILTER}"
            log_error "可用配置: ${ALL_CONFIGS[*]}"
            exit 1
        fi

        # 检查环境
        if ! check_environment; then
            log_error "环境检查失败"
            exit 1
        fi

        # 编译指定配置
        if build_config "$CONFIG_FILTER"; then
            exit 0
        else
            exit 1
        fi
    fi

    # 测试所有配置
    test_all_configs
}

# 执行主函数
main "$@"
