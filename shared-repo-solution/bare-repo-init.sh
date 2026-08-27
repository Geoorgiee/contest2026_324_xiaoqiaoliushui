#!/bin/bash
# 裸仓库初始化脚本
# 用于创建和初始化 openvela 的 git 裸仓库（作为 --reference 对象库）
# Usage: sudo bash bare-repo-init.sh [options]
#
# Options:
#   --nfs-dir <path>       NFS 共享目录 (默认: /nfs/openvela)
#   --source <source>      代码源 (gitee|github) (默认: gitee)
#   --branch <branch>      分支 (dev|trunk) (默认: dev)
#   --protocol <proto>     协议 (ssh|https) (默认: ssh)
#   --lfs                  启用 Git LFS

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
SOURCE="gitee"
BRANCH="dev"
PROTOCOL="ssh"
ENABLE_LFS=false
BARE_REPO_DIR="$NFS_DIR/bare-repo"
LFS_STORAGE_DIR="$NFS_DIR/lfs-storage"
LOG_DIR="$NFS_DIR/logs"

# 解析命令行参数
while [[ $# -gt 0 ]]; do
    case $1 in
        --nfs-dir)
            NFS_DIR="$2"
            BARE_REPO_DIR="$NFS_DIR/bare-repo"
            LFS_STORAGE_DIR="$NFS_DIR/lfs-storage"
            LOG_DIR="$NFS_DIR/logs"
            shift 2
            ;;
        --source)
            SOURCE="$2"
            shift 2
            ;;
        --branch)
            BRANCH="$2"
            shift 2
            ;;
        --protocol)
            PROTOCOL="$2"
            shift 2
            ;;
        --lfs)
            ENABLE_LFS=true
            shift
            ;;
        --help|-h)
            echo "Usage: sudo bash bare-repo-init.sh [options]"
            echo ""
            echo "Options:"
            echo "  --nfs-dir <path>       NFS 共享目录 (默认: /nfs/openvela)"
            echo "  --source <source>      代码源 (gitee|github) (默认: gitee)"
            echo "  --branch <branch>      分支 (dev|trunk) (默认: dev)"
            echo "  --protocol <proto>     协议 (ssh|https) (默认: ssh)"
            echo "  --lfs                  启用 Git LFS"
            exit 0
            ;;
        *)
            error "未知参数: $1"
            ;;
    esac
done

# 检查是否以 root 权限运行
if [ "$EUID" -ne 0 ]; then
    error "请使用 sudo 运行此脚本"
fi

info "=== openvela 裸仓库初始化 ==="
info "NFS 目录: $NFS_DIR"
info "代码源: $SOURCE"
info "分支: $BRANCH"
info "协议: $PROTOCOL"
info "Git LFS: $ENABLE_LFS"

# 创建目录结构
create_directories() {
    info "创建目录结构..."

    mkdir -p "$BARE_REPO_DIR"
    mkdir -p "$LFS_STORAGE_DIR"
    mkdir -p "$LOG_DIR"
    mkdir -p "$NFS_DIR/backup"

    # 设置权限
    chmod -R 777 "$NFS_DIR"

    info "目录结构创建完成"
}

# 安装依赖
install_dependencies() {
    info "安装依赖..."

    # 检测操作系统
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        OS=$ID
    else
        error "无法检测操作系统"
    fi

    case $OS in
        ubuntu|debian)
            apt-get update
            apt-get install -y git git-lfs curl wget
            ;;
        centos|rhel|rocky|alma)
            yum install -y git git-lfs curl wget
            ;;
        fedora)
            dnf install -y git git-lfs curl wget
            ;;
    esac

    # 安装 repo 工具
    if ! command -v repo &> /dev/null; then
        info "安装 repo 工具..."
        curl -sSL "https://storage.googleapis.com/git-repo-downloads/repo" > /usr/local/bin/repo
        chmod +x /usr/local/bin/repo
    fi

    info "依赖安装完成"
}

# 初始化 Git LFS
init_git_lfs() {
    if [ "$ENABLE_LFS" = true ]; then
        info "初始化 Git LFS..."

        # 安装 Git LFS
        if ! command -v git-lfs &> /dev/null; then
            case $OS in
                ubuntu|debian)
                    curl -s https://packagecloud.io/install/repositories/github/git-lfs/script.deb.sh | bash
                    apt-get install -y git-lfs
                    ;;
                centos|rhel|rocky|alma|fedora)
                    curl -s https://packagecloud.io/install/repositories/github/git-lfs/script.rpm.sh | bash
                    yum install -y git-lfs
                    ;;
            esac
        fi

        git lfs install

        info "Git LFS 初始化完成"
    fi
}

