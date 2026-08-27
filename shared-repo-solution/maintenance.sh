#!/bin/bash
# openvela 仓库维护脚本
# 用于维护 NFS 共享仓库的健康状态
# Usage: bash maintenance.sh [options]
#
# Options:
#   --nfs-dir <path>       NFS 共享目录 (默认: /nfs/openvela)
#   --action <action>      执行的操作 (gc|backup|cleanup|integrity|update|all)
#   --backup-dir <path>    备份目录
#   --retention <days>     备份保留天数 (默认: 7)
#   --auto                 自动模式，无需交互

set -e

# 颜色定义
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

info() { echo -e "${GREEN}[INFO]${NC} $1"; }
warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
error() { echo -e "${RED}[ERROR]${NC} $1"; exit 1; }
debug() { echo -e "${BLUE}[DEBUG]${NC} $1"; }

# 默认配置
NFS_DIR="/nfs/openvela"
ACTION=""
BACKUP_DIR=""
RETENTION_DAYS=7
AUTO_MODE=false
BARE_REPO_DIR="$NFS_DIR/bare-repo/openvela.git"
LOG_DIR="$NFS_DIR/logs"

# 解析命令行参数
while [[ $# -gt 0 ]]; do
    case $1 in
        --nfs-dir)
            NFS_DIR="$2"
            BARE_REPO_DIR="$NFS_DIR/bare-repo/openvela.git"
            LOG_DIR="$NFS_DIR/logs"
            shift 2
            ;;
        --action)
            ACTION="$2"
            shift 2
            ;;
        --backup-dir)
            BACKUP_DIR="$2"
            shift 2
            ;;
        --retention)
            RETENTION_DAYS="$2"
            shift 2
            ;;
        --auto)
            AUTO_MODE=true
            shift
            ;;
        --help|-h)
            echo "Usage: bash maintenance.sh [options]"
            echo ""
            echo "Options:"
            echo "  --nfs-dir <path>       NFS 共享目录 (默认: /nfs/openvela)"
            echo "  --action <action>      执行的操作"
            echo "  --backup-dir <path>    备份目录"
            echo "  --retention <days>     备份保留天数 (默认: 7)"
            echo "  --auto                 自动模式"
            echo ""
            echo "操作:"
            echo "  gc          - 垃圾回收"
            echo "  backup      - 创建备份"
            echo "  cleanup     - 清理旧文件"
            echo "  integrity   - 检查完整性"
            echo "  update      - 更新仓库"
            echo "  all         - 执行所有维护任务"
            exit 0
            ;;
        *)
            error "未知参数: $1"
            ;;
    esac
done

# 设置默认备份目录
if [ -z "$BACKUP_DIR" ]; then
    BACKUP_DIR="$NFS_DIR/backup"
fi

# 日志函数
log() {
    local timestamp=$(date '+%Y-%m-%d %H:%M:%S')
    local message="[$timestamp] $1"
    echo "$message" | tee -a "$LOG_DIR/maintenance.log"
}

# 检查前置条件
check_prerequisites() {
    info "检查前置条件..."

    # 检查是否以 root 权限运行
    if [ "$EUID" -ne 0 ]; then
        error "请使用 sudo 运行此脚本"
    fi

    # 检查 NFS 目录
    if [ ! -d "$NFS_DIR" ]; then
        error "NFS 目录不存在: $NFS_DIR"
    fi

    # 检查裸仓库
    if [ ! -d "$BARE_REPO_DIR" ]; then
        error "裸仓库不存在: $BARE_REPO_DIR"
    fi

    # 创建日志目录
    mkdir -p "$LOG_DIR"

    # 创建备份目录
    mkdir -p "$BACKUP_DIR"

    info "前置条件检查完成"
}

# 垃圾回收
garbage_collect() {
    log "=== 开始垃圾回收 ==="

    local start_time=$(date +%s)

    cd "$BARE_REPO_DIR"

    # 记录回收前的大小
    local size_before=$(du -sh . | awk '{print $1}')
    log "回收前大小: $size_before"

    # 执行垃圾回收
    log "执行 git gc --aggressive --prune=now..."
    git gc --aggressive --prune=now 2>&1 | tee -a "$LOG_DIR/maintenance.log"

    # 清理 reflog
    log "清理 reflog..."
    git reflog expire --expire=now --all 2>&1 | tee -a "$LOG_DIR/maintenance.log"

    # 清理不必要的文件
    log "清理不必要的文件..."
    git prune --expire=now 2>&1 | tee -a "$LOG_DIR/maintenance.log"

    # 记录回收后的大小
    local size_after=$(du -sh . | awk '{print $1}')
    log "回收后大小: $size_after"

    local end_time=$(date +%s)
    local duration=$((end_time - start_time))

    log "垃圾回收完成，耗时: ${duration}秒"
    log "=== 垃圾回收完成 ==="
}

