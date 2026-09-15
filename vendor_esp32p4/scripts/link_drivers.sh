#!/bin/bash
# ESP32-P4 驱动管理脚本
# 用于链接、检查和清理显示和触摸驱动

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

# 驱动源文件路径
DRIVERS_DIR="$VENDOR_DIR/drivers"
DISPLAY_DRIVERS_DIR="$DRIVERS_DIR/display"
INPUT_DRIVERS_DIR="$DRIVERS_DIR/input"

# NuttX 驱动目标路径
NUTTX_DISPLAY_DIR="$NUTTX_DIR/drivers/video"
NUTTX_INPUT_DIR="$NUTTX_DIR/drivers/input"

# 日志文件
LOG_FILE="$VENDOR_DIR/logs/driver_link.log"

# 打印帮助信息
show_help() {
    echo "ESP32-P4 驱动管理脚本"
    echo ""
    echo "用法: $0 [选项]"
    echo ""
    echo "选项:"
    echo "  --help          显示此帮助信息"
    echo "  --link          链接驱动到 NuttX"
    echo "  --check         检查驱动链接状态"
    echo "  --clean         清理驱动链接"
    echo "  --status        显示驱动状态"
    echo "  --log           显示驱动链接日志"
    echo ""
    echo "示例:"
    echo "  $0 --link       # 链接驱动"
    echo "  $0 --check      # 检查驱动链接"
    echo "  $0 --clean      # 清理驱动链接"
}

# 记录日志
log() {
    local message="$1"
    local timestamp=$(date '+%Y-%m-%d %H:%M:%S')
    echo "[$timestamp] $message" >> "$LOG_FILE"
    echo -e "${BLUE}[$timestamp]${NC} $message"
}

# 记录错误
log_error() {
    local message="$1"
    local timestamp=$(date '+%Y-%m-%d %H:%M:%S')
    echo "[$timestamp] ERROR: $message" >> "$LOG_FILE"
    echo -e "${RED}[$timestamp] ERROR: $message${NC}"
}

# 记录成功
log_success() {
    local message="$1"
    local timestamp=$(date '+%Y-%m-%d %H:%M:%S')
    echo "[$timestamp] SUCCESS: $message" >> "$LOG_FILE"
    echo -e "${GREEN}[$timestamp] SUCCESS: $message${NC}"
}

# 记录警告
log_warning() {
    local message="$1"
    local timestamp=$(date '+%Y-%m-%d %H:%M:%S')
    echo "[$timestamp] WARNING: $message" >> "$LOG_FILE"
    echo -e "${YELLOW}[$timestamp] WARNING: $message${NC}"
}

# 检查目录是否存在
check_directories() {
    if [ ! -d "$DRIVERS_DIR" ]; then
        log_error "驱动目录不存在: $DRIVERS_DIR"
        return 1
    fi

    if [ ! -d "$NUTTX_DIR" ]; then
        log_error "NuttX 目录不存在: $NUTTX_DIR"
        return 1
    fi

    return 0
}

# 链接显示驱动
link_display_drivers() {
    log "链接显示驱动..."

    if [ ! -d "$DISPLAY_DRIVERS_DIR" ]; then
        log_warning "显示驱动目录不存在: $DISPLAY_DRIVERS_DIR"
        return 0
    fi

    if [ ! -d "$NUTTX_DISPLAY_DIR" ]; then
        log_error "NuttX 显示驱动目录不存在: $NUTTX_DISPLAY_DIR"
        return 1
    fi

    # 链接 EK79007 驱动
    if [ -f "$DISPLAY_DRIVERS_DIR/ek79007.c" ]; then
        ln -sf "$DISPLAY_DRIVERS_DIR/ek79007.c" "$NUTTX_DISPLAY_DIR/ek79007.c"
        log_success "链接 EK79007 驱动"
    else
        log_warning "EK79007 驱动文件不存在"
    fi

    # 链接 EK79007 头文件
    if [ -f "$DISPLAY_DRIVERS_DIR/ek79007.h" ]; then
        ln -sf "$DISPLAY_DRIVERS_DIR/ek79007.h" "$NUTTX_DISPLAY_DIR/ek79007.h"
        log_success "链接 EK79007 头文件"
    fi

    return 0
}

# 链接触摸驱动
link_input_drivers() {
    log "链接触摸驱动..."

    if [ ! -d "$INPUT_DRIVERS_DIR" ]; then
        log_warning "触摸驱动目录不存在: $INPUT_DRIVERS_DIR"
        return 0
    fi

    if [ ! -d "$NUTTX_INPUT_DIR" ]; then
        log_error "NuttX 触摸驱动目录不存在: $NUTTX_INPUT_DIR"
        return 1
    fi

    # 链接 GT911 驱动
    if [ -f "$INPUT_DRIVERS_DIR/gt911.c" ]; then
        ln -sf "$INPUT_DRIVERS_DIR/gt911.c" "$NUTTX_INPUT_DIR/gt911.c"
        log_success "链接 GT911 驱动"
    else
        log_warning "GT911 驱动文件不存在"
    fi

    # 链接 GT911 头文件
    if [ -f "$INPUT_DRIVERS_DIR/gt911.h" ]; then
        ln -sf "$INPUT_DRIVERS_DIR/gt911.h" "$NUTTX_INPUT_DIR/gt911.h"
        log_success "链接 GT911 头文件"
    fi

    return 0
}

