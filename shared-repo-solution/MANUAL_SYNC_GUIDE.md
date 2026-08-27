# openvela 共享仓库开发者手动同步方案

---

## 一、方案概述

### 1.1 背景

openvela 是基于 NuttX 的实时操作系统，使用 `repo` 工具管理多个 git 子仓库（nuttx、apps、vendor/openvela、frameworks、external、prebuilts）。完整代码仓库约 20GB，每位开发者独立克隆会导致：

- 磁盘空间大量浪费（N 台开发机 = N x 20GB）
- 首次克隆耗时长（1-2 小时）
- 网络带宽占用高

### 1.2 解决方案

采用 **NFS 共享存储 + git --reference** 的架构，在局域网内共享一份 git 对象库，各开发机通过 NFS 挂载引用该对象库，避免重复下载。

**核心原理：**

`git clone --reference <path>` 命令让 git 在克隆时优先从本地参考仓库读取对象，只有本地不存在的对象才从远程下载。将参考仓库存放在 NFS 共享目录上，多台开发机即可共享同一份对象数据。

**架构图：**

```
                         NFS 服务器
                ┌──────────────────────────┐
                │  /nfs/openvela/bare-repo/ │
                │  openvela.git (裸仓库)    │
                │  objects/ (所有git对象)   │
                │  refs/    (引用)          │
                └────────────┬─────────────┘
                             │ NFS (只读)
            ┌────────────────┼────────────────┐
            │                │                │
            v                v                v
     开发机 1 (ARM)    开发机 2 (RISC-V)  开发机 3 (ESP32)
     ~/openvela/       ~/openvela/        ~/openvela/
     ├── nuttx/        ├── nuttx/         ├── nuttx/
     ├── apps/         ├── apps/          ├── apps/
     └── vendor/       └── vendor/        └── vendor/
     --reference=      --reference=       --reference=
     NFS裸仓库         NFS裸仓库          NFS裸仓库
```

### 1.3 性能对比

| 指标 | 传统克隆 | 本方案 | 提升幅度 |
|------|----------|--------|----------|
| 首次同步 | 1-2 小时 | 15-30 分钟 | ~75% |
| 每台磁盘占用 | ~20 GB | ~5 GB | ~75% |
| 增量同步 | 5-10 分钟 | 1-2 分钟 | ~80% |
| 网络带宽 | 高 | 低 | ~90% |

### 1.4 前置条件

- NFS 服务器：Linux 主机，磁盘 >= 100GB，千兆网络
- 开发机：Linux（Ubuntu 20.04+ / Debian 11+ / CentOS 8+），磁盘 >= 10GB
- 网络：所有机器在同一局域网，能访问 gitee.com 或 github.com
- SSH 密钥：已配置 gitee.com 或 github.com 的 SSH 访问权限

---

## 二、手动操作步骤

整个方案分为 5 个阶段，按顺序执行。

### 阶段一：NFS 服务器配置（仅需执行一次）

以下所有命令在 NFS 服务器上以 root 权限执行。

**步骤 1.1 安装 NFS 服务**

```bash
# Ubuntu / Debian
sudo apt-get update
sudo apt-get install -y nfs-kernel-server nfs-common

# CentOS / RHEL / Rocky Linux
sudo yum install -y nfs-utils
```

**步骤 1.2 创建共享目录**

```bash
sudo mkdir -p /nfs/openvela/bare-repo
sudo mkdir -p /nfs/openvela/lfs-storage
sudo mkdir -p /nfs/openvela/logs
sudo mkdir -p /nfs/openvela/backup

# 设置权限（所有开发机用户可读写）
sudo chmod -R 777 /nfs/openvela
```

**步骤 1.3 配置 NFS 导出规则**

编辑 `/etc/exports`，添加以下内容（将 `192.168.1.0/24` 替换为实际子网）：

```bash
# 裸仓库目录 -- 只读，供开发机 --reference 使用
/nfs/openvela/bare-repo 192.168.1.0/24(ro,sync,no_subtree_check,no_root_squash)

# LFS 存储目录 -- 读写
/nfs/openvela/lfs-storage 192.168.1.0/24(rw,sync,no_subtree_check,no_root_squash)

# 日志目录 -- 读写
/nfs/openvela/logs 192.168.1.0/24(rw,sync,no_subtree_check,no_root_squash)

# 备份目录 -- 读写
/nfs/openvela/backup 192.168.1.0/24(rw,sync,no_subtree_check,no_root_squash)
```

