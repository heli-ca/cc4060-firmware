# GitHub Actions 自动编译设置指南

## 快速开始（5分钟完成）

### 第1步：获取 GitHub Personal Access Token

1. 访问 https://github.com/settings/tokens
2. 点击 "Generate new token (classic)"
3. Token 描述填写：`CC4060 Firmware Build`
4. 过期时间选择：`7 days`（7天后自动过期，安全）
5. 勾选权限：只需勾选 `public_repo`（创建公开仓库的权限）
6. 点击底部 "Generate token"
7. **复制生成的 token**（格式类似 `ghp_xxxxxxxxxxxxxxxxxxxx`），只显示一次！

### 第2步：告诉我你的 Token

将复制的 token 提供给我，我会自动完成：
- ✅ 创建 GitHub 仓库
- ✅ 上传所有源代码
- ✅ 配置 GitHub Actions
- ✅ 触发自动编译
- ✅ 下载生成的 updata.bfu 固件

### 第3步：等待编译完成

GitHub Actions 会自动在 Ubuntu 服务器上编译固件，大约需要 3-5 分钟。

### 第4步：获取固件

编译完成后，我会下载 updata.bfu 并提供给你。

---

## 安全性说明

- Token 只用于本次操作，用完后你可以立即在 GitHub 设置中删除它
- 只会创建公开仓库，不包含任何敏感信息
- 所有代码都是开源的 CC4060 固件源码
- Token 7天后自动过期

---

## 替代方案：手动操作

如果你不想提供 token，也可以手动操作：

1. 访问 https://github.com/new
2. 仓库名填写：`cc4060-firmware`
3. 勾选 "Add a README file"
4. 点击 "Create repository"
5. 点击 "uploading an existing file"
6. 上传 `cc4060_firmware_for_github.zip` 解压后的所有文件
7. 点击 "Commit changes"
8. 等待 2-3 分钟，Actions 会自动编译
9. 在 Actions 标签页下载生成的 updata.bfu

---

## 推荐方案

**强烈推荐使用自动方案**（提供 token），因为：
- 更快（我帮你完成所有步骤）
- 更少出错（自动化流程）
- 更安全（token 7天自动过期）

请决定使用哪种方案？
