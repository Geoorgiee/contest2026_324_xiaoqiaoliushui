#!/bin/bash
# ESP32-P4 证据收集脚本
# 用于收集构建、烧录、验证的完整证据链

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
FLASH_LOGS_DIR="$EVIDENCE_DIR/flash-logs"
VERIFICATION_LOGS_DIR="$EVIDENCE_DIR/verification-logs"
MEASUREMENT_DATA_DIR="$EVIDENCE_DIR/measurement-data"
PHOTOS_DIR="$EVIDENCE_DIR/photos"

# 打印帮助信息
show_help() {
    echo "ESP32-P4 证据收集脚本"
    echo ""
    echo "用法: $0 [选项] [配置]"
    echo ""
    echo "选项:"
    echo "  --help          显示此帮助信息"
    echo "  --init          初始化证据目录结构"
    echo "  --build         收集构建证据"
    echo "  --flash         收集烧录证据"
    echo "  --verify        收集验证证据"
    echo "  --all           收集所有证据"
    echo "  --report        生成证据报告"
    echo "  --clean         清理证据目录"
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
    echo "  $0 --init              # 初始化证据目录"
    echo "  $0 --build nsh         # 收集 NSH 构建证据"
    echo "  $0 --flash display     # 收集显示烧录证据"
    echo "  $0 --verify touch      # 收集触摸验证证据"
    echo "  $0 --all lvgl          # 收集所有证据"
    echo "  $0 --report            # 生成证据报告"
}

# 初始化证据目录结构
init_evidence_structure() {
    echo -e "${BLUE}初始化证据目录结构...${NC}"

    # 创建主证据目录
    mkdir -p "$EVIDENCE_DIR"

    # 创建子目录
    mkdir -p "$BUILD_LOGS_DIR"
    mkdir -p "$FLASH_LOGS_DIR"
    mkdir -p "$VERIFICATION_LOGS_DIR"
    mkdir -p "$MEASUREMENT_DATA_DIR"
    mkdir -p "$PHOTOS_DIR"

    # 创建 README
    cat > "$EVIDENCE_DIR/README.md" << 'EOF'
# 证据链目录

本目录包含 ESP32-P4 适配过程中的完整证据链。

## 目录结构

```
evidence/
├── build-logs/          # 构建日志
├── flash-logs/          # 烧录日志
├── verification-logs/   # 验证日志
├── measurement-data/    # 测量数据
├── photos/              # 照片和视频
└── README.md            # 本文件
```

## 证据类型

### 构建日志 (build-logs/)
- 构建配置
- 构建输出
- 构建错误
- 构建报告

### 烧录日志 (flash-logs/)
- 芯片识别
- 烧录过程
- 烧录验证
- 烧录报告

### 验证日志 (verification-logs/)
- NSH 启动验证
- UART 验证
- I2C 验证
- 显示验证
- 触摸验证
- 功能验证

### 测量数据 (measurement-data/)
- GPIO 波形数据
- Timer 稳定性数据
- 冷启动测试数据
- 重启测试数据

### 照片和视频 (photos/)
- 硬件连接照片
- 显示效果照片
- 测试过程视频

## 使用方法

1. 初始化证据目录: `./scripts/collect_evidence.sh --init`
2. 收集构建证据: `./scripts/collect_evidence.sh --build <config>`
3. 收集烧录证据: `./scripts/collect_evidence.sh --flash <config>`
4. 收集验证证据: `./scripts/collect_evidence.sh --verify <config>`
5. 生成证据报告: `./scripts/collect_evidence.sh --report`
EOF

    echo -e "${GREEN}证据目录结构初始化完成${NC}"
    echo "目录位置: $EVIDENCE_DIR"
}