# 获取仓库 URL
get_repo_url() {
    local url=""

    case "$SOURCE-$PROTOCOL" in
        gitee-ssh)
            url="ssh://git@gitee.com/open-vela/manifests.git"
            ;;
        gitee-https)
            url="https://gitee.com/open-vela/manifests.git"
            ;;
        github-ssh)
            url="ssh://git@github.com/open-vela/manifests.git"
            ;;
        github-https)
            url="https://github.com/open-vela/manifests.git"
            ;;
        *)
            error "不支持的源和协议组合: $SOURCE-$PROTOCOL"
            ;;
    esac

    echo "$url"
}

# 克隆裸仓库
clone_bare_repo() {
    info "克隆裸仓库..."

    local repo_url=$(get_repo_url)
    local timestamp=$(date +%Y%m%d%H%M%S)
    local log_file="$LOG_DIR/clone-$timestamp.log"

    # 备份现有仓库
    if [ -d "$BARE_REPO_DIR/openvela.git" ]; then
        warn "检测到现有裸仓库，备份中..."
        mv "$BARE_REPO_DIR/openvela.git" "$BARE_REPO_DIR/openvela.git.backup.$timestamp"
    fi

    # 克隆裸仓库
    info "从 $repo_url 克隆..."
    info "日志文件: $log_file"

    git clone --bare "$repo_url" "$BARE_REPO_DIR/openvela.git" 2>&1 | tee "$log_file"

    if [ $? -eq 0 ]; then
        info "裸仓库克隆完成"
    else
        error "裸仓库克隆失败，请检查日志: $log_file"
    fi
}

# 配置裸仓库
configure_bare_repo() {
    info "配置裸仓库..."

    cd "$BARE_REPO_DIR/openvela.git"

    # 设置仓库配置
    git config core.bare true
    git config core.logAllRefUpdates true
    git config remote.origin.fetch "+refs/heads/*:refs/heads/*"
    git config remote.origin.fetch "+refs/tags/*:refs/tags/*"

    # 如果启用了 LFS，配置 LFS 存储
    if [ "$ENABLE_LFS" = true ]; then
        git config lfs.storage "$LFS_STORAGE_DIR"
        git config lfs.fetchinclude "*"
        git config lfs.fetchexclude ""
    fi

    # 创建符号链接，方便访问
    ln -sf "$BARE_REPO_DIR/openvela.git" "$BARE_REPO_DIR/openvela-bare.git"

    info "裸仓库配置完成"
}

# 同步所有分支
sync_all_branches() {
    info "同步所有分支..."

    cd "$BARE_REPO_DIR/openvela.git"

    # 获取所有远程分支
    git fetch origin

    # 获取所有标签
    git fetch origin --tags

    info "分支同步完成"
}

# 创建 repo 清单配置
create_repo_manifest() {
    info "创建 repo 清单配置..."

    local manifest_dir="$NFS_DIR/manifests"
    mkdir -p "$manifest_dir"

    cat > "$manifest_dir/openvela.xml" << 'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<manifest>
  <remote name="origin" fetch=".." review="https://gitee.com/open-vela" />

  <default revision="dev" remote="origin" sync-j="4" sync-tags="true" />

  <project path="nuttx" name="nuttx" groups="default" />
  <project path="apps" name="apps" groups="default" />
  <project path="vendor/openvela" name="vendor-openvela" groups="default" />
  <project path="frameworks" name="frameworks" groups="default" />
  <project path="external" name="external" groups="default" />
  <project path="prebuilts" name="prebuilts" groups="default" />
</manifest>
EOF

    info "repo 清单配置创建完成"
}

