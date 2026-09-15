#!/bin/bash
# ESP32-P4 defconfig 自检脚本
# 检查 defconfig 是否符合最小 NSH 基线要求

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

# 检查结果
CHECK_PASS=0
CHECK_FAIL=0
CHECK_WARN=0

# 打印检查结果
print_check() {
    local status=$1
    local message=$2
    local detail=${3:-""}

    if [ "$status" = "PASS" ]; then
        echo -e "${GREEN}✓${NC} $message"
        ((CHECK_PASS++))
    elif [ "$status" = "FAIL" ]; then
        echo -e "${RED}✗${NC} $message"
        if [ -n "$detail" ]; then
            echo -e "  ${RED}原因: $detail${NC}"
        fi
        ((CHECK_FAIL++))
    elif [ "$status" = "WARN" ]; then
        echo -e "${YELLOW}⚠${NC} $message"
        if [ -n "$detail" ]; then
            echo -e "  ${YELLOW}警告: $detail${NC}"
        fi
        ((CHECK_WARN++))
    fi
}

# 检查 defconfig 文件
check_defconfig() {
    local config=$1
    local config_dir="$VENDOR_DIR/boards/risc-v/esp32p4/esp32p4-evb/configs/$config"
    local defconfig="$config_dir/defconfig"

    echo -e "\n${BLUE}检查配置: $config${NC}"

    # 检查配置目录
    if [ ! -d "$config_dir" ]; then
        print_check "FAIL" "配置目录不存在: $config_dir"
        return 1
    fi

    # 检查 defconfig 文件
    if [ ! -f "$defconfig" ]; then
        print_check "FAIL" "defconfig 文件不存在: $defconfig"
        return 1
    fi

    print_check "PASS" "defconfig 文件存在"

    # 检查必要的配置项
    check_required_config "$defconfig"

    # 检查架构配置
    check_arch_config "$defconfig"

    # 检查串口配置
    check_uart_config "$defconfig"

    # 检查内存配置
    check_memory_config "$defconfig"

    # 检查 NSH 配置
    check_nsh_config "$defconfig"

    # 检查可选配置
    check_optional_config "$defconfig" "$config"
}

# 检查必要的配置项
check_required_config() {
    local defconfig=$1

    # 检查架构配置
    if grep -q 'CONFIG_ARCH="risc-v"' "$defconfig"; then
        print_check "PASS" "架构配置正确: RISC-V"
    else
        print_check "FAIL" "架构配置错误" "需要 CONFIG_ARCH=\"risc-v\""
    fi

    # 检查芯片配置
    if grep -q 'CONFIG_ARCH_CHIP="esp32p4"' "$defconfig"; then
        print_check "PASS" "芯片配置正确: ESP32-P4"
    else
        print_check "FAIL" "芯片配置错误" "需要 CONFIG_ARCH_CHIP=\"esp32p4\""
    fi

    # 检查板级配置
    if grep -q 'CONFIG_ARCH_BOARD_CUSTOM=y' "$defconfig"; then
        print_check "PASS" "使用自定义板级配置"
    else
        print_check "WARN" "未使用自定义板级配置"
    fi

    # 检查自定义板级目录
    if grep -q 'CONFIG_ARCH_BOARD_CUSTOM_DIR=' "$defconfig"; then
        local board_dir=$(grep 'CONFIG_ARCH_BOARD_CUSTOM_DIR=' "$defconfig" | cut -d'"' -f2)
        print_check "PASS" "板级目录配置: $board_dir"
    else
        print_check "FAIL" "未配置板级目录"
    fi

    # 检查自定义芯片目录
    if grep -q 'CONFIG_ARCH_CHIP_CUSTOM_DIR=' "$defconfig"; then
        local chip_dir=$(grep 'CONFIG_ARCH_CHIP_CUSTOM_DIR=' "$defconfig" | cut -d'"' -f2)
        print_check "PASS" "芯片目录配置: $chip_dir"
    else
        print_check "FAIL" "未配置芯片目录"
    fi
}