**步骤 1.4 应用配置并启动 NFS**

```bash
# 使配置生效
sudo exportfs -ra

# 启动 NFS 服务
sudo systemctl enable nfs-kernel-server   # Ubuntu/Debian
sudo systemctl start nfs-kernel-server

# 或者 CentOS/RHEL:
# sudo systemctl enable nfs-server
# sudo systemctl start nfs-server
```

**步骤 1.5 配置防火墙**

```bash
# Ubuntu/Debian (ufw)
sudo ufw allow from 192.168.1.0/24 to any port 2049
sudo ufw allow from 192.168.1.0/24 to any port 111
sudo ufw allow from 192.168.1.0/24 to any port 20048

# CentOS/RHEL (firewalld)
sudo firewall-cmd --permanent --add-service=nfs
sudo firewall-cmd --permanent --add-service=rpc-bind
sudo firewall-cmd --permanent --add-service=mountd
sudo firewall-cmd --reload
```

**步骤 1.6 验证 NFS 服务**

```bash
# 检查服务状态
sudo systemctl status nfs-kernel-server

# 查看已导出的共享
showmount -e localhost

# 预期输出应包含 /nfs/openvela/bare-repo 等条目
```

---

### 阶段二：裸仓库初始化（仅需执行一次）

在 NFS 服务器上执行，创建作为 --reference 对象库的裸仓库。

**步骤 2.1 安装依赖**

```bash
sudo apt-get install -y git git-lfs

# 安装 repo 工具（如未安装）
curl -sSL https://storage.googleapis.com/git-repo-downloads/repo > /tmp/repo
chmod +x /tmp/repo
sudo mv /tmp/repo /usr/local/bin/repo
```

**步骤 2.2 克隆裸仓库**

openvela 使用 repo 管理多仓库，需要分别对 manifest 仓库和各子仓库创建裸仓库。以下是核心步骤：

```bash
cd /nfs/openvela/bare-repo

# 克隆 manifest 仓库（裸仓库模式）
git clone --bare ssh://git@gitee.com/open-vela/manifests.git manifests.git

# 使用 repo init 初始化（带 --reference 需要先有裸仓库）
# 创建临时工作目录
mkdir -p /tmp/openvela-init
cd /tmp/openvela-init

# 初始化 repo（使用 gitee 源）
repo init -u ssh://git@gitee.com/open-vela/manifests.git \
    -b dev \
    -m openvela.xml \
    --repo-url=https://mirrors.tuna.tsinghua.edu.cn/git/git-repo/

# 同步代码（首次同步会下载所有对象）
repo sync -c -j8

# 同步完成后，将各子仓库转为裸仓库存放到 NFS
for project_dir in nuttx apps vendor/openvela frameworks external prebuilts; do
    if [ -d "$project_dir/.git" ]; then
        repo_name=$(basename "$project_dir")
        git clone --bare "$project_dir/.git" "/nfs/openvela/bare-repo/${repo_name}.git"
        echo "已创建裸仓库: ${repo_name}.git"
    fi
done
```

**步骤 2.3 验证裸仓库**

```bash
# 检查裸仓库完整性
for repo in /nfs/openvela/bare-repo/*.git; do
    echo "检查: $repo"
    git --git-dir="$repo" fsck --no-dangling 2>&1 | tail -1
    git --git-dir="$repo" count-objects -v | grep -E "^(count|size)"
    echo "---"
done

# 查看分支列表
git --git-dir=/nfs/openvela/bare-repo/nuttx.git branch -a | head -20
```

**步骤 2.4 清理临时目录**

```bash
rm -rf /tmp/openvela-init
```

---

### 阶段三：开发机配置

以下所有命令在开发机上执行。

**步骤 3.1 挂载 NFS 共享**

```bash
# 创建挂载点
sudo mkdir -p /mnt/openvela-bare

# 临时挂载（测试用）
sudo mount -t nfs <NFS服务器IP>:/nfs/openvela/bare-repo /mnt/openvela-bare

# 验证挂载
ls /mnt/openvela-bare/
# 应看到 nuttx.git、apps.git 等裸仓库目录

# 配置开机自动挂载（持久化）
echo "<NFS服务器IP>:/nfs/openvela/bare-repo /mnt/openvela-bare nfs defaults,_netdev 0 0" | sudo tee -a /etc/fstab

# 挂载 LFS 存储（如果使用 LFS）
sudo mkdir -p /mnt/openvela-lfs
echo "<NFS服务器IP>:/nfs/openvela/lfs-storage /mnt/openvela-lfs nfs defaults,_netdev 0 0" | sudo tee -a /etc/fstab
sudo mount -a
```

