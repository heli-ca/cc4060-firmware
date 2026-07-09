# CC4060 固件编译 - 超简单方案

## 问题说明

你之前刷入的 updata.bfu 是杰理 SDK 的演示固件（使用 RCSP 协议），不是真正的 CC4060 固件。这就是为什么小程序无法控制设备。

**根本原因**：CC4060 固件源代码（ble_bridge.c）需要编译成真正的固件，但编译器 pi32-clang 只能在 Linux x86-64 或 Windows x86-64 上运行，不能在 macOS ARM64 上运行。

## 最简单的解决方案（5分钟）

### 方法1：使用 Gitpod（免费在线 Linux 环境）⭐推荐

1. **访问 Gitpod**
   - 打开浏览器访问：https://gitpod.io/
   - 用 GitHub 账号登录（如果没有，先注册一个，免费）

2. **创建工作区**
   - 点击 "New Workspace"
   - 在 "Context URL" 输入框中，粘贴你的 GitHub 仓库地址（见下方说明）
   - 或者更简单：直接访问 https://gitpod.io/#https://github.com/YOUR_USERNAME/cc4060-firmware

3. **等待环境准备**
   - Gitpod 会自动启动一个 Ubuntu 22.04 容器
   - 自动下载工具链和依赖（约 2-3 分钟）

4. **编译固件**
   - 在终端中运行：`./build.sh`
   - 等待编译完成（约 1-2 分钟）
   - 成功后会生成 `updata.bfu` 文件

5. **下载固件**
   - 在左侧文件浏览器中找到 `updata.bfu`
   - 右键点击 → Download
   - 保存到本地

### 方法2：使用 GitHub Codespaces（同样免费）

1. 在你的 GitHub 仓库页面
2. 点击绿色 "Code" 按钮
3. 选择 "Codespaces" 标签
4. 点击 "Create codespace on main"
5. 等待环境启动后，在终端运行 `./build.sh`
6. 编译完成后下载 `updata.bfu`

### 方法3：使用 Windows 电脑（如果你有）

1. 将 `cc4060_complete_firmware` 文件夹复制到 Windows 电脑
2. 安装 Python 3（从 python.org 下载）
3. 打开 PowerShell，进入项目目录
4. 运行：`.\build.sh`（如果安装了 Git Bash）
   - 或者手动运行 Makefile

---

## 我需要你做什么？

### 选项 A：我帮你自动化整个流程（最快）

如果你愿意提供一个临时的 GitHub Personal Access Token，我可以：
1. ✅ 自动创建 GitHub 仓库
2. ✅ 上传所有代码
3. ✅ 触发 GitHub Actions 编译
4. ✅ 下载生成的 updata.bfu
5. ✅ 立即提供给你

**Token 获取方法**：
1. 访问 https://github.com/settings/tokens
2. 点击 "Generate new token (classic)"
3. 描述填写：`CC4060 Build`
4. 过期时间：`7 days`
5. 只勾选权限：`public_repo`
6. 点击生成，复制 token
7. 把 token 告诉我

**安全性**：Token 7天后自动过期，只用一次，非常安全。

### 选项 B：你自己按上述步骤操作

按照上面的 "方法1：使用 Gitpod" 步骤，5分钟内可以完成。

---

## 编译完成后如何验证？

```bash
# 查看固件信息
python3 tools/bfu_builder.py info updata.bfu

# 验证固件完整性
python3 tools/bfu_builder.py verify updata.bfu
```

应该看到：
- 设备ID: 0x000076C0 (AC692x)
- 文件名: JL_692X.BIN
- 验证通过

---

## 刷机步骤

1. 准备 FAT32 格式 U 盘
2. 将 `updata.bfu` 复制到 U 盘根目录
3. 插入 CC4060 设备的 USB 口
4. 设备上电，自动升级（10-30秒）
5. 自动重启，完成！

---

## 常见问题

**Q: 为什么不能在我的 Mac 上编译？**
A: pi32-clang 编译器是为 x86-64 架构设计的，Mac M1/M2 是 ARM64 架构，无法运行。

**Q: Gitpod 真的免费吗？**
A: 是的，Gitpod 为个人用户提供每月 50 小时的免费使用时间，编译一次只需要几分钟。

**Q: 编译需要多长时间？**
A: 在 Gitpod/GitHub Actions 上，从开始到生成固件大约 3-5 分钟。

**Q: 如果我提供 GitHub Token，安全吗？**
A: 非常安全。Token 只授予创建公开仓库的权限，不包含任何敏感信息，且 7 天后自动过期。

---

## 现在请决定

请告诉我你想使用哪种方案：
- **方案 A**：提供 GitHub Token，我帮你全自动完成（最快）
- **方案 B**：你自己按 Gitpod 步骤操作

无论选择哪种，最终都能得到真正的 CC4060 固件！