# 检查架构配置
check_arch_config() {
    local defconfig=$1

    # 检查 RISC-V 配置
    if grep -q 'CONFIG_ARCH_RISCV=y' "$defconfig"; then
        print_check "PASS" "RISC-V 架构已启用"
    else
        print_check "FAIL" "RISC-V 架构未启用"
    fi

    # 检查 RV32 配置
    if grep -q 'CONFIG_ARCH_RV32=y' "$defconfig"; then
        print_check "PASS" "RV32 配置正确"
    else
        print_check "WARN" "RV32 配置未找到"
    fi

    # 检查中断栈大小
    if grep -q 'CONFIG_ARCH_INTERRUPTSTACK=' "$defconfig"; then
        local stack_size=$(grep 'CONFIG_ARCH_INTERRUPTSTACK=' "$defconfig" | cut -d'=' -f2)
        if [ "$stack_size" -ge 2048 ]; then
            print_check "PASS" "中断栈大小: $stack_size bytes"
        else
            print_check "WARN" "中断栈大小偏小: $stack_size bytes" "建议至少 2048 bytes"
        fi
    else
        print_check "WARN" "未配置中断栈大小"
    fi
}

# 检查串口配置
check_uart_config() {
    local defconfig=$1

    # 检查 UART0 配置
    if grep -q 'CONFIG_ESP32P4_UART0=y' "$defconfig"; then
        print_check "PASS" "UART0 已启用"
    else
        print_check "FAIL" "UART0 未启用"
    fi

    # 检查串口控制台
    if grep -q 'CONFIG_ESP32P4_UART0_SERIAL_CONSOLE=y' "$defconfig"; then
        print_check "PASS" "UART0 作为串口控制台"
    else
        print_check "FAIL" "UART0 未配置为串口控制台"
    fi

    # 检查 UART1 配置（用于 syslog）
    if grep -q 'CONFIG_ESP32P4_UART1=y' "$defconfig"; then
        print_check "PASS" "UART1 已启用（用于 syslog）"
    else
        print_check "WARN" "UART1 未启用" "建议启用用于 syslog 输出"
    fi
}

# 检查内存配置
check_memory_config() {
    local defconfig=$1

    # 检查 RAM 起始地址
    if grep -q 'CONFIG_RAM_START=' "$defconfig"; then
        local ram_start=$(grep 'CONFIG_RAM_START=' "$defconfig" | cut -d'=' -f2)
        if [ "$ram_start" = "0x4FF00000" ]; then
            print_check "PASS" "RAM 起始地址正确: $ram_start"
        else
            print_check "WARN" "RAM 起始地址: $ram_start" "ESP32-P4 应为 0x4FF00000"
        fi
    else
        print_check "FAIL" "未配置 RAM 起始地址"
    fi

    # 检查 RAM 大小
    if grep -q 'CONFIG_RAM_SIZE=' "$defconfig"; then
        local ram_size=$(grep 'CONFIG_RAM_SIZE=' "$defconfig" | cut -d'=' -f2)
        local ram_size_kb=$((ram_size / 1024))
        if [ "$ram_size_kb" -eq 768 ]; then
            print_check "PASS" "RAM 大小正确: ${ram_size_kb}KB"
        else
            print_check "WARN" "RAM 大小: ${ram_size_kb}KB" "ESP32-P4 应为 768KB"
        fi
    else
        print_check "FAIL" "未配置 RAM 大小"
    fi

    # 检查 PSRAM 配置
    if grep -q 'CONFIG_ESP32P4_PSRAM=y' "$defconfig"; then
        print_check "PASS" "PSRAM 已启用"
    else
        print_check "WARN" "PSRAM 未启用" "建议启用 32MB PSRAM"
    fi

    # 检查内存区域数
    if grep -q 'CONFIG_MM_REGIONS=' "$defconfig"; then
        local regions=$(grep 'CONFIG_MM_REGIONS=' "$defconfig" | cut -d'=' -f2)
        if [ "$regions" -ge 2 ]; then
            print_check "PASS" "内存区域数: $regions"
        else
            print_check "WARN" "内存区域数偏少: $regions" "建议至少 2（SRAM + PSRAM）"
        fi
    else
        print_check "WARN" "未配置内存区域数"
    fi
}