**步骤 3.2 配置 Git 性能参数**

```bash
git config --global core.preloadindex true
git config --global core.fscache true
git config --global gc.auto 256
git config --global fetch.parallel 4
git config --global pull.rebase true
git config --global push.default current

# 如果使用 Git LFS
git config --global lfs.fetchinclude "*"
```

**步骤 3.3 初始化工作仓库**

```bash
mkdir -p ~/openvela
cd ~/openvela

# 使用 repo init 并指定 --reference 指向 NFS 裸仓库
repo init -u ssh://git@gitee.com/open-vela/manifests.git \
    -b dev \
    -m openvela.xml \
    --reference=/mnt/openvela-bare \
    --repo-url=https://mirrors.tuna.tsinghua.edu.cn/git/git-repo/
```

**说明：** `--reference=/mnt/openvela-bare` 会让 repo 在克隆各子仓库时，优先从 `/mnt/openvela-bare/<repo-name>.git` 读取 git 对象。只有本地参考仓库中不存在的对象才会从远程下载。

**步骤 3.4 同步代码**

```bash
cd ~/openvela

# 首次同步（-c 仅同步当前分支，-j8 并行 8 个任务）
repo sync -c -j8

# 后续增量同步
repo sync -c -j4
```

**步骤 3.5 验证同步结果**

```bash
cd ~/openvela

# 查看仓库信息
repo info -o | head -30

# 查看各子仓库状态
repo status

# 检查磁盘使用
du -sh .
# 预期约 5GB（而非 20GB）
```

---

### 阶段四：日常同步操作

开发者在日常工作中需要执行的同步操作。

**4.1 拉取最新代码**

```bash
cd ~/openvela
repo sync -c -j4
```

**4.2 创建开发分支**

```bash
# 在所有子仓库创建分支
repo start my-feature --all

# 或在特定子仓库创建分支
cd nuttx
git checkout -b my-feature
```

**4.3 提交代码**

```bash
# 在子仓库目录中提交
cd nuttx
git add .
git commit -m "feat: 描述你的修改"

# 推送到远程
git push origin HEAD:refs/for/dev
```

**4.4 更新裸仓库（NFS 服务器维护人员操作）**

当远程仓库有新提交时，需要手动更新 NFS 上的裸仓库：

```bash
# 在 NFS 服务器上执行
cd /nfs/openvela/bare-repo

# 更新所有裸仓库
for repo_dir in /nfs/openvela/bare-repo/*.git; do
    echo "更新: $repo_dir"
    git --git-dir="$repo_dir" fetch origin --prune
    git --git-dir="$repo_dir" fetch origin --tags
done

# 执行垃圾回收（可选，释放空间）
for repo_dir in /nfs/openvela/bare-repo/*.git; do
    echo "GC: $repo_dir"
    git --git-dir="$repo_dir" gc --auto
done
```

---

### 阶段五：备份与恢复

**5.1 创建备份（NFS 服务器）**

```bash
# 备份裸仓库
timestamp=$(date +%Y%m%d%H%M%S)
sudo tar -czf /nfs/openvela/backup/openvela-bare-${timestamp}.tar.gz \
    -C /nfs/openvela/bare-repo .

# 清理 7 天前的备份
find /nfs/openvela/backup -name "*.tar.gz" -mtime +7 -delete
```

**5.2 恢复备份（NFS 服务器）**

```bash
# 停止 NFS 服务
sudo systemctl stop nfs-kernel-server

# 恢复最近的备份
latest_backup=$(ls -t /nfs/openvela/backup/openvela-bare-*.tar.gz | head -1)
sudo tar -xzf "$latest_backup" -C /nfs/openvela/bare-repo/

# 恢复权限
sudo chmod -R 777 /nfs/openvela/bare-repo

# 启动 NFS 服务
sudo systemctl start nfs-kernel-server

# 验证
git --git-dir=/nfs/openvela/bare-repo/nuttx.git fsck --no-dangling
```

