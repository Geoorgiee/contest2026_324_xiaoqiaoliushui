# openvela 共享仓库故障排查指南

## 常见问题

### 1. NFS 连接问题

#### 问题: 无法连接到 NFS 服务器

**症状:**
```
mount.nfs: Connection timed out
mount.nfs: access denied by server while mounting
```

**排查步骤:**

1. **检查网络连接:**
   ```bash
   ping <nfs-server-ip>
   ```

2. **检查 NFS 服务状态:**
   ```bash
   ssh <nfs-server-ip> "systemctl status nfs-kernel-server"
   ```

3. **检查防火墙:**
   ```bash
   # Ubuntu/Debian
   sudo ufw status

   # CentOS/RHEL
   sudo firewall-cmd --list-all
   ```

4. **检查 NFS 导出:**
   ```bash
   ssh <nfs-server-ip> "showmount -e localhost"
   ```

5. **检查端口:**
   ```bash
   nc -zv <nfs-server-ip> 2049
   ```

**解决方案:**

- 确保 NFS 服务正在运行
- 检查防火墙规则，允许 NFS 端口 (2049, 111, 20048)
- 检查 `/etc/exports` 配置是否正确
- 确保客户端 IP 在允许的子网内

---

### 2. Git --reference 问题

#### 问题: git fetch 失败

**症状:**
```
fatal: failed to read object <hash>: Invalid argument
error: Could not fetch origin
```

**排查步骤:**

1. **检查裸仓库完整性:**
   ```bash
   git --git-dir=/mnt/openvela-bare/bare-repo/openvela.git fsck
   ```

2. **检查引用:**
   ```bash
   git --git-dir=/mnt/openvela-bare/bare-repo/openvela.git show-ref
   ```

3. **检查对象:**
   ```bash
   git --git-dir=/mnt/openvela-bare/bare-repo/openvela.git count-objects -v
   ```

**解决方案:**

- 如果裸仓库损坏，从备份恢复
- 执行垃圾回收: `git gc --aggressive --prune=now`
- 重新克隆裸仓库

---

### 3. Repo 同步问题

#### 问题: repo sync 失败

**症状:**
```
error: Cannot fetch <project>
error: Exited with code 1
```

**排查步骤:**

1. **检查网络连接:**
   ```bash
   curl -I https://gitee.com
   ```

2. **检查 SSH 密钥:**
   ```bash
   ssh -T git@gitee.com
   ```

3. **检查 repo 版本:**
   ```bash
   repo version
   ```

4. **查看详细日志:**
   ```bash
   repo sync -c -j8 -v
   ```

**解决方案:**

- 检查网络连接和代理设置
- 确保 SSH 密钥已正确配置
- 更新 repo 工具到最新版本
- 减少并行任务数: `repo sync -c -j4`

---

### 4. Git LFS 问题

#### 问题: LFS 文件损坏

**症状:**
```
Pointer file error: Not a valid LFS pointer file
LFS:-lfs-file: smudge error
```

**排查步骤:**

1. **检查 LFS 状态:**
   ```bash
   git lfs status
   ```

2. **检查 LFS 对象:**
   ```bash
   git lfs ls-files
   ```

3. **验证 LFS 指针:**
   ```bash
   git lfs pointer --check --file <file>
   ```

**解决方案:**

```bash
# 重新安装 Git LFS
git lfs install

# 重新拉取 LFS 文件
git lfs pull

# 清理并重新获取
git lfs fetch --all
git lfs checkout
```

---

### 5. 权限问题

#### 问题: 权限被拒绝

**症状:**
```
Permission denied (publickey)
fatal: Could not read from remote repository
```

**排查步骤:**

1. **检查 SSH 密钥:**
   ```bash
   ls -la ~/.ssh/
   ```

2. **测试 SSH 连接:**
   ```bash
   ssh -T git@gitee.com
   ```

3. **检查文件权限:**
   ```bash
   ls -la /mnt/openvela-bare/
   ```

**解决方案:**

- 确保 SSH 密钥已添加到 Gitee/GitHub
- 检查 NFS 挂载点的权限设置
- 确保用户在正确的用户组中

---

### 6. 磁盘空间问题

#### 问题: 磁盘空间不足

**症状:**
```
error: unable to write file
fatal: failed to write object
```

**排查步骤:**

1. **检查磁盘使用:**
   ```bash
   df -h
   du -sh /nfs/openvela/
   ```

2. **检查大文件:**
   ```bash
   find /nfs/openvela -type f -size +100M
   ```

**解决方案:**

```bash
# 垃圾回收
git gc --aggressive --prune=now

# 清理旧备份
find /nfs/openvela/backup -name "*.tar.gz" -mtime +7 -delete

# 清理旧日志
find /nfs/openvela/logs -name "*.log" -mtime +30 -delete
```

---

## 性能优化

### 1. NFS 性能调优

```bash
# /etc/default/nfs-kernel-server
RPCNFSDCOUNT=16  # 增加工作线程数

# /etc/fstab
<NFS_SERVER>:/nfs/openvela /mnt/openvela-bare nfs defaults,noatime,nodiratime,rsize=1048576,wsize=1048576 0 0
```

### 2. Git 性能优化

```bash
# 启用并行操作
git config --global fetch.parallel 4
git config --global pack.threads 4

# 启用文件系统缓存
git config --global core.fscache true
git config --global core.preloadindex true

# 启用增量打包
git config --global repack.usedeltabaseoffset true
```

### 3. Repo 性能优化

```bash
# 增加并行任务数
repo sync -c -j16

# 使用浅克隆
repo init --depth=1

# 禁用标签同步（如果不需要）
repo init --no-tags
```

---

## 监控和告警

### 1. 设置监控

```bash
# 启动监控服务
bash monitor.sh --nfs-server <ip> --daemon

# 查看监控日志
tail -f /var/log/openvela-monitor.log
```

### 2. 配置告警

```bash
# 邮件告警
bash monitor.sh --nfs-server <ip> --alert-email admin@example.com

# 系统日志告警
tail -f /var/log/syslog | grep openvela-monitor
```

---

## 备份和恢复

### 1. 创建备份

```bash
# 手动备份
bash maintenance.sh --action backup

# 自动备份（cron）
0 2 * * * /path/to/maintenance.sh --action backup --auto
```

### 2. 恢复备份

```bash
# 停止 NFS 服务
sudo systemctl stop nfs-kernel-server

# 恢复备份
sudo tar -xzf /nfs/openvela/backup/openvela-bare-*.tar.gz -C /nfs/openvela/bare-repo/

# 启动 NFS 服务
sudo systemctl start nfs-kernel-server

# 验证恢复
git --git-dir=/nfs/openvela/bare-repo/openvela.git fsck
```

---

## 联系支持

如果以上方法都无法解决问题，请：

1. 收集错误日志
2. 记录操作步骤
3. 联系技术支持

```bash
# 收集诊断信息
bash monitor.sh --report > diagnostic-report.txt
```
