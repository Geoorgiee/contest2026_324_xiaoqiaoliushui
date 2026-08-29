#!/usr/bin/env bash
############################################################################
# vendor_esp32p4/verify_build_system.sh
#
# 快速验证 ESP32-P4 构建系统是否正确配置
#
# Usage: bash verify_build_system.sh
#
############################################################################

set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

pass() { echo -e "${GREEN}[PASS]${NC} $1"; }
fail() { echo -e "${RED}[FAIL]${NC} $1"; }
warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }

ERRORS=0
WARNINGS=0

echo "=========================================="
echo "  ESP32-P4 构建系统验证"
echo "=========================================="
echo ""

# 1. 检查工具链
echo "【1/8】RISC-V 工具链..."
TOOLCHAIN="/home/geo/openvela-build/prebuilts/gcc/linux-x86_64/riscv-none-elf/bin"
if [ -x "${TOOLCHAIN}/riscv-none-elf-gcc" ]; then
    GCC_VER=$("${TOOLCHAIN}/riscv-none-elf-gcc" --version | head -1)
    pass "GCC: ${GCC_VER}"
else
    fail "riscv-none-elf-gcc 未找到"
    ERRORS=$((ERRORS + 1))
fi

if [ -x "${TOOLCHAIN}/riscv-none-elf-ld" ]; then
    pass "LD: 已安装"
else
    fail "riscv-none-elf-ld 未找到"
    ERRORS=$((ERRORS + 1))
fi

if [ -x "${TOOLCHAIN}/riscv-none-elf-size" ]; then
    pass "SIZE: 已安装"
else
    warn "riscv-none-elf-size 未找到 (可选)"
    WARNINGS=$((WARNINGS + 1))
fi

# 2. 检查 kconfig
echo ""
echo "【2/8】Kconfig 工具..."
KCONFIG="/opt/openvela/prebuilts/build-tools/linux-x86_64/bin"
if [ -x "${KCONFIG}/kconfig-conf" ]; then
    pass "kconfig-conf: 已安装"
else
    fail "kconfig-conf 未找到"
    ERRORS=$((ERRORS + 1))
fi

if [ -x "${KCONFIG}/kconfig-mconf" ]; then
    pass "kconfig-mconf: 已安装 (menuconfig 支持)"
else
    warn "kconfig-mconf 未找到 (menuconfig 不可用)"
    WARNINGS=$((WARNINGS + 1))
fi

# 3. 检查 NuttX 目录
echo ""
echo "【3/8】NuttX 源码..."
NUTTX="/home/geo/openvela-build/nuttx"
if [ -d "${NUTTX}" ]; then
    pass "NuttX 目录: ${NUTTX}"
else
    fail "NuttX 目录不存在"
    ERRORS=$((ERRORS + 1))
fi

if [ -x "${NUTTX}/tools/configure.sh" ]; then
    pass "configure.sh: 已就绪"
else
    fail "configure.sh 未找到"
    ERRORS=$((ERRORS + 1))
fi

if [ -f "${NUTTX}/Makefile" ]; then
    pass "Makefile: 存在"
else
    fail "Makefile 不存在"
    ERRORS=$((ERRORS + 1))
fi

# 4. 检查 vendor 目录
echo ""
echo "【4/8】Vendor 目录..."
VENDOR="/home/geo/openvela-build/vendor/espressif"
if [ -d "${VENDOR}" ]; then
    pass "Vendor 目录: ${VENDOR}"
else
    fail "Vendor 目录不存在"
    ERRORS=$((ERRORS + 1))
fi

if [ -d "${VENDOR}/boards/esp32p4" ]; then
    pass "ESP32-P4 板级目录: 存在"
else
    fail "ESP32-P4 板级目录不存在"
    ERRORS=$((ERRORS + 1))
fi

if [ -d "${VENDOR}/chips/esp32p4" ]; then
    pass "ESP32-P4 芯片目录: 存在"
else
    fail "ESP32-P4 芯片目录不存在"
    ERRORS=$((ERRORS + 1))
fi

