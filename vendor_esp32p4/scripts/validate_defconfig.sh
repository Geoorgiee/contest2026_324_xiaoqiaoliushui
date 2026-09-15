#!/bin/bash
# ESP32-P4 defconfig 验证脚本
# 简化版，快速检查 defconfig 配置

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

# 检查单个配置
validate_config() {
    local config=$1
    local defconfig="$VENDOR_DIR/boards/risc-v/esp32p4/esp32p4-evb/configs/$config/defconfig"

    echo -e "\n${BLUE}验证配置: $config${NC}"

    if [ ! -f "$defconfig" ]; then
        echo -e "${RED}✗ defconfig 不存在${NC}"
        return 1
    fi

    echo -e "${GREEN}✓ defconfig 存在${NC}"

    # 检查必要配置
    local has_error=false

    # 检查架构
    if grep -q 'CONFIG_ARCH="risc-v"' "$defconfig"; then
        echo -e "${GREEN}✓ 架构: RISC-V${NC}"
    else
        echo -e "${RED}✗ 架构配置错误${NC}"
        has_error=true
    fi

    # 检查芯片
    if grep -q 'CONFIG_ARCH_CHIP="esp32p4"' "$defconfig"; then
        echo -e "${GREEN}✓ 芯片: ESP32-P4${NC}"
    else
        echo -e "${RED}✗ 芯片配置错误${NC}"
        has_error=true
    fi

    # 检查自定义目录
    if grep -q 'CONFIG_ARCH_BOARD_CUSTOM=y' "$defconfig"; then
        echo -e "${GREEN}✓ 使用自定义板级目录${NC}"
    else
        echo -e "${YELLOW}⚠ 未使用自定义板级目录${NC}"
    fi

    # 检查串口控制台
    if grep -q 'CONFIG_ESP32P4_UART0_SERIAL_CONSOLE=y' "$defconfig"; then
        echo -e "${GREEN}✓ UART0 串口控制台已启用${NC}"
    else
        echo -e "${RED}✗ UART0 串口控制台未启用${NC}"
        has_error=true
    fi

    # 检查 NSH
    if grep -q 'CONFIG_SYSTEM_NSH=y' "$defconfig"; then
        echo -e "${GREEN}✓ NSH 已启用${NC}"
    else
        echo -e "${RED}✗ NSH 未启用${NC}"
        has_error=true
    fi

    # 检查入口点
    if grep -q 'CONFIG_INIT_ENTRYPOINT="nsh_main"' "$defconfig"; then
        echo -e "${GREEN}✓ 入口点: nsh_main${NC}"
    else
        echo -e "${RED}✗ 入口点配置错误${NC}"
        has_error=true
    fi

    # 检查内存配置
    if grep -q 'CONFIG_RAM_START=0x4FF00000' "$defconfig"; then
        echo -e "${GREEN}✓ RAM 起始地址正确${NC}"
    else
        echo -e "${YELLOW}⚠ RAM 起始地址可能不正确${NC}"
    fi

    # 检查 PSRAM
    if grep -q 'CONFIG_ESP32P4_PSRAM=y' "$defconfig"; then
        echo -e "${GREEN}✓ PSRAM 已启用${NC}"
    else
        echo -e "${YELLOW}⚠ PSRAM 未启用${NC}"
    fi

    if $has_error; then
        echo -e "${RED}✗ 配置有错误${NC}"
        return 1
    else
        echo -e "${GREEN}✓ 配置验证通过${NC}"
        return 0
    fi
}

# 主函数
main() {
    echo -e "${BLUE}ESP32-P4 defconfig 快速验证${NC}"
    echo -e "${BLUE}================================${NC}"

    local all_ok=true

    # 验证所有配置
    for config in nsh uart0 i2c display touch lvgl full; do
        if ! validate_config "$config"; then
            all_ok=false
        fi
    done

    echo -e "\n${BLUE}================================${NC}"
    if $all_ok; then
        echo -e "${GREEN}✓ 所有配置验证通过${NC}"
    else
        echo -e "${RED}✗ 部分配置验证失败${NC}"
    fi
}

# 执行主函数
main
