#!/bin/bash
# ESP32-P4 自检脚本
# 检查工程是否具备启动和烧录条件

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

# 检查项目结构
check_project_structure() {
    echo -e "\n${BLUE}1. 检查项目结构${NC}"

    # 检查 vendor 目录
    if [ -d "$VENDOR_DIR" ]; then
        print_check "PASS" "vendor_esp32p4 目录存在"
    else
        print_check "FAIL" "vendor_esp32p4 目录不存在"
    fi

    # 检查 NuttX 目录
    if [ -d "$NUTTX_DIR" ]; then
        print_check "PASS" "nuttx 目录存在"
    else
        print_check "FAIL" "nuttx 目录不存在"
    fi

    # 检查构建脚本
    if [ -f "$PROJECT_ROOT/build.sh" ]; then
        print_check "PASS" "build.sh 构建脚本存在"
    else
        print_check "FAIL" "build.sh 构建脚本不存在"
    fi

    # 检查配置目录
    local config_dir="$VENDOR_DIR/boards/risc-v/esp32p4/esp32p4-evb/configs"
    if [ -d "$config_dir" ]; then
        local config_count=$(ls -d "$config_dir"/*/ 2>/dev/null | wc -l)
        print_check "PASS" "配置目录存在 ($config_count 个配置)"
    else
        print_check "FAIL" "配置目录不存在"
    fi
}

# 检查构建环境
check_build_environment() {
    echo -e "\n${BLUE}2. 检查构建环境${NC}"

    # 检查 esptool
    if command -v esptool &> /dev/null; then
        local esptool_version=$(esptool version 2>/dev/null | head -1)
        print_check "PASS" "esptool 已安装" "版本: $esptool_version"
    else
        print_check "FAIL" "esptool 未安装" "请运行: pip install esptool"
    fi

    # 检查 RISC-V 工具链
    if command -v riscv32-esp-elf-gcc &> /dev/null; then
        local gcc_version=$(riscv32-esp-elf-gcc --version 2>/dev/null | head -1)
        print_check "PASS" "RISC-V 工具链已安装" "版本: $gcc_version"
    else
        print_check "WARN" "RISC-V 工具链未找到" "可能需要设置 PATH 或安装工具链"
    fi

    # 检查 Python
    if command -v python3 &> /dev/null; then
        local python_version=$(python3 --version 2>/dev/null)
        print_check "PASS" "Python3 已安装" "版本: $python_version"
    else
        print_check "FAIL" "Python3 未安装"
    fi

    # 检查 picocom
    if command -v picocom &> /dev/null; then
        print_check "PASS" "picocom 已安装"
    else
        print_check "WARN" "picocom 未安装" "串口终端工具，可选安装"
    fi
}

# 检查固件文件
check_firmware() {
    echo -e "\n${BLUE}3. 检查固件文件${NC}"

    local firmware="$NUTTX_DIR/nuttx.bin"

    if [ -f "$firmware" ]; then
        local size=$(stat -c%s "$firmware")
        local size_human=$(ls -lh "$firmware" | awk '{print $5}')
        print_check "PASS" "固件文件存在" "大小: $size_human ($size bytes)"

        # 检查固件大小是否合理
        if [ "$size" -gt 1000 ] && [ "$size" -lt 10000000 ]; then
            print_check "PASS" "固件大小合理"
        else
            print_check "WARN" "固件大小异常" "可能不是有效的固件"
        fi

        # 计算 MD5
        local md5=$(md5sum "$firmware" 2>/dev/null | awk '{print $1}')
        if [ -n "$md5" ]; then
            print_check "PASS" "固件 MD5: $md5"
        fi
    else
        print_check "FAIL" "固件文件不存在" "请先构建固件: ./vendor_esp32p4/scripts/build.sh nsh"
    fi
}

# 检查串口设备
check_serial_ports() {
    echo -e "\n${BLUE}4. 检查串口设备${NC}"

    # 检查常见串口设备
    local found_serial=false

    for port in /dev/ttyACM0 /dev/ttyUSB0 /dev/ttyACM1 /dev/ttyUSB1; do
        if [ -e "$port" ]; then
            local permissions=$(ls -la "$port" | awk '{print $1}')
            local group=$(ls -la "$port" | awk '{print $4}')
            print_check "PASS" "串口设备存在: $port" "权限: $permissions, 组: $group"
            found_serial=true
        fi
    done

    if ! $found_serial; then
        print_check "WARN" "未找到常见串口设备" "请连接 ESP32-P4 开发板到 USB"

        # 列出所有 tty 设备
        echo -e "  ${YELLOW}可用串口设备:${NC}"
        ls /dev/tty{ACM,USB,S}* 2>/dev/null | head -5 | while read -r port; do
            echo "    - $port"
        done
    fi

    # 检查用户是否在 dialout 组
    if groups | grep -q "dialout"; then
        print_check "PASS" "用户在 dialout 组中"
    else
        print_check "FAIL" "用户不在 dialout 组中" "请运行: sudo usermod -a -G dialout \$USER"
    fi
}

# 检查驱动链接
check_driver_links() {
    echo -e "\n${BLUE}5. 检查驱动链接${NC}"

    # 检查驱动管理脚本
    if [ -f "$VENDOR_DIR/scripts/link_drivers.sh" ]; then
        print_check "PASS" "驱动管理脚本存在"
    else
        print_check "FAIL" "驱动管理脚本不存在"
        return
    fi

    # 检查驱动源文件
    local drivers_dir="$VENDOR_DIR/drivers"
    if [ -d "$drivers_dir" ]; then
        local driver_count=$(find "$drivers_dir" -name "*.c" -o -name "*.h" 2>/dev/null | wc -l)
        print_check "PASS" "驱动目录存在 ($driver_count 个文件)"
    else
        print_check "WARN" "驱动目录不存在" "可能需要创建驱动目录"
    fi
}

# 检查构建配置
check_build_configs() {
    echo -e "\n${BLUE}6. 检查构建配置${NC}"

    local config_dir="$VENDOR_DIR/boards/risc-v/esp32p4/esp32p4-evb/configs"

    if [ ! -d "$config_dir" ]; then
        print_check "FAIL" "配置目录不存在"
        return
    fi

    # 检查每个配置
    for config in nsh uart0 i2c display touch lvgl full; do
        local config_path="$config_dir/$config"
        if [ -d "$config_path" ]; then
            if [ -f "$config_path/defconfig" ]; then
                print_check "PASS" "配置 '$config' 存在且有 defconfig"
            else
                print_check "WARN" "配置 '$config' 存在但缺少 defconfig"
            fi
        else
            print_check "WARN" "配置 '$config' 不存在"
        fi
    done
}

# 检查证据目录
check_evidence_structure() {
    echo -e "\n${BLUE}7. 检查证据目录${NC}"

    local evidence_dir="$VENDOR_DIR/docs/evidence"

    if [ -d "$evidence_dir" ]; then
        print_check "PASS" "证据目录存在"

        # 检查子目录
        for subdir in build-logs flash-logs verification-logs measurement-data photos; do
            if [ -d "$evidence_dir/$subdir" ]; then
                local file_count=$(find "$evidence_dir/$subdir" -type f 2>/dev/null | wc -l)
                print_check "PASS" "证据子目录 '$subdir' 存在 ($file_count 个文件)"
            else
                print_check "WARN" "证据子目录 '$subdir' 不存在"
            fi
        done
    else
        print_check "WARN" "证据目录不存在" "可运行: ./vendor_esp32p4/scripts/collect_evidence.sh --init"
    fi
}

# 检查文档
check_documentation() {
    echo -e "\n${BLUE}8. 检查文档${NC}"

    # 检查参考工程分析
    local reference_dir="$VENDOR_DIR/docs/reference"
    if [ -d "$reference_dir" ]; then
        local doc_count=$(find "$reference_dir" -name "*.md" 2>/dev/null | wc -l)
        print_check "PASS" "参考工程分析文档存在 ($doc_count 个文档)"
    else
        print_check "WARN" "参考工程分析文档不存在"
    fi

    # 检查配置指南
    local config_guide="$VENDOR_DIR/boards/risc-v/esp32p4/esp32p4-evb/configs/CONFIGS_GUIDE.md"
    if [ -f "$config_guide" ]; then
        print_check "PASS" "配置指南文档存在"
    else
        print_check "WARN" "配置指南文档不存在"
    fi

    # 检查改进总结
    local improvement_summary="$VENDOR_DIR/docs/IMPROVEMENT_SUMMARY.md"
    if [ -f "$improvement_summary" ]; then
        print_check "PASS" "改进总结文档存在"
    else
        print_check "WARN" "改进总结文档不存在"
    fi
}

# 检查脚本权限
check_script_permissions() {
    echo -e "\n${BLUE}9. 检查脚本权限${NC}"

    local scripts_dir="$VENDOR_DIR/scripts"

    if [ ! -d "$scripts_dir" ]; then
        print_check "FAIL" "脚本目录不存在"
        return
    fi

    for script in build.sh flash.sh link_drivers.sh collect_evidence.sh self_check.sh; do
        local script_path="$scripts_dir/$script"
        if [ -f "$script_path" ]; then
            if [ -x "$script_path" ]; then
                print_check "PASS" "脚本 '$script' 存在且可执行"
            else
                print_check "WARN" "脚本 '$script' 存在但不可执行" "请运行: chmod +x $script_path"
            fi
        else
            print_check "WARN" "脚本 '$script' 不存在"
        fi
    done
}

# 生成自检报告
generate_report() {
    echo -e "\n${BLUE}========================================${NC}"
    echo -e "${BLUE}自检报告${NC}"
    echo -e "${BLUE}========================================${NC}"

    echo -e "\n检查结果:"
    echo -e "  ${GREEN}通过: $CHECK_PASS${NC}"
    echo -e "  ${YELLOW}警告: $CHECK_WARN${NC}"
    echo -e "  ${RED}失败: $CHECK_FAIL${NC}"

    echo -e "\n总结:"
    if [ $CHECK_FAIL -eq 0 ]; then
        if [ $CHECK_WARN -eq 0 ]; then
            echo -e "  ${GREEN}✓ 所有检查通过，工程状态良好${NC}"
        else
            echo -e "  ${YELLOW}⚠ 有 $CHECK_WARN 个警告，但基本功能可用${NC}"
        fi
    else
        echo -e "  ${RED}✗ 有 $CHECK_FAIL 个检查失败，需要修复${NC}"
    fi

    echo -e "\n建议操作:"
    if [ $CHECK_FAIL -gt 0 ]; then
        echo -e "  1. 修复所有失败的检查项"
        echo -e "  2. 重新运行自检: ./vendor_esp32p4/scripts/self_check.sh"
    fi

    if [ $CHECK_WARN -gt 0 ]; then
        echo -e "  3. 处理警告项以优化工程状态"
    fi

    echo -e "\n下一步:"
    echo -e "  1. 连接 ESP32-P4 开发板到 USB"
    echo -e "  2. 构建固件: ./vendor_esp32p4/scripts/build.sh nsh"
    echo -e "  3. 烧录固件: ./vendor_esp32p4/scripts/flash.sh"
    echo -e "  4. 连接串口: picocom -b 115200 /dev/ttyACM0"
    echo -e "  5. 验证启动: 输入 'help' 命令"
}

# 主函数
main() {
    echo -e "${BLUE}ESP32-P4 工程自检${NC}"
    echo -e "${BLUE}========================================${NC}"

    # 执行所有检查
    check_project_structure
    check_build_environment
    check_firmware
    check_serial_ports
    check_driver_links
    check_build_configs
    check_evidence_structure
    check_documentation
    check_script_permissions

    # 生成报告
    generate_report
}

# 执行主函数
main