# 创建同步脚本
create_sync_script() {
    info "创建同步脚本..."

    cat > "$NFS_DIR/sync-repo.sh" << 'SYNCSCRIPT'
#!/bin/bash
# openvela 仓库同步脚本
# 用于从裸仓库同步代码到开发机

set -e

NFS_DIR="/nfs/openvela"
BARE_REPO_DIR="$NFS_DIR/bare-repo/openvela.git"
WORK_DIR="${1:-$(pwd)}"
BRANCH="${2:-dev}"

echo "=== openvela 仓库同步 ==="
echo "工作目录: $WORK_DIR"
echo "分支: $BRANCH"
echo "裸仓库: $BARE_REPO_DIR"

# 检查裸仓库是否存在
if [ ! -d "$BARE_REPO_DIR" ]; then
    echo "错误: 裸仓库不存在: $BARE_REPO_DIR"
    echo "请先运行 bare-repo-init.sh 初始化裸仓库"
    exit 1
fi

# 创建工作目录
mkdir -p "$WORK_DIR"
cd "$WORK_DIR"

# 初始化 repo
if [ ! -d ".repo" ]; then
    echo "初始化 repo..."
    repo init -u "$BARE_REPO_DIR" \
        -b "$BRANCH" \
        -m openvela.xml \
        --reference="$BARE_REPO_DIR" \
        --git-lfs
fi

# 同步代码
echo "同步代码..."
repo sync -c -j8

echo "=== 同步完成 ==="
SYNCSCRIPT

    chmod +x "$NFS_DIR/sync-repo.sh"

    info "同步脚本创建完成"
}

# 创建监控脚本
create_monitor_script() {
    info "创建监控脚本..."

    cat > "$NFS_DIR/monitor.sh" << 'MONITORSCRIPT'
#!/bin/bash
# openvela 仓库监控脚本

NFS_DIR="/nfs/openvela"
BARE_REPO_DIR="$NFS_DIR/bare-repo/openvela.git"
LOG_FILE="$NFS_DIR/logs/monitor.log"

log() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $1" | tee -a "$LOG_FILE"
}

check_repo_status() {
    log "检查仓库状态..."

    if [ ! -d "$BARE_REPO_DIR" ]; then
        log "错误: 裸仓库不存在"
        return 1
    fi

    cd "$BARE_REPO_DIR"

    # 检查仓库完整性
    if ! git fsck --no-dangling > /dev/null 2>&1; then
        log "警告: 仓库完整性检查失败"
        return 1
    fi

    # 获取最新提交
    local latest_commit=$(git log --oneline -1)
    log "最新提交: $latest_commit"

    # 获取分支列表
    local branches=$(git branch | wc -l)
    log "分支数量: $branches"

    # 获取对象数量
    local objects=$(git count-objects -v | grep count | awk '{print $2}')
    log "对象数量: $objects"

    return 0
}

check_nfs_status() {
    log "检查 NFS 状态..."

    # 检查 NFS 服务状态
    if systemctl is-active --quiet nfs-kernel-server; then
        log "NFS 服务状态: 运行中"
    elif systemctl is-active --quiet nfs-server; then
        log "NFS 服务状态: 运行中"
    else
        log "错误: NFS 服务未运行"
        return 1
    fi

    # 检查 NFS 共享
    if showmount -e localhost > /dev/null 2>&1; then
        log "NFS 共享状态: 正常"
    else
        log "警告: NFS 共享检查失败"
    fi

    return 0
}

check_disk_usage() {
    log "检查磁盘使用..."

    local usage=$(df -h "$NFS_DIR" | tail -1 | awk '{print $5}' | sed 's/%//')
    log "磁盘使用率: ${usage}%"

    if [ "$usage" -gt 90 ]; then
        log "警告: 磁盘使用率过高"
        return 1
    fi

    return 0
}

main() {
    log "=== 开始监控检查 ==="

    local status=0

    check_repo_status || status=1
    check_nfs_status || status=1
    check_disk_usage || status=1

    if [ $status -eq 0 ]; then
        log "=== 监控检查完成: 正常 ==="
    else
        log "=== 监控检查完成: 异常 ==="
    fi

    return $status
}

main
MONITORSCRIPT

    chmod +x "$NFS_DIR/monitor.sh"

    info "监控脚本创建完成"
}

# 创建维护脚本
create_maintenance_script() {
    info "创建维护脚本..."

    cat > "$NFS_DIR/maintenance.sh" << 'MAINTENANCESCRIPT'
#!/bin/bash
# openvela 仓库维护脚本

NFS_DIR="/nfs/openvela"
BARE_REPO_DIR="$NFS_DIR/bare-repo/openvela.git"
LOG_FILE="$NFS_DIR/logs/maintenance.log"
BACKUP_DIR="$NFS_DIR/backup"

log() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $1" | tee -a "$LOG_FILE"
}

# 垃圾回收
garbage_collect() {
    log "开始垃圾回收..."

    if [ ! -d "$BARE_REPO_DIR" ]; then
        log "错误: 裸仓库不存在"
        return 1
    fi

    cd "$BARE_REPO_DIR"

    # 执行垃圾回收
    git gc --aggressive --prune=now

    # 清理 reflog
    git reflog expire --expire=now --all

    log "垃圾回收完成"
}