---

## 三、配置说明

### 3.1 NFS 导出参数说明

| 参数 | 含义 |
|------|------|
| `ro` | 只读，防止开发机意外修改裸仓库 |
| `rw` | 读写，用于日志、备份等需要写入的目录 |
| `sync` | 同步写入，保证数据一致性 |
| `no_subtree_check` | 禁用子树检查，提升性能 |
| `no_root_squash` | 不压缩 root 用户权限，避免权限问题 |
| `fsid=0` | 指定文件系统 ID，NFSv4 需要 |

### 3.2 NFS 挂载优化参数

```bash
# /etc/fstab 中的优化挂载选项
<NFS_SERVER_IP>:/nfs/openvela/bare-repo /mnt/openvela-bare nfs \
    defaults,_netdev,noatime,nodiratime,rsize=1048576,wsize=1048576 0 0
```

| 参数 | 含义 |
|------|------|
| `_netdev` | 等待网络就绪后再挂载 |
| `noatime` | 不更新访问时间，减少写操作 |
| `nodiratime` | 不更新目录访问时间 |
| `rsize=1048576` | 读缓冲区 1MB，提升大文件读取性能 |
| `wsize=1048576` | 写缓冲区 1MB |

### 3.3 Git 配置说明

```bash
# ~/.gitconfig 完整配置

[core]
    preloadindex = true      # 预加载索引，加速 git status
    fscache = true           # 启用文件系统缓存（Windows NTFS 有效）
    fsmonitor = true         # 启用文件系统监控

[gc]
    auto = 256               # 对象数超过 256 时自动 GC
    autopacklimit = 50       # 自动打包限制

[fetch]
    parallel = 4             # 并行 fetch 线程数

[pull]
    rebase = true            # pull 时使用 rebase

[push]
    default = current        # 推送当前分支
```

### 3.4 Repo 清单配置

openvela 的 repo 清单文件（`openvela.xml`）定义了所有子仓库：

```xml
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
```

### 3.5 磁盘空间规划

| 位置 | 用途 | 预估大小 |
|------|------|----------|
| NFS 服务器 - 裸仓库 | 所有 git 对象 | ~20 GB |
| NFS 服务器 - LFS 存储 | 大文件 | ~10 GB |
| NFS 服务器 - 备份 | 保留 7 天 | ~20 GB |
| NFS 服务器 - 日志 | 运行日志 | ~1 GB |
| NFS 服务器合计 | | ~51 GB |
| 开发机 - 代码仓库 | 本地工作副本 | ~5 GB |
| 开发机 - 构建产物 | 编译输出 | ~2 GB |
| 开发机合计 | | ~7 GB |

---

## 四、命令示例

### 4.1 NFS 服务器操作

```bash
# 检查 NFS 服务状态
sudo systemctl status nfs-kernel-server

# 重新加载导出配置
sudo exportfs -ra

# 查看当前导出列表
showmount -e localhost

# 查看 NFS 客户端连接
showmount -a localhost

# 重启 NFS 服务
sudo systemctl restart nfs-kernel-server
```

### 4.2 裸仓库操作

```bash
# 检查裸仓库完整性
git --git-dir=/nfs/openvela/bare-repo/nuttx.git fsck --no-dangling

# 查看裸仓库对象统计
git --git-dir=/nfs/openvela/bare-repo/nuttx.git count-objects -v

# 查看裸仓库分支
git --git-dir=/nfs/openvela/bare-repo/nuttx.git branch -a

# 更新裸仓库
git --git-dir=/nfs/openvela/bare-repo/nuttx.git fetch origin --prune

# 手动垃圾回收
git --git-dir=/nfs/openvela/bare-repo/nuttx.git gc --aggressive --prune=now
```

### 4.3 开发机操作

```bash
# 挂载 NFS
sudo mount -t nfs 192.168.1.100:/nfs/openvela/bare-repo /mnt/openvela-bare

# 检查挂载状态
mountpoint -q /mnt/openvela-bare && echo "已挂载" || echo "未挂载"

# 初始化仓库（首次）
cd ~/openvela
repo init -u ssh://git@gitee.com/open-vela/manifests.git \
    -b dev -m openvela.xml --reference=/mnt/openvela-bare

# 同步代码
repo sync -c -j8

# 创建开发分支
repo start my-feature --all

# 查看分支状态
repo status

# 查看最新提交
repo info -o | head -20

# 切换分支
repo checkout dev
```

