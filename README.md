# CC4060 BLE Bridge Firmware

真正的 CC4060 协议固件，用于杰理 AC6921A BLE 芯片。

## 功能特性

- ✅ 完整的 CC4060 协议支持（帧头 `01FE0000`）
- ✅ 31段PEQ均衡器
- ✅ 6声道独立控制（FL/FR/RL/RR/C/SW）
- ✅ BLE GATT 透明传输（UUID: 0xAE00, ae01写入/ae02通知）
- ✅ UART1 连接 AK7738 DSP（115200bps）
- ✅ CRC16 Modbus 校验

## 编译方法

### 方法1：GitHub Actions（推荐）

1. Fork 或克隆此仓库到 GitHub
2. Push 代码后，GitHub Actions 会自动编译
3. 在 Actions 页面下载生成的 `updata.bfu`

### 方法2：本地编译（需要 Linux x86-64）

```bash
# 安装依赖
sudo apt-get install -y python3 wget

# 运行构建脚本
./build.sh

# 或者手动编译
make TOOLCHAIN_PATH=/path/to/pi32v2
```

## 刷机方法

1. 准备一个 FAT32 格式的 U 盘
2. 将 `updata.bfu` 复制到 U 盘根目录
3. 将 U 盘插入 CC4060 设备的 USB 接口
4. 设备上电，会自动检测并升级
5. 等待 10-30 秒，设备自动重启
6. 升级完成！

## 验证固件

```bash
python3 tools/bfu_builder.py verify updata.bfu
python3 tools/bfu_builder.py info updata.bfu
```

## 文件结构

```
cc4060_complete_firmware/
├── src/                    # 源代码
│   ├── ble_bridge.c       # BLE桥接主程序
│   ├── ble_bridge.h       # 头文件
│   └── app_config.h       # 配置文件
├── tools/                  # 工具
│   └── bfu_builder.py     # BFU打包工具
├── .github/workflows/      # GitHub Actions
│   └── build-firmware.yml
├── Makefile               # 编译脚本
├── build.sh               # 自动化构建脚本
└── README.md              # 说明文档
```

## 技术细节

- **芯片**: JL AC6921A (PI32架构, BR21平台, 160MHz)
- **编译器**: pi32-clang (仅支持 Linux x86-64 / Windows x86-64)
- **BLE服务**: UUID 0xAE00
  - 写入特征: 0xAE01
  - 通知特征: 0xAE02
- **UART**: UART1 (PB04/RX, PB05/TX), 115200bps
- **Flash布局**: 256KB总容量, 200KB代码, 16KB VM, 16KB BTIF

## 许可证

本项目基于杰理 SDK 开发，仅供学习研究使用。
