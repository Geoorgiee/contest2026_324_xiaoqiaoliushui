#!/bin/bash
# ESP32-P4 一键烧录脚本
# 支持自动检测设备、烧录固件、收集证据

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
FLASH_LOGS_DIR="$EVIDENCE_DIR/flash-logs"

# 默认配置
DEFAULT_PORT="/dev/ttyACM0"
DEFAULT_BAUD="921600"
DEFAULT_FLASH_SIZE="16MB"
DEFAULT_FLASH_MODE="dio"
DEFAULT_FLASH_FREQ="80m"
DEFAULT_OFFSET="0x2000"

# 打印帮助信息
show_help() {
    echo "ESP32-P4 一键烧录脚本"
    echo ""
    echo "用法: $0 [选项] [固件文件]"
    echo ""
    echo "选项:"
    echo "  --help          显示此帮助信息"
    echo "  --port PORT     串口设备 (默认: $DEFAULT_PORT)"
    echo "  --baud BAUD     波特率 (默认: $DEFAULT_BAUD)"
    echo "  --size SIZE     Flash 大小 (默认: $DEFAULT_FLASH_SIZE)"
    echo "  --mode MODE     Flash 模式 (默认: $DEFAULT_FLASH_MODE)"
    echo "  --freq FREQ     Flash 频率 (默认: $DEFAULT_FLASH_FREQ)"
    echo "  --offset OFFSET 烧录偏移 (默认: $DEFAULT_OFFSET)"
    echo "  --evidence      收集烧录证据"
    echo "  --verify        烧录后验证"
    echo "  --detect        检测设备"
    echo "  --info          显示设备信息"
    echo ""
    echo "示例:"
    echo "  $0                           # 烧录默认固件"
    echo "  $0 --port /dev/ttyUSB0       # 指定串口"
    echo "  $0 --evidence                # 烧录并收集证据"
    echo "  $0 --verify                  # 烧录后验证"
    echo "  $0 --detect                  # 检测设备"
    echo "  $0 --info                    # 显示设备信息"
}

# 检测设备
detect_device() {
    local port=${1:-$DEFAULT_PORT}

    echo -e "${BLUE}检测设备: $port${NC}"

    # 检查串口是否存在
    if [ ! -e "$port" ]; then
        echo -e "${RED}错误: 串口 $port 不存在${NC}"
        echo "可用串口:"
        ls /dev/tty{ACM,USB,S}* 2>/dev/null | head -10
        return 1
    fi

    # 检查 esptool 是否可用
    if ! command -v esptool &> /dev/null; then
        echo -e "${RED}错误: esptool 未安装${NC}"
        echo "请安装: pip install esptool"
        return 1
    fi

    # 检测芯片
    echo "检测芯片..."
    if esptool --chip esp32p4 --port "$port" chip-id 2>&1; then
        echo -e "${GREEN}设备检测成功${NC}"
        return 0
    else
        echo -e "${RED}设备检测失败${NC}"
        return 1
    fi
}

# 显示设备信息
show_device_info() {
    local port=${1:-$DEFAULT_PORT}

    echo -e "${BLUE}设备信息: $port${NC}"

    # 检测设备
    if ! detect_device "$port"; then
        return 1
    fi

    # 显示详细信息
    echo ""
    echo "芯片信息:"
    esptool --chip esp32p4 --port "$port" chip-id 2>&1 | grep -E "^(Chip|MAC|Features)" || true

    echo ""
    echo "Flash 信息:"
    esptool --chip esp32p4 --port "$port" flash-id 2>&1 | grep -E "^(Manufacturer|Device|Detected)" || true
}

