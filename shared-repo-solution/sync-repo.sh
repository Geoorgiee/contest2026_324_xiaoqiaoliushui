#!/bin/bash
# openvela 仓库同步脚本
# 用于从 NFS 共享仓库同步代码到本地开发环境
# Usage: bash sync-repo.sh [options]
#
# Options:
#   --nfs-server <ip>      NFS 服务器 IP (默认: 自动检测)
#   --work-dir <path>      工作目录 (默认: ~/openvela)
#   --branch <branch>      分支 (dev|trunk) (默认: dev)
#   --source <source>      代码源 (gitee|github) (默认: gitee)
#   --protocol <proto>     协议 (ssh|https) (默认: ssh)
#   --lfs                  启用 Git LFS
#   --force                强制重新同步
#   --jobs <num>           并行任务数 (默认: 8)

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
NFS_SERVER=""
WORK_DIR="$HOME/openvela"
BRANCH="dev"
SOURCE="gitee"
PROTOCOL="ssh"
ENABLE_LFS=false
FORCE_SYNC=false
JOBS=8
MOUNT_POINT="/mnt/openvela-bare"
NFS_DIR="/nfs/openvela"

# 解析命令行参数
while [[ $# -gt 0 ]]; do
    case $1 in
        --nfs-server)
            NFS_SERVER="$2"
            shift 2
            ;;
        --work-dir)
            WORK_DIR="$2"
            shift 2
            ;;
        --branch)
            BRANCH="$2"
            shift 2
            ;;
        --source)
            SOURCE="$2"
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
        --force)
            FORCE_SYNC=true
            shift
            ;;
        --jobs)
            JOBS="$2"
            shift 2
            ;;
        --help|-h)
            echo "Usage: bash sync-repo.sh [options]"
            echo ""
            echo "Options:"
            echo "  --nfs-server <ip>      NFS 服务器 IP"
            echo "  --work-dir <path>      工作目录 (默认: ~/openvela)"
            echo "  --branch <branch>      分支 (dev|trunk) (默认: dev)"
            echo "  --source <source>      代码源 (gitee|github) (默认: gitee)"
            echo "  --protocol <proto>     协议 (ssh|https) (默认: ssh)"
            echo "  --lfs                  启用 Git LFS"
            echo "  --force                强制重新同步"
            echo "  --jobs <num>           并行任务数 (默认: 8)"
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

info "=== openvela 仓库同步 ==="
info "NFS 服务器: $NFS_SERVER"
info "工作目录: $WORK_DIR"
info "分支: $BRANCH"
info "代码源: $SOURCE"
info "协议: $PROTOCOL"
info "Git LFS: $ENABLE_LFS"
info "并行任务: $JOBS"