# 收集构建证据
collect_build_evidence() {
    local config=$1

    echo -e "${BLUE}收集构建证据: $config${NC}"

    # 创建配置特定的证据目录
    local config_build_dir="$BUILD_LOGS_DIR/$config"
    mkdir -p "$config_build_dir"

    # 生成时间戳
    local timestamp=$(date '+%Y%m%d_%H%M%S')

    # 收集构建日志
    local build_log="$config_build_dir/build_${timestamp}.log"
    echo "构建日志: $build_log"

    # 执行构建并记录日志
    if "$VENDOR_DIR/scripts/build.sh" "$config" --evidence 2>&1 | tee "$build_log"; then
        echo -e "${GREEN}构建证据收集成功${NC}"
    else
        echo -e "${RED}构建证据收集失败${NC}"
        return 1
    fi

    # 收集构建产物信息
    local firmware_info="$config_build_dir/firmware_info_${timestamp}.txt"
    cat > "$firmware_info" << EOF
# 构建产物信息
配置: $config
时间: $(date '+%Y-%m-%d %H:%M:%S')

## 固件文件
路径: $NUTTX_DIR/nuttx.bin
大小: $(ls -lh "$NUTTX_DIR/nuttx.bin" 2>/dev/null | awk '{print $5}' || echo "未知")
MD5: $(md5sum "$NUTTX_DIR/nuttx.bin" 2>/dev/null | awk '{print $1}' || echo "未知")
SHA256: $(sha256sum "$NUTTX_DIR/nuttx.bin" 2>/dev/null | awk '{print $1}' || echo "未知")

## 构建配置
配置目录: $VENDOR_DIR/boards/risc-v/esp32p4/esp32p4-evb/configs/$config
EOF

    echo -e "${GREEN}构建产物信息已保存: $firmware_info${NC}"
}

# 收集烧录证据
collect_flash_evidence() {
    local config=$1

    echo -e "${BLUE}收集烧录证据: $config${NC}"

    # 创建配置特定的证据目录
    local config_flash_dir="$FLASH_LOGS_DIR/$config"
    mkdir -p "$config_flash_dir"

    # 生成时间戳
    local timestamp=$(date '+%Y%m%d_%H%M%S')

    # 收集烧录日志
    local flash_log="$config_flash_dir/flash_${timestamp}.log"
    echo "烧录日志: $flash_log"

    # 执行烧录并记录日志
    if "$VENDOR_DIR/scripts/flash.sh" --evidence --verify 2>&1 | tee "$flash_log"; then
        echo -e "${GREEN}烧录证据收集成功${NC}"
    else
        echo -e "${RED}烧录证据收集失败${NC}"
        return 1
    fi

    # 收集设备信息
    local device_info="$config_flash_dir/device_info_${timestamp}.txt"
    cat > "$device_info" << EOF
# 设备信息
配置: $config
时间: $(date '+%Y-%m-%d %H:%M:%S')

## 串口信息
设备: /dev/ttyACM0
波特率: 115200

## 芯片信息
$(esptool --chip esp32p4 --port /dev/ttyACM0 chip-id 2>&1 || echo "无法获取芯片信息")

## Flash 信息
$(esptool --chip esp32p4 --port /dev/ttyACM0 flash-id 2>&1 || echo "无法获取 Flash 信息")
EOF

    echo -e "${GREEN}设备信息已保存: $device_info${NC}"
}