# 创建备份
create_backup() {
    log "=== 开始创建备份 ==="

    local timestamp=$(date +%Y%m%d%H%M%S)
    local backup_file="$BACKUP_DIR/openvela-bare-$timestamp.tar.gz"
    local start_time=$(date +%s)

    # 创建备份
    log "创建备份: $backup_file"
    tar -czf "$backup_file" -C "$NFS_DIR/bare-repo" openvela.git 2>&1 | tee -a "$LOG_DIR/maintenance.log"

    # 计算备份大小
    local backup_size=$(du -sh "$backup_file" | awk '{print $1}')
    log "备份大小: $backup_size"

    # 清理旧备份
    log "清理 ${RETENTION_DAYS} 天前的备份..."
    find "$BACKUP_DIR" -name "openvela-bare-*.tar.gz" -mtime +$RETENTION_DAYS -delete 2>&1 | tee -a "$LOG_DIR/maintenance.log"

    local end_time=$(date +%s)
    local duration=$((end_time - start_time))

    log "备份创建完成，耗时: ${duration}秒"
    log "=== 备份创建完成 ==="
}

# 清理旧文件
cleanup_old_files() {
    log "=== 开始清理旧文件 ==="

    local start_time=$(date +%s)

    # 清理旧日志
    log "清理 30 天前的日志..."
    find "$LOG_DIR" -name "*.log" -mtime +30 -delete 2>&1 | tee -a "$LOG_DIR/maintenance.log"

    # 清理旧备份
    log "清理 ${RETENTION_DAYS} 天前的备份..."
    find "$BACKUP_DIR" -name "openvela-bare-*.tar.gz" -mtime +$RETENTION_DAYS -delete 2>&1 | tee -a "$LOG_DIR/maintenance.log"

    # 清理临时文件
    log "清理临时文件..."
    find "$NFS_DIR" -name "*.tmp" -mtime +7 -delete 2>&1 | tee -a "$LOG_DIR/maintenance.log"
    find "$NFS_DIR" -name "*.bak" -mtime +7 -delete 2>&1 | tee -a "$LOG_DIR/maintenance.log"

    local end_time=$(date +%s)
    local duration=$((end_time - start_time))

    log "清理完成，耗时: ${duration}秒"
    log "=== 清理完成 ==="
}

# 检查完整性
check_integrity() {
    log "=== 开始检查完整性 ==="

    local start_time=$(date +%s)
    local status=0

    cd "$BARE_REPO_DIR"

    # 快速完整性检查
    log "执行快速完整性检查..."
    if git fsck --no-dangling --quick 2>&1 | tee -a "$LOG_DIR/maintenance.log"; then
        log "快速完整性检查: 通过"
    else
        log "快速完整性检查: 失败"
        status=1
    fi

    # 完整性检查（如果快速检查失败）
    if [ $status -ne 0 ]; then
        log "执行完整完整性检查..."
        if git fsck --full --no-dangling 2>&1 | tee -a "$LOG_DIR/maintenance.log"; then
            log "完整完整性检查: 通过"
            status=0
        else
            log "完整完整性检查: 失败"
        fi
    fi

    # 检查引用
    log "检查引用..."
    local ref_count=$(git show-ref | wc -l)
    log "引用数量: $ref_count"

    # �查对象
    log "检查对象..."
    local object_info=$(git count-objects -v)
    log "对象信息: $object_info"

    local end_time=$(date +%s)
    local duration=$((end_time - start_time))

    if [ $status -eq 0 ]; then
        log "完整性检查完成: 正常，耗时: ${duration}秒"
    else
        log "完整性检查完成: 异常，耗时: ${duration}秒"
    fi

    log "=== 完整性检查完成 ==="
    return $status
}

