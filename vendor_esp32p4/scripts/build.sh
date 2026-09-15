#!/bin/bash
# ESP32-P4 一键构建脚本
# 支持多配置构建，自动收集证据

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 项目根目录
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
VENDOR_DIR="$PROJECT_ROOT/vendor_esp32p4"
NUTTX_DIR="$PROJECT_ROOT/nuttx"

# 证据目录
EVIDENCE_DIR="$VENDOR_DIR/docs/evidence"
BUILD_LOGS_DIR="$EVIDENCE_DIR/build-logs"

# 可用配置
AVAILABLE_CONFIGS=(
    "nsh"
    "uart0"
    "i2c"
    "display"
    "touch"
    "lvgl"
    "full"
)

# 打印帮助信息
show_help() {
    echo "ESP32-P4 一键构建脚本"
    echo ""
    echo "用法: $0 [选项] <配置>"
    echo ""
    echo "选项:"
    echo "  --help          显示此帮助信息"
    echo "  --list          列出所有可用配置"
    echo "  --clean         构建前清理"
    echo "  --parallel N    并行编译数 (默认: 2)"
    echo "  --evidence      收集构建证据"
    echo "  --all           构建所有配置"
    echo ""
    echo "配置:"
    echo "  nsh             最小 NSH 配置"
    echo "  uart0           UART0 验证配置"
    echo "  i2c             I2C 验证配置"
    echo "  display         显示验证配置"
    echo "  touch           触摸验证配置"
    echo "  lvgl            LVGL UI 配置"
    echo "  full            完整功能配置"
    echo ""
    echo "示例:"
    echo "  $0 nsh                    # 构建 NSH 配置"
    echo "  $0 --clean display        # 清理后构建显示配置"
    echo "  $0 --parallel 4 lvgl      # 使用4个并行任务构建"
    echo "  $0 --evidence full        # 构建并收集证据"
    echo "  $0 --all                  # 构建所有配置"
}

# 列出所有配置
list_configs() {
    echo "可用配置:"
    echo "=================="
    for config in "${AVAILABLE_CONFIGS[@]}"; do
        local config_dir="$VENDOR_DIR/boards/risc-v/esp32p4/esp32p4-evb/configs/$config"
        if [ -d "$config_dir" ]; then
            echo -e "  ${GREEN}✓${NC} $config"
        else
            echo -e "  ${RED}✗${NC} $config (目录不存在)"
        fi
    done
}

# 检查配置是否存在
check_config() {
    local config=$1
    local config_dir="$VENDOR_DIR/boards/risc-v/esp32p4/esp32p4-evb/configs/$config"

    if [ ! -d "$config_dir" ]; then
        echo -e "${RED}错误: 配置 '$config' 不存在${NC}"
        echo "可用配置: ${AVAILABLE_CONFIGS[*]}"
        return 1
    fi

    return 0
}

# 准备构建环境
prepare_environment() {
    echo -e "${BLUE}准备构建环境...${NC}"

    # 检查 NuttX 目录
    if [ ! -d "$NUTTX_DIR" ]; then
        echo -e "${RED}错误: NuttX 目录不存在: $NUTTX_DIR${NC}"
        return 1
    fi

    # 检查构建脚本
    if [ ! -f "$PROJECT_ROOT/build.sh" ]; then
        echo -e "${RED}错误: 构建脚本不存在: $PROJECT_ROOT/build.sh${NC}"
        return 1
    fi

    # 链接驱动
    echo "链接驱动..."
    if [ -f "$VENDOR_DIR/scripts/link_drivers.sh" ]; then
        bash "$VENDOR_DIR/scripts/link_drivers.sh" --link
    fi

    echo -e "${GREEN}构建环境准备完成${NC}"
    return 0
}