# 创建备份
create_backup() {
    log "开始创建备份..."

    local timestamp=$(date +%Y%m%d%H%M%S)
    local backup_file="$BACKUP_DIR/openvela-bare-$timestamp.tar.gz"

    mkdir -p "$BACKUP_DIR"

    # 创建压缩备份
    tar -czf "$backup_file" -C "$NFS_DIR/bare-repo" openvela.git

    # 清理旧备份（保留最近 7 天）
    find "$BACKUP_DIR" -name "openvela-bare-*.tar.gz" -mtime +7 -delete

    log "备份创建完成: $backup_file"
}

# 清理日志
cleanup_logs() {
    log "清理旧日志..."

    # 清理 30 天前的日志
    find "$NFS_DIR/logs" -name "*.log" -mtime +30 -delete

    log "日志清理完成"
}

# 更新仓库
update_repo() {
    log "开始更新仓库..."

    if [ ! -d "$BARE_REPO_DIR" ]; then
        log "错误: 裸仓库不存在"
        return 1
    fi

    cd "$BARE_REPO_DIR"

    # 获取最新更新
    git fetch origin --prune

    # 获取所有标签
    git fetch origin --tags

    log "仓库更新完成"
}

# 检查仓库完整性
check_integrity() {
    log "检查仓库完整性..."

    if [ ! -d "$BARE_REPO_DIR" ]; then
        log "错误: 裸仓库不存在"
        return 1
    fi

    cd "$BARE_REPO_DIR"

    # 执行完整性检查
    if git fsck --full --no-dangling; then
        log "仓库完整性检查: 通过"
        return 0
    else
        log "仓库完整性检查: 失败"
        return 1
    fi
}

# 主菜单
show_menu() {
    echo ""
    echo "=========================================="
    echo "  openvela 仓库维护工具"
    echo "=========================================="
    echo ""
    echo "1. 垃圾回收"
    echo "2. 创建备份"
    echo "3. 清理日志"
    echo "4. 更新仓库"
    echo "5. 检查完整性"
    echo "6. 执行所有维护任务"
    echo "0. 退出"
    echo ""
    echo "=========================================="
    echo ""
}

# 主流程
main() {
    if [ "$1" = "--auto" ]; then
        # 自动模式，执行所有维护任务
        log "=== 开始自动维护 ==="
        update_repo
        garbage_collect
        create_backup
        cleanup_logs
        check_integrity
        log "=== 自动维护完成 ==="
        return
    fi

    while true; do
        show_menu
        read -p "请选择操作 [0-6]: " choice

        case $choice in
            1)
                garbage_collect
                ;;
            2)
                create_backup
                ;;
            3)
                cleanup_logs
                ;;
            4)
                update_repo
                ;;
            5)
                check_integrity
                ;;
            6)
                log "=== 开始完整维护 ==="
                update_repo
                garbage_collect
                create_backup
                cleanup_logs
                check_integrity
                log "=== 完整维护完成 ==="
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

main "$@"
MAINTENANCESCRIPT

    chmod +x "$NFS_DIR/maintenance.sh"

    info "维护脚本创建完成"
}

# 显示完成信息
show_completion() {
    echo ""
    echo "=========================================="
    echo "  裸仓库初始化完成"
    echo "=========================================="
    echo ""
    echo "裸仓库位置: $BARE_REPO_DIR/openvela.git"
    echo "LFS 存储: $LFS_STORAGE_DIR"
    echo "日志目录: $LOG_DIR"
    echo ""
    echo "在开发机上使用:"
    echo "  repo init -u $BARE_REPO_DIR/openvela.git \\"
    echo "    -b $BRANCH \\"
    echo "    -m openvela.xml \\"
    echo "    --reference=$BARE_REPO_DIR/openvela.git \\"
    echo "    --git-lfs"
    echo ""
    echo "维护命令:"
    echo "  监控: bash $NFS_DIR/monitor.sh"
    echo "  维护: bash $NFS_DIR/maintenance.sh"
    echo "  同步: bash $NFS_DIR/sync-repo.sh"
    echo ""
    echo "=========================================="
}

# 主流程
main() {
    info "=== openvela 裸仓库初始化 ==="

    create_directories
    install_dependencies
    init_git_lfs
    clone_bare_repo
    configure_bare_repo
    sync_all_branches
    create_repo_manifest
    create_sync_script
    create_monitor_script
    create_maintenance_script
    show_completion

    info "=== 裸仓库初始化完成 ==="
}

# 执行主流程
main