# 收集烧录证据
collect_evidence() {
    local port=$1
    local firmware=$2
    local flash_result=$3
    local flash_log=$4

    echo -e "${BLUE}收集烧录证据...${NC}"

    # 创建证据目录
    mkdir -p "$FLASH_LOGS_DIR"

    # 生成证据文件名
    local timestamp=$(date '+%Y%m%d_%H%M%S')
    local evidence_file="$FLASH_LOGS_DIR/flash_${timestamp}.log"

    # 保存烧录日志
    if [ -f "$flash_log" ]; then
        cp "$flash_log" "$evidence_file"
        echo -e "${GREEN}烧录日志已保存: $evidence_file${NC}"
    fi

    # 生成烧录报告
    local report_file="$FLASH_LOGS_DIR/flash_report_${timestamp}.md"
    cat > "$report_file" << EOF
# 烧录报告

## 基本信息
- 时间: $(date '+%Y-%m-%d %H:%M:%S')
- 串口: $port
- 固件: $firmware
- 结果: $flash_result

## 烧录参数
- Flash 大小: $DEFAULT_FLASH_SIZE
- Flash 模式: $DEFAULT_FLASH_MODE
- Flash 频率: $DEFAULT_FLASH_FREQ
- 烧录偏移: $DEFAULT_OFFSET
- 波特率: $DEFAULT_BAUD

## 固件信息
- 文件大小: $(ls -lh "$firmware" 2>/dev/null | awk '{print $5}' || echo "未知")
- 文件路径: $firmware

## 烧录日志
\`\`\`
$(tail -50 "$flash_log" 2>/dev/null || echo "日志文件不存在")
\`\`\`

## 验证状态
- [ ] 烧录成功
- [ ] 固件完整
- [ ] 设备可启动
EOF

    echo -e "${GREEN}烧录报告已生成: $report_file${NC}"
}

# 烧录固件
flash_firmware() {
    local port=${1:-$DEFAULT_PORT}
    local firmware=${2:-"$NUTTX_DIR/nuttx.bin"}
    local evidence=${3:-false}
    local verify=${4:-false}

    echo -e "${BLUE}开始烧录固件${NC}"

    # 检查固件文件
    if [ ! -f "$firmware" ]; then
        echo -e "${RED}错误: 固件文件不存在: $firmware${NC}"
        return 1
    fi

    # 检查固件大小
    local firmware_size=$(stat -c%s "$firmware")
    if [ "$firmware_size" -eq 0 ]; then
        echo -e "${RED}错误: 固件文件为空${NC}"
        return 1
    fi

    # 检测设备
    if ! detect_device "$port"; then
        return 1
    fi

    # 创建日志文件
    local flash_log="/tmp/esp32p4_flash_$(date +%Y%m%d_%H%M%S).log"

    # 执行烧录
    echo "烧录中..."
    local start_time=$(date +%s)

    if esptool --chip esp32p4 \
        --port "$port" \
        --baud "$DEFAULT_BAUD" \
        write-flash \
        -fs "$DEFAULT_FLASH_SIZE" \
        -fm "$DEFAULT_FLASH_MODE" \
        -ff "$DEFAULT_FLASH_FREQ" \
        "$DEFAULT_OFFSET" \
        "$firmware" 2>&1 | tee "$flash_log"; then

        local end_time=$(date +%s)
        local duration=$((end_time - start_time))

        echo -e "${GREEN}烧录成功 (耗时: ${duration}秒)${NC}"

        # 收集证据
        if $evidence; then
            collect_evidence "$port" "$firmware" "成功" "$flash_log"
        fi

        # 烧录后验证
        if $verify; then
            echo "等待设备重启..."
            sleep 2
            echo "验证设备..."
            if detect_device "$port"; then
                echo -e "${GREEN}设备验证成功${NC}"
            else
                echo -e "${YELLOW}设备验证失败，但烧录已完成${NC}"
            fi
        fi

        return 0
    else
        local end_time=$(date +%s)
        local duration=$((end_time - start_time))

        echo -e "${RED}烧录失败 (耗时: ${duration}秒)${NC}"

        # 收集证据
        if $evidence; then
            collect_evidence "$port" "$firmware" "失败" "$flash_log"
        fi

        return 1
    fi
}

# 连接串口
connect_serial() {
    local port=${1:-$DEFAULT_PORT}
    local baud=${2:-115200}

    echo -e "${BLUE}连接串口: $port ($baud)${NC}"

    # 检查串口是否存在
    if [ ! -e "$port" ]; then
        echo -e "${RED}错误: 串口 $port 不存在${NC}"
        return 1
    fi

    # 检查 picocom 是否可用
    if ! command -v picocom &> /dev/null; then
        echo -e "${RED}错误: picocom 未安装${NC}"
        echo "请安装: sudo apt-get install picocom"
        return 1
    fi

    echo "连接串口... (按 Ctrl+A Ctrl+Q 退出)"
    picocom -b "$baud" "$port"
}

# 主函数
main() {
    local port="$DEFAULT_PORT"
    local firmware="$NUTTX_DIR/nuttx.bin"
    local evidence=false
    local verify=false
    local detect=false
    local info=false
    local connect=false

    # 解析参数
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --help|-h)
                show_help
                exit 0
                ;;
            --port|-p)
                port="$2"
                shift 2
                ;;
            --baud|-b)
                DEFAULT_BAUD="$2"
                shift 2
                ;;
            --size|-s)
                DEFAULT_FLASH_SIZE="$2"
                shift 2
                ;;
            --mode|-m)
                DEFAULT_FLASH_MODE="$2"
                shift 2
                ;;
            --freq|-f)
                DEFAULT_FLASH_FREQ="$2"
                shift 2
                ;;
            --offset|-o)
                DEFAULT_OFFSET="$2"
                shift 2
                ;;
            --evidence|-e)
                evidence=true
                shift
                ;;
            --verify|-v)
                verify=true
                shift
                ;;
            --detect|-d)
                detect=true
                shift
                ;;
            --info|-i)
                info=true
                shift
                ;;
            --connect|-c)
                connect=true
                shift
                ;;
            -*)
                echo -e "${RED}未知选项: $1${NC}"
                show_help
                exit 1
                ;;
            *)
                firmware="$1"
                shift
                ;;
        esac
    done

    # 检测设备
    if $detect; then
        detect_device "$port"
        exit $?
    fi

    # 显示设备信息
    if $info; then
        show_device_info "$port"
        exit $?
    fi

    # 连接串口
    if $connect; then
        connect_serial "$port"
        exit $?
    fi

    # 烧录固件
    flash_firmware "$port" "$firmware" "$evidence" "$verify"
    exit $?
}

# 执行主函数
main "$@"