# 更新仓库
update_repo() {
    log "=== 开始更新仓库 ==="

    local start_time=$(date +%s)

    cd "$BARE_REPO_DIR"

    # 获取当前状态
    local current_commit=$(git log --oneline -1)
    log "当前提交: $current_commit"

    # 获取远程更新
    log "获取远程更新..."
    git fetch origin --prune 2>&1 | tee -a "$LOG_DIR/maintenance.log"

    # 获取标签
    log "获取标签..."
    git fetch origin --tags 2>&1 | tee -a "$LOG_DIR/maintenance.log"

    # 获取更新后的状态
    local new_commit=$(git log --oneline -1)
    log "更新后提交: $new_commit"

    if [ "$current_commit" != "$new_commit" ]; then
        log "仓库已更新"
    else
        log "仓库已是最新"
    fi

    local end_time=$(date +%s)
    local duration=$((end_time - start_time))

    log "仓库更新完成，耗时: ${duration}秒"
    log "=== 仓库更新完成 ==="
}

# 显示仓库状态
show_repo_status() {
    log "=== 仓库状态 ==="

    cd "$BARE_REPO_DIR"

    # 基本信息
    log "裸仓库路径: $BARE_REPO_DIR"
    log "仓库大小: $(du -sh . | awk '{print $1}')"

    # 最新提交
    log "最新提交: $(git log --oneline -1)"

    # 分支信息
    local branch_count=$(git branch | wc -l)
    log "分支数量: $branch_count"

    # 标签信息
    local tag_count=$(git tag | wc -l)
    log "标签数量: $tag_count"

    # 对象信息
    local object_info=$(git count-objects -v)
    log "对象信息: $object_info"

    # 磁盘使用
    log "磁盘使用: $(df -h . | tail -1 | awk '{print $5}')"

    log "=== 仓库状态完成 ==="
}

# 执行所有维护任务
run_all_maintenance() {
    log "=== 开始完整维护 ==="

    local start_time=$(date +%s)

    update_repo
    garbage_collect
    create_backup
    cleanup_old_files
    check_integrity
    show_repo_status

    local end_time=$(date +%s)
    local duration=$((end_time - start_time))

    log "=== 完整维护完成，总耗时: ${duration}秒 ==="
}

# 显示菜单
show_menu() {
    echo ""
    echo "=========================================="
    echo "  openvela 仓库维护工具"
    echo "=========================================="
    echo ""
    echo "NFS 目录: $NFS_DIR"
    echo "裸仓库: $BARE_REPO_DIR"
    echo ""
    echo "1. 垃圾回收"
    echo "2. 创建备份"
    echo "3. 清理旧文件"
    echo "4. 检查完整性"
    echo "5. 更新仓库"
    echo "6. 显示仓库状态"
    echo "7. 执行所有维护任务"
    echo "0. 退出"
    echo ""
    echo "=========================================="
    echo ""
}

# 交互式菜单
interactive_menu() {
    while true; do
        show_menu
        read -p "请选择操作 [0-7]: " choice

        case $choice in
            1)
                garbage_collect
                ;;
            2)
                create_backup
                ;;
            3)
                cleanup_old_files
                ;;
            4)
                check_integrity
                ;;
            5)
                update_repo
                ;;
            6)
                show_repo_status
                ;;
            7)
                run_all_maintenance
                ;;
            0)
                log "退出维护工具"
                exit 0
                ;;
            *)
                echo "无效选择，请重新输入"
                ;;
        esac

        echo ""
        read -p "按 Enter 继续..."
    done
}

# 主流程
main() {
    log "=== openvela 仓库维护 ==="

    check_prerequisites

    # 如果指定了操作，直接执行
    if [ -n "$ACTION" ]; then
        case $ACTION in
            gc)
                garbage_collect
                ;;
            backup)
                create_backup
                ;;
            cleanup)
                cleanup_old_files
                ;;
            integrity)
                check_integrity
                ;;
            update)
                update_repo
                ;;
            all)
                run_all_maintenance
                ;;
            status)
                show_repo_status
                ;;
            *)
                error "未知操作: $ACTION"
                ;;
        esac
        return
    fi

    # 自动模式
    if [ "$AUTO_MODE" = true ]; then
        run_all_maintenance
        return
    fi

    # 交互式菜单
    interactive_menu
}

# 执行主流程
main
