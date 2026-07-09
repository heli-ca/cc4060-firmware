#!/usr/bin/env python3
"""
Jieli BFU (JL_UDFIR) 固件升级包构建工具

用于打包/解包杰理AC692x系列芯片的BFU格式固件升级包。
生成的 updata.bfu 可直接放入U盘，设备bootloader自动识别并升级。

用法:
  python bfu_builder.py build   <firmware.bin> <output.bfu> [--template ref.bfu]
  python bfu_builder.py extract <input.bfu>    <output.bin>
  python bfu_builder.py verify  <input.bfu>
  python bfu_builder.py info    <input.bfu>
"""

import struct
import sys
import os
import argparse

# BFU 常量
BFU_MAGIC       = b"JL_UDFIR"
BFU_HEADER_SIZE = 0x200          # 512字节头部
BFU_FILENAME    = b"JL_692X.BIN\x00"
BFU_BFUD_MAGIC  = b"BFUD"
BFU_DNEF_MAGIC  = b"DNEF"


def crc16_modbus(data: bytes) -> int:
    """计算 CRC16-Modbus (与杰理协议一致)"""
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc & 0xFFFF


def build_bfu_header(payload_size: int, template_header: bytes = None,
                     device_id: int = 0x000076C0) -> bytes:
    """
    构建512字节BFU头部。

    参数:
      payload_size:   固件二进制数据大小
      template_header: 可选，参考BFU的原始头部(保留BFUD/DNEF等标记)
      device_id:      设备ID (默认0x76C0 = AC692x系列)
    返回:
      512字节的头部数据
    """
    if template_header and len(template_header) >= BFU_HEADER_SIZE:
        # 以原始头部为模板，仅更新 payload_size
        header = bytearray(template_header[:BFU_HEADER_SIZE])
        struct.pack_into(">I", header, 0x14, payload_size)
        return bytes(header)

    # 从零构建头部
    header = bytearray(BFU_HEADER_SIZE)

    # 0x00: Magic "JL_UDFIR"
    header[0:8] = BFU_MAGIC

    # 0x08: header_size = 0x20 (大端)
    struct.pack_into(">I", header, 0x08, 0x20)

    # 0x0C: device_id (大端)
    struct.pack_into(">I", header, 0x0C, device_id)

    # 0x10: data_offset = 0x200 (大端)
    struct.pack_into(">I", header, 0x10, BFU_HEADER_SIZE)

    # 0x14: payload_size (大端)
    struct.pack_into(">I", header, 0x14, payload_size)

    # 0x18: checksum (先置0，后面计算)
    struct.pack_into(">I", header, 0x18, 0)

    # 0x1C: reserved = 0
    struct.pack_into(">I", header, 0x1C, 0)

    # 0x24: filename
    header[0x24:0x24 + len(BFU_FILENAME)] = BFU_FILENAME

    # 0x30: BFUD section marker (与原始文件一致)
    header[0x30:0x34] = BFU_BFUD_MAGIC
    # BFUD 后续12字节: 典型值
    struct.pack_into(">I", header, 0x34, 0x00186C00 + payload_size)
    struct.pack_into(">I", header, 0x38, 0x14000001)

    # 0x3C: DNEF section marker
    header[0x3C:0x40] = BFU_DNEF_MAGIC

    # 计算头部checksum (简单累加 0x00~0x1F 除checksum字段)
    checksum = 0
    for i in range(0, 0x20):
        if 0x18 <= i < 0x1C:
            continue  # 跳过checksum字段本身
        checksum = (checksum + header[i]) & 0xFFFFFFFF
    struct.pack_into(">I", header, 0x18, checksum)

    return bytes(header)


def build_bfu(firmware_bin: bytes, template_bfu: bytes = None,
              device_id: int = 0x000076C0) -> bytes:
    """
    将固件二进制数据打包为BFU格式。

    参数:
      firmware_bin: 固件二进制数据
      template_bfu: 可选，参考BFU文件内容(用作头部模板)
      device_id:    设备ID
    返回:
      完整的BFU文件内容
    """
    template_header = None
    if template_bfu and len(template_bfu) >= BFU_HEADER_SIZE:
        if template_bfu[:8] == BFU_MAGIC:
            template_header = template_bfu[:BFU_HEADER_SIZE]

    payload_size = len(firmware_bin)
    header = build_bfu_header(payload_size, template_header, device_id)
    return header + firmware_bin