# 检查 NFS 挂载
check_nfs_mount() {
    info "检查 NFS 挂载..."

    if ! mountpoint -q "$MOUNT_POINT" 2>/dev/null; then
        warn "NFS 未挂载，尝试挂载..."
        sudo mkdir -p "$MOUNT_POINT"
        sudo mount -t nfs "$NFS_SERVER:$NFS_DIR" "$MOUNT_POINT"

        if [ $? -ne 0 ]; then
            error "NFS 挂载失败"
        fi
    fi

    # 检查裸仓库
    if [ ! -d "$MOUNT_POINT/bare-repo/openvela.git" ]; then
        error "裸仓库不存在: $MOUNT_POINT/bare-repo/openvela.git"
    fi

    info "NFS 挂载检查完成"
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

# 初始化仓库
init_repo() {
    info "初始化仓库..."

    # 创建工作目录
    mkdir -p "$WORK_DIR"
    cd "$WORK_DIR"

    # 检查是否已初始化
    if [ -d ".repo" ]; then
        if [ "$FORCE_SYNC" = true ]; then
            warn "强制重新同步，删除 .repo 目录..."
            rm -rf .repo
        else
            warn "检测到已存在的 .repo 目录，跳过初始化"
            return
        fi
    fi

    local repo_url=$(get_repo_url)
    local reference_path="$MOUNT_POINT/bare-repo/openvela.git"

    # 构建 repo init 命令
    local cmd="repo init -u $repo_url -b $BRANCH -m openvela.xml --reference=$reference_path"

    # 如果启用了 LFS
    if [ "$ENABLE_LFS" = true ]; then
        cmd="$cmd --git-lfs"
    fi

    # 添加镜像源（如果使用 gitee）
    if [ "$SOURCE" = "gitee" ]; then
        cmd="$cmd --repo-url=https://mirrors.tuna.tsinghua.edu.cn/git/git-repo/"
    fi

    info "执行: $cmd"
    eval "$cmd"

    if [ $? -eq 0 ]; then
        info "仓库初始化完成"
    else
        error "仓库初始化失败"
    fi
}

# 同步代码
sync_repo() {
    info "同步代码..."

    cd "$WORK_DIR"

    # 构建 repo sync 命令
    local cmd="repo sync -c -j$JOBS"

    # 如果是强制同步
    if [ "$FORCE_SYNC" = true ]; then
        cmd="$cmd --force-sync"
    fi

    info "执行: $cmd"

    # 记录开始时间
    local start_time=$(date +%s)

    # 执行同步
    eval "$cmd" 2>&1 | tee sync.log

    # 记录结束时间
    local end_time=$(date +%s)
    local duration=$((end_time - start_time))

    if [ $? -eq 0 ]; then
        info "代码同步完成，耗时: ${duration}秒"
    else
        error "代码同步失败，请检查 sync.log"
    fi
}

# 更新 LFS 文件
update_lfs() {
    if [ "$ENABLE_LFS" = true ]; then
        info "更新 Git LFS 文件..."

        cd "$WORK_DIR"

        # 更新 LFS 文件
        git lfs pull

        info "Git LFS 文件更新完成"
    fi
}

# 显示同步状态
show_sync_status() {
    info "同步状态:"

    cd "$WORK_DIR"

    # 显示最新提交
    echo ""
    echo "最新提交:"
    repo info -o | head -20

    # 显示分支信息
    echo ""
    echo "分支信息:"
    repo branch | head -20

    # 显示磁盘使用
    echo ""
    echo "磁盘使用: $(du -sh . | awk '{print $1}')"
}

# 创建快速同步脚本
create_quick_sync_script() {
    info "创建快速同步脚本..."

    cat > "$WORK_DIR/quick-sync.sh" << 'QUICKSYNC'
#!/bin/bash
# openvela 快速同步脚本
# 用于日常开发中的代码同步

set -e

GREEN='\033[0;32m'
NC='\033[0m'

info() { echo -e "${GREEN}[INFO]${NC} $1"; }

info "开始快速同步..."

cd "$(dirname "$0")"

# 同步代码
repo sync -c -j8

# 显示更新
info "最新更新:"
repo info -o | head -10

info "快速同步完成"
QUICKSYNC

    chmod +x "$WORK_DIR/quick-sync.sh"

    info "快速同步脚本创建完成"
}

# 创建开发分支脚本
create_dev_branch_script() {
    info "创建开发分支脚本..."

    cat > "$WORK_DIR/create-dev-branch.sh" << 'DEVBRANCH'
#!/bin/bash
# openvela 创建开发分支脚本
# 用于创建和管理开发分支

set -e

GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

info() { echo -e "${GREEN}[INFO]${NC} $1"; }
error() { echo -e "${RED}[ERROR]${NC} $1"; exit 1; }

BRANCH_NAME="${1:-}"
PROJECT="${2:-}"

if [ -z "$BRANCH_NAME" ]; then
    echo "Usage: bash create-dev-branch.sh <branch-name> [project]"
    echo ""
    echo "Example:"
    echo "  bash create-dev-branch.sh feature/my-feature"
    echo "  bash create-dev-branch.sh feature/my-feature nuttx"
    exit 1
fi

info "创建开发分支: $BRANCH_NAME"

cd "$(dirname "$0")"

if [ -n "$PROJECT" ]; then
    # 在特定项目中创建分支
    cd "$PROJECT"
    git checkout -b "$BRANCH_NAME"
    info "在 $PROJECT 中创建分支: $BRANCH_NAME"
else
    # 在所有项目中创建分支
    repo start "$BRANCH_NAME" --all
    info "在所有项目中创建分支: $BRANCH_NAME"
fi

info "开发分支创建完成"
DEVBRANCH

    chmod +x "$WORK_DIR/create-dev-branch.sh"

    info "开发分支脚本创建完成"
}

# 显示完成信息
show_completion() {
    echo ""
    echo "=========================================="
    echo "  仓库同步完成"
    echo "=========================================="
    echo ""
    echo "工作目录: $WORK_DIR"
    echo "分支: $BRANCH"
    echo ""
    echo "常用命令:"
    echo "  快速同步: cd $WORK_DIR && ./quick-sync.sh"
    echo "  创建分支: cd $WORK_DIR && ./create-dev-branch.sh <branch-name>"
    echo "  构建固件: cd $WORK_DIR && ./build-chip.sh <chip>"
    echo "  运行模拟器: cd $WORK_DIR && ./emulator.sh <output_dir>"
    echo ""
    echo "=========================================="
}

# 主流程
main() {
    info "=== openvela 仓库同步 ==="

    check_nfs_mount
    init_repo
    sync_repo
    update_lfs
    show_sync_status
    create_quick_sync_script
    create_dev_branch_script
    show_completion

    info "=== 仓库同步完成 ==="
}

# 执行主流程
main