### 4.4 多芯片构建命令

```bash
cd ~/openvela

# ---- 乐鑫 ESP32 (Xtensa) ----
# 前置：安装 ESP-IDF 并 source export.sh
source ~/esp/esp-idf/export.sh
./build.sh vendor/espressif/boards/esp32/configs/default --cmake -j$(nproc)

# ---- 乐鑫 ESP32-C3 (RISC-V) ----
./build.sh vendor/espressif/boards/esp32c3/configs/default --cmake -j$(nproc)

# ---- ARM Cortex-M4 (STM32F4) ----
sudo apt-get install -y gcc-arm-none-eabi
./build.sh vendor/stm32/boards/stm32f4/configs/nsh --cmake -j$(nproc)

# ---- ARM Cortex-M7 (STM32H7) ----
./build.sh vendor/stm32/boards/stm32h7/configs/nsh --cmake -j$(nproc)

# ---- Nordic nRF52 (ARM Cortex-M4) ----
./build.sh vendor/nordic/boards/nrf52/configs/nsh --cmake -j$(nproc)

# ---- RISC-V 64 (QEMU 模拟器) ----
sudo apt-get install -y gcc-riscv64-linux-gnu qemu-system-misc
./build.sh vendor/openvela/boards/vela/configs/goldfish-riscv64 --cmake -j$(nproc)

# ---- ARM64 (QEMU 模拟器) ----
sudo apt-get install -y gcc-aarch64-linux-gnu qemu-system-arm
./build.sh vendor/openvela/boards/vela/configs/goldfish-arm64-v8a-ap --cmake -j$(nproc)

# ---- menuconfig（修改内核配置）----
./build.sh vendor/openvela/boards/vela/configs/goldfish-arm64-v8a-ap --cmake menuconfig

# ---- 清理后重新构建 ----
./build.sh vendor/openvela/boards/vela/configs/goldfish-arm64-v8a-ap --cmake --clean -j$(nproc)
```

### 4.5 运行模拟器

```bash
cd ~/openvela

# 运行 ARM64 模拟器
./emulator.sh cmake_out/vela_goldfish-arm64-v8a-ap/

# 运行 RISC-V 模拟器
./emulator.sh cmake_out/vela_goldfish-riscv64/

# 无图形模式运行
./emulator.sh -no-window cmake_out/vela_goldfish-arm64-v8a-ap/

# 调试模式运行（等待 GDB 连接）
./emulator.sh -s -S cmake_out/vela_goldfish-arm64-v8a-ap/
# 另一个终端：
gdb cmake_out/vela_goldfish-arm64-v8a-ap/nuttx
(gdb) target remote :1234
(gdb) continue
```

---

## 五、常见问题解答

### Q1: NFS 挂载失败，提示 "Connection timed out"

**原因：** 网络不通或防火墙阻断。

**排查步骤：**

```bash
# 1. 测试网络连通性
ping <NFS服务器IP>

# 2. 测试 NFS 端口
nc -zv <NFS服务器IP> 2049

# 3. 检查 NFS 服务是否运行
ssh <NFS服务器IP> "systemctl status nfs-kernel-server"

# 4. 检查防火墙规则
ssh <NFS服务器IP> "sudo ufw status"
```

**解决方案：**

```bash
# 确保防火墙放行 NFS 端口
sudo ufw allow from 192.168.1.0/24 to any port 2049
sudo ufw allow from 192.168.1.0/24 to any port 111
sudo ufw allow from 192.168.1.0/24 to any port 20048
```

---

### Q2: repo init 时报错 "fatal: failed to read object"

**原因：** NFS 裸仓库损坏或 NFS 挂载不稳定导致对象读取失败。

**排查步骤：**

```bash
# 1. 检查 NFS 挂载状态
mountpoint -q /mnt/openvela-bare && echo "已挂载" || echo "未挂载"

# 2. 检查裸仓库完整性
git --git-dir=/mnt/openvela-bare/nuttx.git fsck --no-dangling

# 3. 检查 NFS 读取是否正常
ls -la /mnt/openvela-bare/nuttx.git/objects/
```

**解决方案：**