# 5. 检查配置目录
echo ""
echo "【5/8】配置目录..."
CONFIGS="/home/geo/openvela/vendor_esp32p4/boards/risc-v/esp32p4/esp32p4-evb/configs"
if [ -d "${CONFIGS}" ]; then
    pass "配置目录: ${CONFIGS}"
    CONFIG_COUNT=$(ls -d ${CONFIGS}/*/ 2>/dev/null | wc -l)
    pass "配置数量: ${CONFIG_COUNT}"
else
    fail "配置目录不存在"
    ERRORS=$((ERRORS + 1))
fi

# 6. 检查 defconfig 文件
echo ""
echo "【6/8】Defconfig 文件..."
for config in nsh default ble display storage tinyml; do
    DEFCONFIG="${CONFIGS}/${config}/defconfig"
    if [ -f "${DEFCONFIG}" ]; then
        pass "${config}: 存在"
    else
        fail "${config}: 缺失"
        ERRORS=$((ERRORS + 1))
    fi
done

# 7. 检查链接脚本
echo ""
echo "【7/8】链接脚本..."
LD_SCRIPT="/home/geo/openvela/vendor_esp32p4/boards/risc-v/esp32p4/esp32p4-evb/scripts/ld.script"
if [ -f "${LD_SCRIPT}" ]; then
    pass "ld.script: 存在"
else
    fail "ld.script 不存在"
    ERRORS=$((ERRORS + 1))
fi

MAKE_DEFS="/home/geo/openvela/vendor_esp32p4/boards/risc-v/esp32p4/esp32p4-evb/scripts/Make.defs"
if [ -f "${MAKE_DEFS}" ]; then
    pass "Make.defs: 存在"
else
    fail "Make.defs 不存在"
    ERRORS=$((ERRORS + 1))
fi

TOOLCHAIN_DEFS="/home/geo/openvela/vendor_esp32p4/boards/risc-v/esp32p4/esp32p4-evb/scripts/Toolchain.defs"
if [ -f "${TOOLCHAIN_DEFS}" ]; then
    pass "Toolchain.defs: 存在"
else
    fail "Toolchain.defs 不存在"
    ERRORS=$((ERRORS + 1))
fi

# 8. 检查系统资源
echo ""
echo "【8/8】系统资源..."
MEM_MB=$(free -m | awk 'NR==2{print $2}')
MEM_GB=$(( (MEM_MB + 512) / 1024 ))
if [ "$MEM_MB" -ge 15360 ]; then
    pass "内存: ${MEM_GB}GB"
elif [ "$MEM_MB" -ge 7680 ]; then
    warn "内存: ${MEM_GB}GB (推荐 16GB)"
    WARNINGS=$((WARNINGS + 1))
else
    fail "内存不足: ${MEM_GB}GB"
    ERRORS=$((ERRORS + 1))
fi

DISK_AVAIL=$(df -BG /home/geo/openvela | awk 'NR==2{print $4}' | tr -d 'G')
if [ "$DISK_AVAIL" -ge 20 ]; then
    pass "磁盘空间: ${DISK_AVAIL}GB 可用"
elif [ "$DISK_AVAIL" -ge 10 ]; then
    warn "磁盘空间: ${DISK_AVAIL}GB (建议 20GB+)"
    WARNINGS=$((WARNINGS + 1))
else
    fail "磁盘空间不足: ${DISK_AVAIL}GB"
    ERRORS=$((ERRORS + 1))
fi

# 总结
echo ""
echo "=========================================="
echo "  验证结果"
echo "=========================================="
echo ""

if [ $ERRORS -eq 0 ]; then
    echo -e "${GREEN}构建系统验证通过!${NC}"
    echo ""
    echo "可以开始编译:"
    echo "  bash build_test.sh --list          # 列出配置"
    echo "  bash build_test.sh --config nsh    # 编译 nsh"
    echo "  bash build_test.sh --dry-run       # 验证所有配置"
    exit 0
else
    echo -e "${RED}发现 ${ERRORS} 个错误${NC}"
    if [ $WARNINGS -gt 0 ]; then
        echo -e "${YELLOW}发现 ${WARNINGS} 个警告${NC}"
    fi
    echo ""
    echo "请修复上述错误后重试"
    exit 1
fi
