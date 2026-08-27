# ESP32-P4 开发环境搭建指南

> 本文档记录在 Ubuntu VM 上为 openvela ESP32-P4 适配搭建开发环境的完整过程，包括遇到的所有问题及解决办法。
>
> 最后更新：2026-08-25

---

## 1. 环境概述

| 项目 | 说明 |
|------|------|
| 目标平台 | ESP32-P4 Function EV Board |
| 开发环境 | Ubuntu 22.04 VM（16GB RAM, 125GB 磁盘） |
| 开发工具 | VS Code Remote SSH + ESP-IDF Extension |
| 用户权限 | 无 sudo 权限（普通用户 `geo`） |
| 工程目录 | `/home/geo/openvela/` |

### 1.1 约束说明

由于开发环境为受限的 Linux VM，以下限制贯穿整个搭建过程：

- **无 sudo 权限** — 所有工具必须安装在用户目录下
- **网络受限** — VS Code Marketplace、GitHub 不可达，需使用替代下载方式
- **内存有限** — 16GB RAM，编译时需注意并行度

---

## 2. 问题排查清单

以下问题按搭建过程中遇到的先后顺序排列。

### 问题 1：RISC-V 交叉编译工具链缺失

**现象**

系统中没有 `riscv-none-elf-gcc`，编译 ESP32-P4（RISC-V 架构）目标时报错 `command not found`。

**原因**

Ubuntu 默认不包含 RISC-V 交叉编译工具链，需要手动安装。ESP32-P4 使用的是 `riscv-none-elf` 目标三元组。

**解决办法**