# 链接所有驱动
link_all_drivers() {
    log "开始链接所有驱动..."

    if ! check_directories; then
        return 1
    fi

    # 创建日志目录
    mkdir -p "$(dirname "$LOG_FILE")"

    # 链接显示驱动
    if ! link_display_drivers; then
        log_error "链接显示驱动失败"
        return 1
    fi

    # 链接触摸驱动
    if ! link_input_drivers; then
        log_error "链接触摸驱动失败"
        return 1
    fi

    log_success "所有驱动链接完成"
    return 0
}

# 检查驱动链接
check_driver_links() {
    log "检查驱动链接状态..."

    if ! check_directories; then
        return 1
    fi

    local all_ok=true

    # 检查显示驱动
    if [ -d "$DISPLAY_DRIVERS_DIR" ]; then
        for driver in "$DISPLAY_DRIVERS_DIR"/*.c "$DISPLAY_DRIVERS_DIR"/*.h; do
            if [ -f "$driver" ]; then
                local filename=$(basename "$driver")
                local target="$NUTTX_DISPLAY_DIR/$filename"
                if [ -L "$target" ]; then
                    if [ -e "$target" ]; then
                        log_success "显示驱动链接正常: $filename"
                    else
                        log_error "显示驱动链接损坏: $filename"
                        all_ok=false
                    fi
                else
                    log_warning "显示驱动未链接: $filename"
                fi
            fi
        done
    fi

    # 检查触摸驱动
    if [ -d "$INPUT_DRIVERS_DIR" ]; then
        for driver in "$INPUT_DRIVERS_DIR"/*.c "$INPUT_DRIVERS_DIR"/*.h; do
            if [ -f "$driver" ]; then
                local filename=$(basename "$driver")
                local target="$NUTTX_INPUT_DIR/$filename"
                if [ -L "$target" ]; then
                    if [ -e "$target" ]; then
                        log_success "触摸驱动链接正常: $filename"
                    else
                        log_error "触摸驱动链接损坏: $filename"
                        all_ok=false
                    fi
                else
                    log_warning "触摸驱动未链接: $filename"
                fi
            fi
        done
    fi

    if $all_ok; then
        log_success "所有驱动链接检查通过"
        return 0
    else
        log_error "部分驱动链接检查失败"
        return 1
    fi
}

# 清理驱动链接
clean_driver_links() {
    log "清理驱动链接..."

    if ! check_directories; then
        return 1
    fi

    # 清理显示驱动链接
    if [ -d "$NUTTX_DISPLAY_DIR" ]; then
        for link in "$NUTTX_DISPLAY_DIR"/*.c "$NUTTX_DISPLAY_DIR"/*.h; do
            if [ -L "$link" ]; then
                local filename=$(basename "$link")
                # 只清理指向我们驱动目录的链接
                if [[ "$(readlink "$link")" == *"vendor_esp32p4/drivers"* ]]; then
                    rm -f "$link"
                    log_success "清理显示驱动链接: $filename"
                fi
            fi
        done
    fi

    # 清理触摸驱动链接
    if [ -d "$NUTTX_INPUT_DIR" ]; then
        for link in "$NUTTX_INPUT_DIR"/*.c "$NUTTX_INPUT_DIR"/*.h; do
            if [ -L "$link" ]; then
                local filename=$(basename "$link")
                # 只清理指向我们驱动目录的链接
                if [[ "$(readlink "$link")" == *"vendor_esp32p4/drivers"* ]]; then
                    rm -f "$link"
                    log_success "清理触摸驱动链接: $filename"
                fi
            fi
        done
    fi

    log_success "驱动链接清理完成"
    return 0
}

# 显示驱动状态
show_driver_status() {
    echo "ESP32-P4 驱动状态"
    echo ""
    echo "驱动目录结构:"
    echo "  $DRIVERS_DIR"
    echo ""

    if [ -d "$DISPLAY_DRIVERS_DIR" ]; then
        echo "显示驱动:"
        ls -la "$DISPLAY_DRIVERS_DIR" 2>/dev/null | grep -E "\.(c|h)$" | awk '{print "  " $NF}'
    else
        echo "显示驱动目录不存在"
    fi

    echo ""

    if [ -d "$INPUT_DRIVERS_DIR" ]; then
        echo "触摸驱动:"
        ls -la "$INPUT_DRIVERS_DIR" 2>/dev/null | grep -E "\.(c|h)$" | awk '{print "  " $NF}'
    else
        echo "触摸驱动目录不存在"
    fi

    echo ""
    echo "NuttX 驱动目录:"
    echo "  显示: $NUTTX_DISPLAY_DIR"
    echo "  触摸: $NUTTX_INPUT_DIR"
    echo ""

    echo "链接状态:"
    check_driver_links > /dev/null 2>&1
    if [ $? -eq 0 ]; then
        echo -e "  ${GREEN}✓ 所有驱动链接正常${NC}"
    else
        echo -e "  ${RED}✗ 部分驱动链接异常${NC}"
    fi
}

# 显示日志
show_log() {
    if [ -f "$LOG_FILE" ]; then
        echo "驱动链接日志:"
        echo "=================="
        tail -20 "$LOG_FILE"
    else
        echo "日志文件不存在"
    fi
}

# 主函数
main() {
    case "${1:-}" in
        --help|-h)
            show_help
            ;;
        --link|-l)
            link_all_drivers
            ;;
        --check|-c)
            check_driver_links
            ;;
        --clean)
            clean_driver_links
            ;;
        --status|-s)
            show_driver_status
            ;;
        --log)
            show_log
            ;;
        "")
            show_help
            ;;
        *)
            log_error "未知选项: $1"
            show_help
            exit 1
            ;;
    esac
}

# 执行主函数
main "$@"