def extract_bfu(bfu_data: bytes) -> tuple:
    """
    从BFU文件中提取固件二进制数据。

    返回: (firmware_bin, header_info_dict)
    """
    if len(bfu_data) < BFU_HEADER_SIZE + 1:
        raise ValueError("文件太小，不是有效的BFU文件")

    if bfu_data[:8] != BFU_MAGIC:
        raise ValueError(f"Magic错误: 期望 'JL_UDFIR', 得到 {bfu_data[:8]!r}")

    header_size = struct.unpack_from(">I", bfu_data, 0x08)[0]
    data_offset = struct.unpack_from(">I", bfu_data, 0x10)[0]
    payload_size = struct.unpack_from(">I", bfu_data, 0x14)[0]
    device_id = struct.unpack_from(">I", bfu_data, 0x0C)[0]
    checksum = struct.unpack_from(">I", bfu_data, 0x18)[0]

    # 读取文件名
    filename_raw = bfu_data[0x24:0x34]
    filename = filename_raw.split(b"\x00")[0].decode("ascii", errors="replace")

    # 检查section标记
    has_bfud = bfu_data[0x30:0x34] == BFU_BFUD_MAGIC
    has_dnef = bfu_data[0x3C:0x40] == BFU_DNEF_MAGIC

    info = {
        "header_size": header_size,
        "data_offset": data_offset,
        "payload_size": payload_size,
        "device_id": device_id,
        "checksum": checksum,
        "filename": filename,
        "has_bfud": has_bfud,
        "has_dnef": has_dnef,
        "file_size": len(bfu_data),
        "actual_payload": len(bfu_data) - data_offset,
    }

    firmware = bfu_data[data_offset:data_offset + payload_size]
    return firmware, info


def verify_bfu(bfu_data: bytes) -> dict:
    """
    验证BFU文件完整性。

    返回验证结果字典。
    """
    results = {"valid": True, "checks": []}

    def check(name, passed, detail=""):
        status = "PASS" if passed else "FAIL"
        results["checks"].append({"name": name, "status": status, "detail": detail})
        if not passed:
            results["valid"] = False

    # 基本检查
    check("Magic", bfu_data[:8] == BFU_MAGIC,
          f"实际值: {bfu_data[:8]!r}")

    if len(bfu_data) < BFU_HEADER_SIZE:
        check("最小尺寸", False, f"文件大小 {len(bfu_data)} < {BFU_HEADER_SIZE}")
        return results

    data_offset = struct.unpack_from(">I", bfu_data, 0x10)[0]
    payload_size = struct.unpack_from(">I", bfu_data, 0x14)[0]
    actual_payload = len(bfu_data) - data_offset

    check("数据偏移", data_offset == BFU_HEADER_SIZE,
          f"偏移值=0x{data_offset:X}, 期望=0x{BFU_HEADER_SIZE:X}")

    check("载荷大小", payload_size == actual_payload,
          f"头部声明={payload_size}, 实际={actual_payload}")

    check("文件完整性", len(bfu_data) >= data_offset + payload_size,
          f"文件大小={len(bfu_data)}, 需要={data_offset + payload_size}")

    # 文件名检查
    filename_raw = bfu_data[0x24:0x34]
    filename = filename_raw.split(b"\x00")[0].decode("ascii", errors="replace")
    check("文件名", filename.upper().endswith(".BIN"),
          f"文件名: {filename}")

    return results


def print_info(info: dict):
    """打印BFU文件信息"""
    device_names = {
        0x000076C0: "AC692x (JL_692X)",
        0x00004400: "AC692x SDK Demo",
    }
    dev_name = device_names.get(info["device_id"], "未知")

    print(f"=== BFU 固件升级包信息 ===")
    print(f"  文件大小:     {info['file_size']:,} 字节 ({info['file_size']/1024:.1f} KB)")
    print(f"  头部大小:     {info['header_size']} (0x{info['header_size']:X})")
    print(f"  数据偏移:     0x{info['data_offset']:X}")
    print(f"  设备ID:       0x{info['device_id']:08X} ({dev_name})")
    print(f"  校验和:       0x{info['checksum']:08X}")
    print(f"  固件文件名:   {info['filename']}")
    print(f"  固件大小:     {info['payload_size']:,} 字节 ({info['payload_size']/1024:.1f} KB)")
    print(f"  实际载荷:     {info['actual_payload']:,} 字节")
    print(f"  BFUD标记:     {'有' if info['has_bfud'] else '无'}")
    print(f"  DNEF标记:     {'有' if info['has_dnef'] else '无'}")

    match = info['payload_size'] == info['actual_payload']
    print(f"  大小一致性:   {'✓ 匹配' if match else '✗ 不匹配!'}")