下载 [xPack RISC-V GNU Toolchain v14.2.0](https://github.com/xpack-dev-tools/riscv-none-elf-gcc-xpack/releases/tag/v14.2.0-3)，安装到用户目录：

```bash
# 1. 创建工具链目录
mkdir -p ~/tools/xpack-riscv-none-elf-gcc

# 2. 下载工具链（注意：GitHub 不可达时需通过其他渠道获取压缩包）
wget https://github.com/xpack-dev-tools/riscv-none-elf-gcc-xpack/releases/download/v14.2.0-3/xpack-riscv-none-elf-gcc-14.2.0-3-linux-x64.tar.gz

# 3. 解压到目标目录
tar -xzf xpack-riscv-none-elf-gcc-14.2.0-3-linux-x64.tar.gz \
    -C ~/tools/xpack-riscv-none-elf-gcc --strip-components=1

# 4. 添加到 PATH（写入 bashrc 持久化）
echo 'export PATH=~/tools/xpack-riscv-none-elf-gcc/bin:$PATH' >> ~/.bashrc
source ~/.bashrc
```

> **注意**：如果 GitHub 不可达，可在 Windows 本地浏览器下载后通过 `scp` 传到 VM：
> ```bash
> scp xpack-riscv-none-elf-gcc-*.tar.gz geo@172.20.10.2:/tmp/
> ```

**验证**

```bash
riscv-none-elf-gcc --version
# 预期输出：riscv-none-elf-gcc (xPack GNU RISC-V Embedded GCC x86_64) 14.2.0
```

---

### 问题 2：VS Code Remote SSH 连接配置

**现象**

需要从 Windows 宿主机连接 Linux VM 进行远程开发，本地 VS Code 无法直接访问 VM 上的工程文件。

**解决办法**

使用 VS Code 的 Remote-SSH 扩展实现远程开发：

1. **Windows 本地**：安装 [Remote - SSH](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-ssh) 扩展
2. 按 `Ctrl+Shift+P`，输入 `Remote-SSH: Connect to Host...`
3. 输入连接信息：`geo@172.20.10.2`（替换为实际 VM IP）
4. 输入密码完成连接
5. 连接成功后，`文件 → 打开文件夹`，选择 `/home/geo/openvela`

**连接后的效果**

- VS Code 左下角显示 `SSH: 172.20.10.2`，表示已进入远程模式
- 终端（Terminal）直接使用 VM 的 bash，所有命令在 VM 上执行
- 文件操作直接作用于 VM 文件系统

---

### 问题 3：ESP-IDF 插件安装报 "Failed to fetch"

**现象**

在 VS Code Remote SSH 模式下，通过扩展市场搜索安装 ESP-IDF 扩展时，报错 **"Failed to fetch"**，安装失败。

**原因**

Remote SSH 模式下，扩展需要在**远程端（Linux VM）**下载安装。但 VM 网络无法访问 VS Code Marketplace（SSL 连接失败）。

**诊断**

```bash
# 测试 Marketplace 连通性
curl -s -o /dev/null -w "%{http_code}" https://marketplace.visualstudio.com/_apis/public/gallery/extensionquery
# 预期：返回 000（连接失败）或超时
```

**解决办法：手动 .vsix 安装**

绕过 Marketplace，从 Windows 本地浏览器下载扩展包，手动安装到远程：

1. **Windows 浏览器**访问 ESP-IDF 扩展页面：
   ```
   https://marketplace.visualstudio.com/items?itemName=espressif.esp-idf-extension
   ```
2. 点击页面上的 **"Download Extension"** 链接，下载 `.vsix` 文件
3. 回到 VS Code（已通过 Remote SSH 连接）
4. `Ctrl+Shift+P` → 输入 `Extensions: Install from VSIX...`
5. 浏览选择刚才下载的 `.vsix` 文件
6. 等待安装完成，必要时重新加载窗口

**验证**

安装完成后，在扩展面板中应能看到 `ESP-IDF` 扩展已安装（显示在"已安装"列表中，标记为"SSH: ..."）。

---

### 问题 4：pip 未安装 (ERROR_INVALID_PIP)

**现象**

配置 ESP-IDF 扩展时（`ESP-IDF: Configure ESP-IDF Extension`），报错：

```
"/usr/bin/python3 -m pip" is not valid. (ERROR_INVALID_PIP)
```

**原因**

系统有 Python3 但没有安装 pip 模块。`python3 -m pip` 执行失败，提示 `No module named pip`。

**诊断步骤**

```bash
# 1. 确认 Python3 存在
python3 --version
# 预期：Python 3.10.12

# 2. 确认 pip 是否可用
python3 -m pip --version
# 预期报错：/usr/bin/python3: No module named pip

# 3. 确认 ensurepip 是否可用（备用方案）
python3 -m ensurepip
# 如果可用可尝试此路径安装
```

**解决办法**

使用 `get-pip.py` 以用户模式安装 pip（无需 sudo）：

```bash
# 1. 用 Python 内置 urllib 下载 get-pip.py（比 curl 更可靠，见问题 5）
python3 -c "import urllib.request; urllib.request.urlretrieve('https://bootstrap.pypa.io/get-pip.py', '/tmp/get-pip.py')"

# 2. 以用户模式安装 pip
python3 /tmp/get-pip.py --user

# 3. 将用户级 bin 目录加入 PATH
echo 'export PATH="$HOME/.local/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc
```

**验证**

```bash
python3 -m pip --version
# 预期：pip 26.2.1 from /home/geo/.local/lib/python3.10/site-packages/pip (python 3.10)
```

> **重要**：安装 pip 后，需要重新启动 ESP-IDF 扩展的配置向导，它才能正确检测到 pip。

---

### 问题 5：get-pip.py 下载损坏

**现象**

用 `curl` 下载 `get-pip.py` 后执行，报 Python 语法错误：

```
SyntaxError: unterminated triple-quoted string literal
```

**原因**

网络连接不稳定，导致 `curl` 下载的文件被截断，`get-pip.py` 内容不完整。

**诊断**

```bash
# 检查文件大小（正常应约 2.5MB）
ls -la /tmp/get-pip.py

# 检查文件末尾是否完整
tail -5 /tmp/get-pip.py
# 截断的文件末尾会出现异常内容
```

**解决办法**

使用 Python 内置 `urllib` 下载，比 `curl` 更能处理不稳定的网络连接：

```bash
# 用 Python 下载（会自动重试、处理连接问题）
python3 -c "import urllib.request; urllib.request.urlretrieve('https://bootstrap.pypa.io/get-pip.py', '/tmp/get-pip.py')"
```

**经验总结**

| 下载方式 | 可靠性 | 备注 |
|----------|--------|------|
| `python3 urllib` | 高 | 内置 HTTP 栈，自动处理重试 |
| `curl` | 低 | 网络不稳定时易截断，不报错 |
| `wget` | 中 | 比 curl 好，但仍可能截断 |

在当前网络环境下，**优先使用 Python urllib 下载文件**。

---

## 3. 环境验证清单

搭建完成后，逐项运行以下命令确认环境就绪：

```bash
# ========== 1. RISC-V 工具链 ==========
riscv-none-elf-gcc --version
# 预期：14.2.0

riscv-none-elf-g++ --version
# 预期：14.2.0

# ========== 2. Python 环境 ==========
python3 --version
# 预期：3.10.12

python3 -m pip --version
# 预期：26.2.1 from ~/.local/lib/python3.10/site-packages/pip

# ========== 3. VS Code 远程连接 ==========
# - VS Code 左下角显示 "SSH: <IP>"
# - Remote-SSH 扩展已安装

# ========== 4. ESP-IDF 扩展 ==========
# - 扩展面板中 ESP-IDF 已安装（通过 .vsix）
# - Ctrl+Shift+P → "ESP-IDF: Configure ESP-IDF Extension" 可正常启动
# - 选择 ESP-IDF 版本后能正常下载配置
```

全部通过后，环境即就绪，可以进入编译阶段。

---

## 4. 常用工具链信息

| 工具 | 版本 | 安装路径 |
|------|------|----------|
| riscv-none-elf-gcc | 14.2.0 | `~/tools/xpack-riscv-none-elf-gcc/bin/` |
| Python | 3.10.12 | `/usr/bin/python3` |
| pip | 26.2.1 | `~/.local/bin/pip`（用户级安装） |
| VS Code Remote-SSH | latest | Windows 本地 VS Code |
| ESP-IDF Extension | latest | `.vsix` 手动安装到远程 |

---

## 5. 网络问题备忘

### 5.1 资源可达性

| 资源 | 可达 | 备注 |
|------|------|------|
| VS Code Marketplace | 不通 | SSL 连接失败 |
| GitHub (`github.com`) | 不通 | 连接被重置 |
| PyPI (`pypi.org`) | 可达 | curl / python3 均可 |
| Open VSX | 可达 | 返回 301 跳转 |
| 阿里云镜像 (`mirrors.aliyun.com`) | 可达 | 国内镜像可用 |

### 5.2 网络问题应对策略

当遇到网络问题时，按优先级使用以下方案：

| 问题场景 | 推荐方案 |
|----------|----------|
| 下载文件失败 | 用 `python3 urllib` 代替 `curl` |
| pip install 超时 | 使用阿里云镜像：`pip install -i https://mirrors.aliyun.com/pypi/simple/ <包名>` |
| VS Code 扩展安装失败 | Windows 浏览器下载 `.vsix`，手动安装到远程 |
| GitHub 资源下载失败 | Windows 本地下载后 `scp` 传到 VM |
| ESP-IDF 下载失败 | 使用国内镜像或离线包 |

### 5.3 pip 国内镜像配置

如果 pip 下载速度慢或超时，配置阿里云镜像源：

```bash
# 临时使用
pip install -i https://mirrors.aliyun.com/pypi/simple/ <包名>

# 永久配置
pip config set global.index-url https://mirrors.aliyun.com/pypi/simple/
```

---

## 6. 后续步骤

环境就绪后，按以下步骤进行编译验证：

1. **初始化 openvela 工程**
   ```bash
   cd /home/geo/openvela
   repo init -u <manifest-url>
   repo sync
   ```

2. **配置并编译 ESP32-P4 目标**
   ```bash
   # 使用最小 NSH 配置
   ./tools/configure.sh evb_esp32p4:nsh
   make -j$(nproc)
   ```

3. **修复编译错误** — 根据编译日志逐个解决

4. **烧录硬件测试**
   ```bash
   # ESP-IDF 烧录工具
   esptool.py --chip esp32p4 --port /dev/ttyUSB0 write_flash 0x0 nuttx.bin
   ```

5. **串口终端验证**
   ```bash
   minicom -D /dev/ttyUSB0 -b 115200
   # 或使用 screen / cu
   ```

---

## 附录：快速参考卡片

### 环境变量配置（~/.bashrc 追加内容）

```bash
# RISC-V 工具链
export PATH=~/tools/xpack-riscv-none-elf-gcc/bin:$PATH

# Python 用户级 pip
export PATH="$HOME/.local/bin:$PATH"
```

### 常用诊断命令

```bash
# 工具链诊断
which riscv-none-elf-gcc
riscv-none-elf-gcc --version

# Python 环境诊断
python3 --version
python3 -m pip --version
python3 -c "import pip; print(pip.__file__)"

# 网络诊断
curl -s -o /dev/null -w "%{http_code}" https://pypi.org
curl -s -o /dev/null -w "%{http_code}" https://marketplace.visualstudio.com

# 磁盘空间
df -h /home/geo
du -sh ~/tools/*
```

---

*文档维护者：geo@openvela | 最后更新：2026-08-25*