```bash
# 重新挂载 NFS
sudo umount /mnt/openvela-bare
sudo mount -t nfs <NFS服务器IP>:/nfs/openvela/bare-repo /mnt/openvela-bare

# 如果裸仓库损坏，在 NFS 服务器上从备份恢复
sudo systemctl stop nfs-kernel-server
sudo tar -xzf /nfs/openvela/backup/openvela-bare-*.tar.gz -C /nfs/openvela/bare-repo/
sudo systemctl start nfs-kernel-server
```

---

### Q3: repo sync 部分子仓库失败

**原因：** 网络中断、SSH 密钥问题、或 --reference 对象库不完整。

**排查步骤：**

```bash
# 1. 查看详细错误
repo sync -c -j4 -v 2>&1 | tee sync-debug.log

# 2. 检查 SSH 连接
ssh -T git@gitee.com

# 3. 检查哪个子仓库失败
repo status
```

**解决方案：**

```bash
# 重试同步（repo 会跳过已成功的子仓库）
repo sync -c -j4

# 强制重新同步特定子仓库
repo sync -c -j4 --force-sync nuttx

# 如果 --reference 对象不完整，不使用 reference 重试
cd ~/openvela
rm -rf .repo
repo init -u ssh://git@gitee.com/open-vela/manifests.git -b dev -m openvela.xml
repo sync -c -j4
```

---

### Q4: 磁盘空间不足

**排查步骤：**

```bash
# 检查各目录大小
du -sh ~/openvela/
du -sh ~/openvela/.repo/
du -sh /nfs/openvela/

# 查找大文件
find ~/openvela -type f -size +100M -exec ls -lh {} \;
```

**解决方案：**

```bash
# 清理构建产物
rm -rf ~/openvela/cmake_out/

# 清理 git 缓存
cd ~/openvela/nuttx
git gc --aggressive --prune=now

# NFS 服务器：清理旧备份和日志
find /nfs/openvela/backup -name "*.tar.gz" -mtime +7 -delete
find /nfs/openvela/logs -name "*.log" -mtime +30 -delete
```

---

### Q5: Git LFS 文件下载失败

**症状：** 文件内容为 LFS 指针而非实际数据。

**排查步骤：**

```bash
# 检查 LFS 状态
git lfs status

# 检查 LFS 对象列表
git lfs ls-files

# 验证 LFS 指针
git lfs pointer --check --file <filename>
```

**解决方案：**

```bash
# 安装/重新安装 Git LFS
sudo apt-get install -y git-lfs
git lfs install

# 重新拉取 LFS 文件
git lfs pull

# 清理并重新获取所有 LFS 对象
git lfs fetch --all
git lfs checkout
```

---

### Q6: 开发机无法写入 NFS 挂载目录

**症状：** `Permission denied` 或 `Read-only file system`。

**排查步骤：**

```bash
# 检查挂载选项
mount | grep openvela

# 检查目录权限
ls -la /mnt/openvela-bare/
```

**解决方案：**

```bash
# 检查 /etc/exports 中是否使用了 ro（只读）选项
# 裸仓库目录应为 ro（只读），这是设计如此，开发机不应直接修改裸仓库

# 如果需要写入日志/备份目录，确保对应的 NFS 导出配置为 rw
# /nfs/openvela/logs 192.168.1.0/24(rw,sync,no_subtree_check,no_root_squash)
```

---

### Q7: repo sync 速度很慢

**排查步骤：**

```bash
# 检查 NFS 延迟
time ls /mnt/openvela-bare/nuttx.git/objects/

# 检查网络带宽
iperf3 -c <NFS服务器IP>
```

**解决方案：**

```bash
# 1. 优化 NFS 挂载参数
sudo umount /mnt/openvela-bare
sudo mount -t nfs -o noatime,nodiratime,rsize=1048576,wsize=1048576 \
    <NFS服务器IP>:/nfs/openvela/bare-repo /mnt/openvela-bare

# 2. 增加 NFS 服务器线程数（NFS 服务器上操作）
echo "RPCNFSDCOUNT=16" | sudo tee -a /etc/default/nfs-kernel-server
sudo systemctl restart nfs-kernel-server

# 3. 增加 repo 并行任务数
repo sync -c -j16

# 4. 优化 git 配置
git config --global fetch.parallel 4
git config --global pack.threads 4
```

---

### Q8: 裸仓库需要更新（NFS 服务器）

当远程仓库有新的提交时，需要手动更新 NFS 上的裸仓库。

**操作步骤：**

