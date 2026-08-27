#!/bin/bash
# openvela 仓库监控脚本
# 用于监控 NFS 服务器和仓库状态
# Usage: bash monitor.sh [options]
#
# Options:
#   --nfs-server <ip>    NFS 服务器 IP (默认: 自动检测)
#   --interval <sec>     监控间隔 (默认: 60)
#   --log-file <path>    日志文件路径
#   --alert-email <addr> 告警邮箱
#   --daemon             后台运行模式

set -e

# 颜色定义
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

info() { echo -e "${GREEN}[INFO]${NC} $1"; }
warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
error() { echo -e "${RED}[ERROR]${NC} $1"; }
debug() { echo -e "${BLUE}[DEBUG]${NC} $1"; }

# 默认配置
NFS_SERVER=""
INTERVAL=60
LOG_FILE=""
ALERT_EMAIL=""
DAEMON_MODE=false
MOUNT_POINT="/mnt/openvela-bare"
NFS_DIR="/nfs/openvela"

# 状态统计
TOTAL_CHECKS=0
SUCCESSFUL_CHECKS=0
FAILED_CHECKS=0
ALERTS_SENT=0

# 解析命令行参数
while [[ $# -gt 0 ]]; do
    case $1 in
        --nfs-server)
            NFS_SERVER="$2"
            shift 2
            ;;
        --interval)
            INTERVAL="$2"
            shift 2
            ;;
        --log-file)
            LOG_FILE="$2"
            shift 2
            ;;
        --alert-email)
            ALERT_EMAIL="$2"
            shift 2
            ;;
        --daemon)
            DAEMON_MODE=true
            shift
            ;;
        --help|-h)
            echo "Usage: bash monitor.sh [options]"
            echo ""
            echo "Options:"
            echo "  --nfs-server <ip>    NFS 服务器 IP"
            echo "  --interval <sec>     监控间隔 (默认: 60)"
            echo "  --log-file <path>    日志文件路径"
            echo "  --alert-email <addr> 告警邮箱"
            echo "  --daemon             后台运行模式"
            exit 0
            ;;
        *)
            error "未知参数: $1"
            ;;
    esac
done

# 自动检测 NFS 服务器
if [ -z "$NFS_SERVER" ]; then
    if [ -f /etc/fstab ]; then
        NFS_SERVER=$(grep "$MOUNT_POINT" /etc/fstab | awk '{print $1}' | cut -d: -f1)
    fi
    if [ -z "$NFS_SERVER" ]; then
        NFS_SERVER="localhost"
    fi
fi

# 设置默认日志文件
if [ -z "$LOG_FILE" ]; then
    LOG_FILE="/var/log/openvela-monitor.log"
fi

# 日志函数
log() {
    local timestamp=$(date '+%Y-%m-%d %H:%M:%S')
    local message="[$timestamp] $1"
    echo "$message" | tee -a "$LOG_FILE"
}

# 发送告警
send_alert() {
    local subject="$1"
    local body="$2"
    local level="$3"

    ALERTS_SENT=$((ALERTS_SENT + 1))

    # 记录告警
    log "ALERT [$level]: $subject"

    # 发送邮件（如果配置了）
    if [ -n "$ALERT_EMAIL" ]; then
        echo "$body" | mail -s "openvela Monitor Alert: $subject" "$ALERT_EMAIL" 2>/dev/null || true
    fi

    # 发送到系统日志
    case $level in
        CRITICAL)
            logger -p local0.crit "openvela-monitor: $subject"
            ;;
        WARNING)
            logger -p local0.warning "openvela-monitor: $subject"
            ;;
        INFO)
            logger -p local0.info "openvela-monitor: $subject"
            ;;
    esac
}

# 检查 NFS 连接
check_nfs_connection() {
    debug "检查 NFS 连接..."

    # 检查网络连接
    if ! ping -c 1 -W 5 "$NFS_SERVER" > /dev/null 2>&1; then
        send_alert "NFS 服务器不可达" "无法连接到 NFS 服务器: $NFS_SERVER" "CRITICAL"
        return 1
    fi

    # 检查 NFS 端口
    if ! nc -z "$NFS_SERVER" 2049 -w 5 > /dev/null 2>&1; then
        send_alert "NFS 端口不可达" "NFS 服务器端口 2049 不可达: $NFS_SERVER" "CRITICAL"
        return 1
    fi

    # 检查 NFS 挂载
    if ! mountpoint -q "$MOUNT_POINT" 2>/dev/null; then
        send_alert "NFS 未挂载" "NFS 共享未挂载到 $MOUNT_POINT" "WARNING"
        return 1
    fi

    debug "NFS 连接正常"
    return 0
}