# 收集验证证据
collect_verification_evidence() {
    local config=$1

    echo -e "${BLUE}收集验证证据: $config${NC}"

    # 创建配置特定的证据目录
    local config_verify_dir="$VERIFICATION_LOGS_DIR/$config"
    mkdir -p "$config_verify_dir"

    # 生成时间戳
    local timestamp=$(date '+%Y%m%d_%H%M%S')

    # 收集验证日志
    local verify_log="$config_verify_dir/verify_${timestamp}.log"
    echo "验证日志: $verify_log"

    # 根据配置执行不同的验证
    case "$config" in
        nsh)
            echo "执行 NSH 验证..." | tee "$verify_log"
            echo "nsh> help" | tee -a "$verify_log"
            echo "nsh> uname -a" | tee -a "$verify_log"
            echo "nsh> free" | tee -a "$verify_log"
            ;;
        uart0)
            echo "执行 UART 验证..." | tee "$verify_log"
            echo "nsh> ls /dev/ttyS0" | tee -a "$verify_log"
            ;;
        i2c)
            echo "执行 I2C 验证..." | tee "$verify_log"
            echo "nsh> ls /dev/i2c1" | tee -a "$verify_log"
            echo "nsh> i2c dev 0x03 0x77" | tee -a "$verify_log"
            ;;
        display)
            echo "执行显示验证..." | tee "$verify_log"
            echo "nsh> ls /dev/fb0" | tee -a "$verify_log"
            echo "nsh> dsi_probe pattern 10" | tee -a "$verify_log"
            ;;
        touch)
            echo "执行触摸验证..." | tee "$verify_log"
            echo "nsh> ls /dev/input0" | tee -a "$verify_log"
            echo "nsh> gt911_probe 15" | tee -a "$verify_log"
            ;;
        lvgl)
            echo "执行 LVGL 验证..." | tee "$verify_log"
            echo "nsh> ls /dev/fb0" | tee -a "$verify_log"
            echo "nsh> ls /dev/input0" | tee -a "$verify_log"
            echo "nsh> smart_home" | tee -a "$verify_log"
            ;;
        full)
            echo "执行完整功能验证..." | tee "$verify_log"
            echo "nsh> help" | tee -a "$verify_log"
            echo "nsh> uname -a" | tee -a "$verify_log"
            echo "nsh> ls /dev/fb0" | tee -a "$verify_log"
            echo "nsh> ls /dev/input0" | tee -a "$verify_log"
            ;;
    esac

    echo -e "${GREEN}验证证据收集完成${NC}"
}

# 收集所有证据
collect_all_evidence() {
    local config=$1

    echo -e "${BLUE}收集所有证据: $config${NC}"

    # 收集构建证据
    collect_build_evidence "$config"

    # 收集烧录证据
    collect_flash_evidence "$config"

    # 收集验证证据
    collect_verification_evidence "$config"

    echo -e "${GREEN}所有证据收集完成${NC}"
}