def main():
    parser = argparse.ArgumentParser(
        description="杰理 JL BFU 固件升级包工具",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  %(prog)s info    updata.bfu                    # 查看BFU信息
  %(prog)s verify  updata.bfu                    # 验证BFU完整性
  %(prog)s extract updata.bfu firmware.bin       # 提取固件
  %(prog)s build   firmware.bin new_updata.bfu   # 打包固件为BFU
  %(prog)s build   firmware.bin new_updata.bfu --template ref.bfu
        """)

    subparsers = parser.add_subparsers(dest="command", help="操作命令")

    # info
    p_info = subparsers.add_parser("info", help="显示BFU文件信息")
    p_info.add_argument("input", help="输入BFU文件路径")

    # verify
    p_verify = subparsers.add_parser("verify", help="验证BFU文件")
    p_verify.add_argument("input", help="输入BFU文件路径")

    # extract
    p_extract = subparsers.add_parser("extract", help="从BFU提取固件")
    p_extract.add_argument("input", help="输入BFU文件路径")
    p_extract.add_argument("output", help="输出固件文件路径")

    # build
    p_build = subparsers.add_parser("build", help="打包固件为BFU")
    p_build.add_argument("firmware", help="输入固件二进制文件路径")
    p_build.add_argument("output", help="输出BFU文件路径")
    p_build.add_argument("--template", help="参考BFU文件(用作头部模板)")
    p_build.add_argument("--device-id", type=lambda x: int(x, 0),
                         default=0x000076C0, help="设备ID (默认: 0x000076C0)")

    args = parser.parse_args()

    if not args.command:
        parser.print_help()
        sys.exit(1)

    if args.command == "info":
        with open(args.input, "rb") as f:
            bfu_data = f.read()
        _, info = extract_bfu(bfu_data)
        print_info(info)

    elif args.command == "verify":
        with open(args.input, "rb") as f:
            bfu_data = f.read()
        results = verify_bfu(bfu_data)
        for c in results["checks"]:
            icon = "✓" if c["status"] == "PASS" else "✗"
            print(f"  {icon} {c['name']}: {c['status']}  {c['detail']}")
        print()
        if results["valid"]:
            print("验证通过: BFU文件格式正确")
        else:
            print("验证失败: BFU文件存在问题")
            sys.exit(1)

    elif args.command == "extract":
        with open(args.input, "rb") as f:
            bfu_data = f.read()
        firmware, info = extract_bfu(bfu_data)
        with open(args.output, "wb") as f:
            f.write(firmware)
        print(f"已提取固件: {args.output} ({len(firmware):,} 字节)")
        print_info(info)

    elif args.command == "build":
        with open(args.firmware, "rb") as f:
            firmware_bin = f.read()

        template_data = None
        if args.template:
            with open(args.template, "rb") as f:
                template_data = f.read()
            print(f"使用模板: {args.template}")

        bfu_data = build_bfu(firmware_bin, template_data, args.device_id)

        with open(args.output, "wb") as f:
            f.write(bfu_data)

        print(f"已生成BFU: {args.output} ({len(bfu_data):,} 字节)")
        print(f"  固件大小: {len(firmware_bin):,} 字节")

        # 自动验证
        results = verify_bfu(bfu_data)
        all_pass = all(c["status"] == "PASS" for c in results["checks"])
        if all_pass:
            print("  验证: ✓ 通过")
        else:
            print("  验证: ✗ 失败")
            for c in results["checks"]:
                if c["status"] == "FAIL":
                    print(f"    - {c['name']}: {c['detail']}")


if __name__ == "__main__":
    main()