# 检查 NSH 配置
check_nsh_config() {
    local defconfig=$1

    # 检查 NSH 启用
    if grep -q 'CONFIG_SYSTEM_NSH=y' "$defconfig"; then
        print_check "PASS" "NSH 已启用"
    else
        print_check "FAIL" "NSH 未启用"
    fi

    # 检查 NSH 入口点
    if grep -q 'CONFIG_INIT_ENTRYPOINT="nsh_main"' "$defconfig"; then
        print_check "PASS" "NSH 入口点配置正确"
    else
        print_check "FAIL" "NSH 入口点配置错误" "需要 CONFIG_INIT_ENTRYPOINT=\"nsh_main\""
    fi

    # 检查内置应用
    if grep -q 'CONFIG_NSH_BUILTIN_APPS=y' "$defconfig"; then
        print_check "PASS" "NSH 内置应用已启用"
    else
        print_check "WARN" "NSH 内置应用未启用"
    fi

    # 检查 readline
    if grep -q 'CONFIG_NSH_READLINE=y' "$defconfig"; then
        print_check "PASS" "NSH readline 已启用"
    else
        print_check "WARN" "NSH readline 未启用"
    fi

    # 检查 procfs
    if grep -q 'CONFIG_FS_PROCFS=y' "$defconfig"; then
        print_check "PASS" "procfs 文件系统已启用"
    else
        print_check "WARN" "procfs 文件系统未启用"
    fi
}

# 检查可选配置
check_optional_config() {
    local defconfig=$1
    local config=$2

    # 根据配置类型检查不同的可选配置
    case "$config" in
        display)
            # 检查显示相关配置
            if grep -q 'CONFIG_ESP32P4_DSI=y' "$defconfig"; then
                print_check "PASS" "DSI 显示已启用"
            else
                print_check "WARN" "DSI 显示未启用"
            fi

            if grep -q 'CONFIG_VIDEO_FRAME_BUFFER=y' "$defconfig"; then
                print_check "PASS" "framebuffer 已启用"
            else
                print_check "WARN" "framebuffer 未启用"
            fi
            ;;
        touch)
            # 检查触摸相关配置
            if grep -q 'CONFIG_INPUT_GT911=y' "$defconfig"; then
                print_check "PASS" "GT911 触摸驱动已启用"
            else
                print_check "WARN" "GT911 触摸驱动未启用"
            fi
            ;;
        lvgl)
            # 检查 LVGL 相关配置
            if grep -q 'CONFIG_GRAPHICS_LVGL=y' "$defconfig"; then
                print_check "PASS" "LVGL 已启用"
            else
                print_check "WARN" "LVGL 未启用"
            fi
            ;;
        i2c)
            # 检查 I2C 相关配置
            if grep -q 'CONFIG_ESP32P4_I2C0=y' "$defconfig"; then
                print_check "PASS" "I2C0 已启用"
            else
                print_check "WARN" "I2C0 未启用"
            fi
            ;;
    esac
}

# 生成检查报告
generate_report() {
    echo -e "\n${BLUE}========================================${NC}"
    echo -e "${BLUE}defconfig 检查报告${NC}"
    echo -e "${BLUE}========================================${NC}"

    echo -e "\n检查结果:"
    echo -e "  ${GREEN}通过: $CHECK_PASS${NC}"
    echo -e "  ${YELLOW}警告: $CHECK_WARN${NC}"
    echo -e "  ${RED}失败: $CHECK_FAIL${NC}"

    echo -e "\n总结:"
    if [ $CHECK_FAIL -eq 0 ]; then
        if [ $CHECK_WARN -eq 0 ]; then
            echo -e "  ${GREEN}✓ defconfig 配置完整，符合最小 NSH 基线要求${NC}"
        else
            echo -e "  ${YELLOW}⚠ defconfig 基本可用，但有 $CHECK_WARN 个警告${NC}"
        fi
    else
        echo -e "  ${RED}✗ defconfig 有 $CHECK_FAIL 个配置错误，需要修复${NC}"
    fi

    echo -e "\n建议:"
    if [ $CHECK_FAIL -gt 0 ]; then
        echo -e "  1. 修复所有失败的配置项"
        echo -e "  2. 重新运行检查: ./vendor_esp32p4/scripts/check_defconfig.sh"
    fi

    if [ $CHECK_WARN -gt 0 ]; then
        echo -e "  3. 考虑处理警告项以优化配置"
    fi
}

# 主函数
main() {
    echo -e "${BLUE}ESP32-P4 defconfig 自检${NC}"
    echo -e "${BLUE}========================================${NC}"

    # 检查所有配置
    for config in nsh uart0 i2c display touch lvgl full; do
        check_defconfig "$config"
    done

    # 生成报告
    generate_report
}

# 执行主函数
main