```bash
# 在 NFS 服务器上执行

# 方法一：逐个更新
for repo_dir in /nfs/openvela/bare-repo/*.git; do
    echo "更新: $(basename $repo_dir)"
    git --git-dir="$repo_dir" fetch origin --prune
    git --git-dir="$repo_dir" fetch origin --tags
done

# 方法二：使用维护脚本（如果已部署）
bash /nfs/openvela/maintenance.sh --action update

# 更新完成后，开发机执行增量同步即可获取最新代码
# 开发机上：repo sync -c -j4
```

---

### Q9: 不同芯片架构的工具链问题

| 芯片 | 工具链缺失时的报错 | 安装命令 |
|------|---------------------|----------|
| ESP32 (Xtensa) | `xtensa-esp32-elf-gcc: not found` | `source ~/esp/esp-idf/export.sh` |
| ESP32-C3 (RISC-V) | `riscv32-esp-elf-gcc: not found` | `source ~/esp/esp-idf/export.sh` |
| STM32 (ARM) | `arm-none-eabi-gcc: not found` | `sudo apt-get install -y gcc-arm-none-eabi` |
| RISC-V 64 | `riscv64-linux-gnu-gcc: not found` | `sudo apt-get install -y gcc-riscv64-linux-gnu` |
| ARM64 | `aarch64-linux-gnu-gcc: not found` | `sudo apt-get install -y gcc-aarch64-linux-gnu` |

---

### Q10: 如何添加新的子仓库到共享方案

当 openvela 项目新增子仓库时：

```bash
# 1. 在 NFS 服务器上创建新的裸仓库
cd /nfs/openvela/bare-repo
git clone --bare ssh://git@gitee.com/open-vela/<新仓库名>.git <新仓库名>.git

# 2. 验证
git --git-dir=/nfs/openvela/bare-repo/<新仓库名>.git branch -a

# 3. 开发机同步时会自动通过 --reference 引用
cd ~/openvela
repo sync -c -j4
```

---

## 六、芯片适配速查表

| 芯片型号 | 架构 | 配置路径 | 工具链 | 特殊说明 |
|----------|------|----------|--------|----------|
| ESP32 | Xtensa LX6 | `vendor/espressif/boards/esp32/configs/default` | ESP-IDF | 520KB SRAM，WiFi/BLE 共用射频 |
| ESP32-S2 | Xtensa LX7 | `vendor/espressif/boards/esp32s2/configs/default` | ESP-IDF | 单核，不支持 SMP |
| ESP32-S3 | Xtensa LX7 | `vendor/espressif/boards/esp32s3/configs/default` | ESP-IDF | 双核，支持 AI 加速 |
| ESP32-C3 | RISC-V | `vendor/espressif/boards/esp32c3/configs/default` | ESP-IDF | RISC-V 架构，BLE 5.0 |
| ESP32-C6 | RISC-V | `vendor/espressif/boards/esp32c6/configs/default` | ESP-IDF | WiFi 6, Thread/Zigbee |
| STM32F4 | Cortex-M4 | `vendor/stm32/boards/stm32f4/configs/nsh` | arm-none-eabi | 带 FPU，DSP 指令 |
| STM32F7 | Cortex-M7 | `vendor/stm32/boards/stm32f7/configs/nsh` | arm-none-eabi | L1 缓存，高性能 |
| STM32H7 | Cortex-M7 | `vendor/stm32/boards/stm32h7/configs/nsh` | arm-none-eabi | 480MHz，双精度 FPU |
| nRF52 | Cortex-M4 | `vendor/nordic/boards/nrf52/configs/nsh` | arm-none-eabi | BLE 5.0，NFC |
| RP2040 | Cortex-M0+ | `vendor/raspberrypi/boards/rp2040/configs/nsh` | arm-none-eabi | 双核，PIO |
| QEMU ARM64 | Cortex-A53 | `vendor/openvela/boards/vela/configs/goldfish-arm64-v8a-ap` | aarch64-linux-gnu | 模拟器，SMP |
| QEMU ARM32 | Cortex-A7 | `vendor/openvela/boards/vela/configs/goldfish-armeabi-v7a-ap` | arm-linux-gnueabihf | 模拟器 |
| QEMU RISC-V | RV64 | `vendor/openvela/boards/vela/configs/goldfish-riscv64` | riscv64-linux-gnu | 模拟器，OpenSBI |