# 检查 NFS 服务状态
check_nfs_service() {
    debug "检查 NFS 服务状态..."

    # 通过 SSH 检查 NFS 服务（如果有权限）
    if command -v ssh &> /dev/null; then
        local service_status=$(ssh -o ConnectTimeout=5 "$NFS_SERVER" "systemctl is-active nfs-kernel-server || systemctl is-active nfs-server" 2>/dev/null || echo "unknown")

        if [ "$service_status" != "active" ]; then
            send_alert "NFS 服务异常" "NFS 服务状态: $service_status" "CRITICAL"
            return 1
        fi
    fi

    debug "NFS 服务状态正常"
    return 0
}

# 检查仓库完整性
check_repo_integrity() {
    debug "检查仓库完整性..."

    local bare_repo="$MOUNT_POINT/bare-repo/openvela.git"

    if [ ! -d "$bare_repo" ]; then
        send_alert "裸仓库不存在" "裸仓库目录不存在: $bare_repo" "CRITICAL"
        return 1
    fi

    # 检查仓库是否可读
    if ! git --git-dir="$bare_repo" status > /dev/null 2>&1; then
        send_alert "仓库不可读" "无法读取裸仓库: $bare_repo" "CRITICAL"
        return 1
    fi

    # 检查仓库完整性（快速检查）
    if ! git --git-dir="$bare_repo" fsck --no-dangling --quick > /dev/null 2>&1; then
        send_alert "仓库完整性检查失败" "裸仓库完整性检查失败: $bare_repo" "WARNING"
        return 1
    fi

    debug "仓库完整性正常"
    return 0
}

# 检查磁盘空间
check_disk_space() {
    debug "检查磁盘空间..."

    # 检查 NFS 共享目录
    local usage=$(df -h "$MOUNT_POINT" 2>/dev/null | tail -1 | awk '{print $5}' | sed 's/%//')

    if [ -n "$usage" ] && [ "$usage" -gt 90 ]; then
        send_alert "磁盘空间不足" "NFS 共享目录使用率: ${usage}%" "WARNING"
        return 1
    fi

    # 检查本地磁盘
    local local_usage=$(df -h . | tail -1 | awk '{print $5}' | sed 's/%//')

    if [ "$local_usage" -gt 90 ]; then
        send_alert "本地磁盘空间不足" "本地磁盘使用率: ${local_usage}%" "WARNING"
        return 1
    fi

    debug "磁盘空间正常"
    return 0
}

# 检查 Git LFS 状态
check_git_lfs() {
    debug "检查 Git LFS 状态..."

    local bare_repo="$MOUNT_POINT/bare-repo/openvela.git"

    if [ ! -d "$bare_repo" ]; then
        return 1
    fi

    # 检查 LFS 对象
    local lfs_objects=$(git --git-dir="$bare_repo" lfs ls-files 2>/dev/null | wc -l)

    if [ "$lfs_objects" -eq 0 ]; then
        debug "未发现 LFS 对象"
    else
        debug "LFS 对象数量: $lfs_objects"
    fi

    return 0
}

# 检查网络延迟
check_network_latency() {
    debug "检查网络延迟..."

    local latency=$(ping -c 3 -W 5 "$NFS_SERVER" 2>/dev/null | tail -1 | awk -F'/' '{print $5}')

    if [ -n "$latency" ]; then
        local latency_ms=$(echo "$latency" | cut -d. -f1)

        if [ "$latency_ms" -gt 100 ]; then
            send_alert "网络延迟过高" "NFS 服务器延迟: ${latency}ms" "WARNING"
            return 1
        fi

        debug "网络延迟: ${latency}ms"
    fi

    return 0
}

# 检查仓库更新
check_repo_updates() {
    debug "检查仓库更新..."

    local bare_repo="$MOUNT_POINT/bare-repo/openvela.git"

    if [ ! -d "$bare_repo" ]; then
        return 1
    fi

    # 获取最新提交时间
    local latest_commit_time=$(git --git-dir="$bare_repo" log -1 --format=%ci 2>/dev/null)

    if [ -n "$latest_commit_time" ]; then
        local commit_date=$(date -d "$latest_commit_time" +%s 2>/dev/null || date -j -f "%Y-%m-%d %H:%M:%S %z" "$latest_commit_time" +%s 2>/dev/null)
        local now=$(date +%s)
        local diff_hours=$(( (now - commit_date) / 3600 ))

        if [ "$diff_hours" -gt 24 ]; then
            warn "仓库超过 24 小时未更新"
        fi

        debug "最新提交时间: $latest_commit_time"
    fi

    return 0
}