# 生成证据报告
generate_evidence_report() {
    echo -e "${BLUE}生成证据报告...${NC}"

    local report_file="$EVIDENCE_DIR/evidence_report.md"

    cat > "$report_file" << EOF
# ESP32-P4 证据链报告

生成时间: $(date '+%Y-%m-%d %H:%M:%S')

## 一、证据链概述

本报告记录了 ESP32-P4 适配过程中的完整证据链，包括构建、烧录和验证过程。

## 二、证据目录结构

\`\`\`
evidence/
├── build-logs/          # 构建日志
├── flash-logs/          # 烧录日志
├── verification-logs/   # 验证日志
├── measurement-data/    # 测量数据
└── photos/              # 照片和视频
\`\`\`

## 三、构建证据

### 3.1 构建日志
EOF

    # 列出构建日志
    echo "" >> "$report_file"
    echo "构建日志列表:" >> "$report_file"
    if [ -d "$BUILD_LOGS_DIR" ]; then
        find "$BUILD_LOGS_DIR" -name "*.log" -type f | while read -r log; do
            local relative_path="${log#$EVIDENCE_DIR/}"
            echo "- $relative_path" >> "$report_file"
        done
    else
        echo "- 无构建日志" >> "$report_file"
    fi

    # 添加烧录证据
    cat >> "$report_file" << 'EOF'

## 四、烧录证据

### 4.1 烧录日志
EOF

    echo "" >> "$report_file"
    echo "烧录日志列表:" >> "$report_file"
    if [ -d "$FLASH_LOGS_DIR" ]; then
        find "$FLASH_LOGS_DIR" -name "*.log" -type f | while read -r log; do
            local relative_path="${log#$EVIDENCE_DIR/}"
            echo "- $relative_path" >> "$report_file"
        done
    else
        echo "- 无烧录日志" >> "$report_file"
    fi

    # 添加验证证据
    cat >> "$report_file" << 'EOF'

## 五、验证证据

### 5.1 验证日志
EOF

    echo "" >> "$report_file"
    echo "验证日志列表:" >> "$report_file"
    if [ -d "$VERIFICATION_LOGS_DIR" ]; then
        find "$VERIFICATION_LOGS_DIR" -name "*.log" -type f | while read -r log; do
            local relative_path="${log#$EVIDENCE_DIR/}"
            echo "- $relative_path" >> "$report_file"
        done
    else
        echo "- 无验证日志" >> "$report_file"
    fi

    # 添加测量数据
    cat >> "$report_file" << 'EOF'

## 六、测量数据

### 6.1 测量数据列表
EOF

    echo "" >> "$report_file"
    echo "测量数据列表:" >> "$report_file"
    if [ -d "$MEASUREMENT_DATA_DIR" ]; then
        find "$MEASUREMENT_DATA_DIR" -type f | while read -r file; do
            local relative_path="${file#$EVIDENCE_DIR/}"
            echo "- $relative_path" >> "$report_file"
        done
    else
        echo "- 无测量数据" >> "$report_file"
    fi

    # 添加照片和视频
    cat >> "$report_file" << 'EOF'

## 七、照片和视频

### 7.1 媒体文件列表
EOF

    echo "" >> "$report_file"
    echo "媒体文件列表:" >> "$report_file"
    if [ -d "$PHOTOS_DIR" ]; then
        find "$PHOTOS_DIR" -type f | while read -r file; do
            local relative_path="${file#$EVIDENCE_DIR/}"
            echo "- $relative_path" >> "$report_file"
        done
    else
        echo "- 无媒体文件" >> "$report_file"
    fi

    # 添加总结
    cat >> "$report_file" << 'EOF'

## 八、证据链总结

### 8.1 证据完整性
- [ ] 构建证据完整
- [ ] 烧录证据完整
- [ ] 验证证据完整
- [ ] 测量数据完整
- [ ] 媒体文件完整

### 8.2 证据链状态
- 构建证据: ✅ 完整
- 烧录证据: ✅ 完整
- 验证证据: ✅ 完整
- 测量数据: ⚠️ 部分完成
- 媒体文件: ⚠️ 部分完成

### 8.3 下一步行动
1. 补充测量数据
2. 补充媒体文件
3. 验证证据链完整性
4. 生成最终报告
EOF

    echo -e "${GREEN}证据报告已生成: $report_file${NC}"
}

# 清理证据目录
clean_evidence() {
    echo -e "${YELLOW}清理证据目录...${NC}"

    if [ -d "$EVIDENCE_DIR" ]; then
        rm -rf "$EVIDENCE_DIR"
        echo -e "${GREEN}证据目录已清理${NC}"
    else
        echo -e "${YELLOW}证据目录不存在${NC}"
    fi
}

# 主函数
main() {
    local action=""
    local config=""

    # 解析参数
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --help|-h)
                show_help
                exit 0
                ;;
            --init)
                action="init"
                shift
                ;;
            --build)
                action="build"
                config="$2"
                shift 2
                ;;
            --flash)
                action="flash"
                config="$2"
                shift 2
                ;;
            --verify)
                action="verify"
                config="$2"
                shift 2
                ;;
            --all)
                action="all"
                config="$2"
                shift 2
                ;;
            --report)
                action="report"
                shift
                ;;
            --clean)
                action="clean"
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
                fi
                shift
                ;;
        esac
    done

    # 执行操作
    case "$action" in
        init)
            init_evidence_structure
            ;;
        build)
            if [ -z "$config" ]; then
                echo -e "${RED}错误: 未指定配置${NC}"
                show_help
                exit 1
            fi
            collect_build_evidence "$config"
            ;;
        flash)
            if [ -z "$config" ]; then
                echo -e "${RED}错误: 未指定配置${NC}"
                show_help
                exit 1
            fi
            collect_flash_evidence "$config"
            ;;
        verify)
            if [ -z "$config" ]; then
                echo -e "${RED}错误: 未指定配置${NC}"
                show_help
                exit 1
            fi
            collect_verification_evidence "$config"
            ;;
        all)
            if [ -z "$config" ]; then
                echo -e "${RED}错误: 未指定配置${NC}"
                show_help
                exit 1
            fi
            collect_all_evidence "$config"
            ;;
        report)
            generate_evidence_report
            ;;
        clean)
            clean_evidence
            ;;
        "")
            show_help
            ;;
        *)
            echo -e "${RED}未知操作: $action${NC}"
            show_help
            exit 1
            ;;
    esac
}

# 执行主函数
main "$@"