# 收集构建证据
collect_evidence() {
    local config=$1
    local build_result=$2
    local build_log=$3

    echo -e "${BLUE}收集构建证据...${NC}"

    # 创建证据目录
    mkdir -p "$BUILD_LOGS_DIR"

    # 生成证据文件名
    local timestamp=$(date '+%Y%m%d_%H%M%S')
    local evidence_file="$BUILD_LOGS_DIR/${config}_build_${timestamp}.log"

    # 保存构建日志
    if [ -f "$build_log" ]; then
        cp "$build_log" "$evidence_file"
        echo -e "${GREEN}构建日志已保存: $evidence_file${NC}"
    fi

    # 生成构建报告
    local report_file="$BUILD_LOGS_DIR/${config}_build_report_${timestamp}.md"
    cat > "$report_file" << EOF
# 构建报告: $config

## 基本信息
- 配置: $config
- 时间: $(date '+%Y-%m-%d %H:%M:%S')
- 结果: $build_result

## 构建环境
- 项目根目录: $PROJECT_ROOT
- NuttX 目录: $NUTTX_DIR
- 配置目录: $VENDOR_DIR/boards/risc-v/esp32p4/esp32p4-evb/configs/$config

## 构建产物
- 固件文件: $NUTTX_DIR/nuttx.bin
- 固件大小: $(ls -lh "$NUTTX_DIR/nuttx.bin" 2>/dev/null | awk '{print $5}' || echo "未知")

## 构建日志
\`\`\`
$(tail -50 "$build_log" 2>/dev/null || echo "日志文件不存在")
\`\`\`

## 验证状态
- [ ] 固件文件存在
- [ ] 固件大小正常
- [ ] 构建无错误
EOF

    echo -e "${GREEN}构建报告已生成: $report_file${NC}"
}

# 构建单个配置
build_config() {
    local config=$1
    local clean=${2:-false}
    local parallel=${3:-2}
    local collect_evidence_flag=${4:-false}

    echo -e "${BLUE}开始构建配置: $config${NC}"

    # 检查配置
    if ! check_config "$config"; then
        return 1
    fi

    # 准备环境
    if ! prepare_environment; then
        return 1
    fi

    # 清理构建
    if $clean; then
        echo "清理构建..."
        "$PROJECT_ROOT/build.sh" \
            "$VENDOR_DIR/boards/risc-v/esp32p4/esp32p4-evb/configs/$config" \
            distclean 2>&1 || true
    fi

    # 创建日志文件
    local build_log="/tmp/esp32p4_build_${config}_$(date +%Y%m%d_%H%M%S).log"

    # 执行构建
    echo "构建中..."
    local start_time=$(date +%s)

    if "$PROJECT_ROOT/build.sh" \
        "$VENDOR_DIR/boards/risc-v/esp32p4/esp32p4-evb/configs/$config" \
        -j"$parallel" 2>&1 | tee "$build_log"; then

        local end_time=$(date +%s)
        local duration=$((end_time - start_time))

        echo -e "${GREEN}构建成功: $config (耗时: ${duration}秒)${NC}"

        # 收集证据
        if $collect_evidence_flag; then
            collect_evidence "$config" "成功" "$build_log"
        fi

        # 显示固件信息
        if [ -f "$NUTTX_DIR/nuttx.bin" ]; then
            local firmware_size=$(ls -lh "$NUTTX_DIR/nuttx.bin" | awk '{print $5}')
            echo -e "${GREEN}固件大小: $firmware_size${NC}"
        fi

        return 0
    else
        local end_time=$(date +%s)
        local duration=$((end_time - start_time))

        echo -e "${RED}构建失败: $config (耗时: ${duration}秒)${NC}"

        # 收集证据
        if $collect_evidence_flag; then
            collect_evidence "$config" "失败" "$build_log"
        fi

        return 1
    fi
}

# 构建所有配置
build_all_configs() {
    local clean=${1:-false}
    local parallel=${2:-2}
    local collect_evidence_flag=${3:-false}

    echo -e "${BLUE}开始构建所有配置${NC}"

    local success_count=0
    local fail_count=0

    for config in "${AVAILABLE_CONFIGS[@]}"; do
        if build_config "$config" "$clean" "$parallel" "$collect_evidence_flag"; then
            ((success_count++))
        else
            ((fail_count++))
        fi
        echo ""
    done

    echo -e "${BLUE}构建完成${NC}"
    echo -e "  成功: ${GREEN}$success_count${NC}"
    echo -e "  失败: ${RED}$fail_count${NC}"
    echo -e "  总计: $((success_count + fail_count))"
}

# 主函数
main() {
    local config=""
    local clean=false
    local parallel=2
    local collect_evidence_flag=false
    local build_all=false

    # 解析参数
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --help|-h)
                show_help
                exit 0
                ;;
            --list|-l)
                list_configs
                exit 0
                ;;
            --clean|-c)
                clean=true
                shift
                ;;
            --parallel|-p)
                parallel="$2"
                shift 2
                ;;
            --evidence|-e)
                collect_evidence_flag=true
                shift
                ;;
            --all|-a)
                build_all=true
                shift
                ;;
            -*)
                echo -e "${RED}未知选项: $1${NC}"
                show_help
                exit 1
                ;;
            *)
                if [ -z "$config" ]; then
                    config="$1"
                else
                    echo -e "${RED}错误: 多余的参数: $1${NC}"
                    show_help
                    exit 1
                fi
                shift
                ;;
        esac
    done

    # 构建所有配置
    if $build_all; then
        build_all_configs "$clean" "$parallel" "$collect_evidence_flag"
        exit $?
    fi

    # 检查是否指定了配置
    if [ -z "$config" ]; then
        echo -e "${RED}错误: 未指定配置${NC}"
        show_help
        exit 1
    fi

    # 构建单个配置
    build_config "$config" "$clean" "$parallel" "$collect_evidence_flag"
    exit $?
}

# 执行主函数
main "$@"