# 生成监控报告
generate_report() {
    local timestamp=$(date '+%Y-%m-%d %H:%M:%S')
    local success_rate=0

    if [ "$TOTAL_CHECKS" -gt 0 ]; then
        success_rate=$((SUCCESSFUL_CHECKS * 100 / TOTAL_CHECKS))
    fi

    cat << EOF

==========================================
  openvela 监控报告
==========================================

时间: $timestamp
NFS 服务器: $NFS_SERVER
挂载点: $MOUNT_POINT

检查统计:
  总检查次数: $TOTAL_CHECKS
  成功次数: $SUCCESSFUL_CHECKS
  失败次数: $FAILED_CHECKS
  成功率: ${success_rate}%
  告警次数: $ALERTS_SENT

仓库状态:
  裸仓库: $MOUNT_POINT/bare-repo/openvela.git
  最新提交: $(git --git-dir="$MOUNT_POINT/bare-repo/openvela.git" log -1 --oneline 2>/dev/null || echo "未知")
  分支数量: $(git --git-dir="$MOUNT_POINT/bare-repo/openvela.git" branch 2>/dev/null | wc -l || echo "未知")

磁盘使用:
  NFS 共享: $(df -h "$MOUNT_POINT" 2>/dev/null | tail -1 | awk '{print $5}' || echo "未知")
  本地磁盘: $(df -h . | tail -1 | awk '{print $5}')

==========================================
EOF
}

# 执行所有检查
run_all_checks() {
    local status=0

    TOTAL_CHECKS=$((TOTAL_CHECKS + 1))

    log "开始监控检查..."

    check_nfs_connection || status=1
    check_nfs_service || status=1
    check_repo_integrity || status=1
    check_disk_space || status=1
    check_git_lfs || status=1
    check_network_latency || status=1
    check_repo_updates || status=1

    if [ $status -eq 0 ]; then
        SUCCESSFUL_CHECKS=$((SUCCESSFUL_CHECKS + 1))
        log "监控检查完成: 正常"
    else
        FAILED_CHECKS=$((FAILED_CHECKS + 1))
        log "监控检查完成: 异常"
    fi

    return $status
}

# 主循环
main_loop() {
    log "=== openvela 监控启动 ==="
    log "NFS 服务器: $NFS_SERVER"
    log "监控间隔: ${INTERVAL}秒"
    log "日志文件: $LOG_FILE"

    while true; do
        run_all_checks

        # 每 10 次检查生成一次报告
        if [ $((TOTAL_CHECKS % 10)) -eq 0 ]; then
            generate_report
        fi

        sleep "$INTERVAL"
    done
}

# 后台运行模式
run_daemon() {
    log "以守护进程模式启动..."

    # 创建 PID 文件
    local pid_file="/var/run/openvela-monitor.pid"

    # 检查是否已在运行
    if [ -f "$pid_file" ]; then
        local old_pid=$(cat "$pid_file")
        if kill -0 "$old_pid" 2>/dev/null; then
            error "监控已在运行 (PID: $old_pid)"
        fi
    fi

    # 后台运行
    nohup "$0" --nfs-server "$NFS_SERVER" --interval "$INTERVAL" --log-file "$LOG_FILE" > /dev/null 2>&1 &
    local pid=$!

    echo "$pid" > "$pid_file"

    log "监控已启动 (PID: $pid)"
    log "停止命令: kill $pid"
}

# 显示帮助
show_help() {
    echo ""
    echo "=========================================="
    echo "  openvela 监控脚本"
    echo "=========================================="
    echo ""
    echo "用法:"
    echo "  bash monitor.sh [options]"
    echo ""
    echo "选项:"
    echo "  --nfs-server <ip>    NFS 服务器 IP"
    echo "  --interval <sec>     监控间隔 (默认: 60)"
    echo "  --log-file <path>    日志文件路径"
    echo "  --alert-email <addr> 告警邮箱"
    echo "  --daemon             后台运行模式"
    echo ""
    echo "示例:"
    echo "  # 前台运行"
    echo "  bash monitor.sh --nfs-server 192.168.1.100"
    echo ""
    echo "  # 后台运行"
    echo "  bash monitor.sh --nfs-server 192.168.1.100 --daemon"
    echo ""
    echo "  # 带告警"
    echo "  bash monitor.sh --nfs-server 192.168.1.100 --alert-email admin@example.com"
    echo ""
    echo "=========================================="
}

# 主流程
main() {
    # 检查是否是帮助请求
    if [ "$1" = "--help" ] || [ "$1" = "-h" ]; then
        show_help
        exit 0
    fi

    # 检查是否是后台模式
    if [ "$DAEMON_MODE" = true ]; then
        run_daemon
        exit 0
    fi

    # 检查是否是报告模式
    if [ "$1" = "--report" ]; then
        run_all_checks
        generate_report
        exit 0
    fi

    # 正常监控模式
    main_loop
}

# 执行主流程
main "$@"
