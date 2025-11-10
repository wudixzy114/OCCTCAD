#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
通过 pygccxml + CastXML 生成 XML 描述文件（支持将多个头文件合并成一个 XML）。
使用方法：在 Tooling 目录下运行本脚本（脚本会切换到自身目录）。
"""
import os
import tempfile
from datetime import datetime
import shutil
import sys
import config

from pygccxml import utils, parser


def read_header_list():
    """从 class_list_to_reflect.txt 读取需要解析的头文件列表（去注释、空行，规范化分隔符）。"""
    headers = []
    if not os.path.exists(config.CLASS_LIST_FILE):
        raise FileNotFoundError(f"Class list file not found: {config.CLASS_LIST_FILE}")

    with open(config.CLASS_LIST_FILE, 'r', encoding='utf-8') as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            # 规范化路径分隔符（保留相对路径）
            headers.append(line.replace('/', os.sep).replace('\\', os.sep))
    return headers


def _write_combined_header(mirrored_paths, target_path):
    """
    生成一个临时组合头文件，内容为对所有 mirrored_paths 的 #include "..."
    返回写入的文件路径（target_path）。
    """
    with open(target_path, 'w', encoding='utf-8') as out_f:
        out_f.write("// Auto-generated combined header for pygccxml parsing\n")
        out_f.write("// Generated: %s\n\n" % datetime.now().isoformat())
        for p in mirrored_paths:
            # 使用相对路径或绝对路径都可以；为了避免路径分隔问题，用绝对路径
            out_f.write(f'#include "{p}"\n')
    return target_path


def run_castxml():
    """
    使用 pygccxml 的 API 调用 CastXML，生成单一 XML 文件（路径由 config.XML_FILE_PATH 指定）。
    返回 True 表示成功，False 表示失败（并在失败时写日志 / 打印错误信息）。
    """
    print("--- [Module 2] Running CastXML via pygccxml API ---")

    # 确保输出目录存在
    os.makedirs(config.GENERATED_DIR, exist_ok=True)

    headers_to_parse = read_header_list()
    if not headers_to_parse:
        print("Warning: No headers found in class_list.txt. Nothing to do.")
        return True

    # 构造镜像文件路径
    mirrored_header_paths = [os.path.join(config.MIRRORED_DIR, h) for h in headers_to_parse]

    # 验证镜像文件存在
    missing = [p for p in mirrored_header_paths if not os.path.exists(p)]
    if missing:
        print("\nFATAL ERROR: The following mirrored header files were not found:")
        for m in missing:
            print("  -", m)
        print("\nPlease ensure 'gen_mirror_occt.py' has been run successfully first.\n")
        return False

    # 查找 CastXML 可执行文件及其名称（castxml / gccxml）
    try:
        generator_path, generator_name = utils.find_xml_generator()
        print(f"Found xml generator: {generator_name} at: {generator_path}")
    except Exception as e:
        print("\nFATAL ERROR: Could not find CastXML (or supported xml generator).")
        print("Make sure CastXML is installed and in PATH, or provide its path.")
        # 记录日志
        with open(config.CASTXML_LOG_FILE, 'a', encoding='utf-8') as log_f:
            log_f.write(f"\n--- CastXML Not Found ({datetime.now()}) ---\n")
            log_f.write(str(e) + "\n")
            log_f.write("--- End ---\n")
        return False

    # 构造 xml_generator_configuration_t
    xml_generator_config = parser.xml_generator_configuration_t(
        xml_generator_path=generator_path,
        xml_generator=generator_name,
        include_paths=[config.MIRRORED_DIR, config.OCCT_INC_PATH],
        # 若需要额外的 cflags，可以在 config 中配置或在此追加
        # 注意：pygccxml 接受 cflags 字符串
        compiler_path=config.COMPILER_PATH,
        cflags="-std=c++17 -DHAVE_RTTI -DHAVE_CPP11 -DOCC_CONVERT_SIGNALS"
    )

    # 准备一个临时组合头文件，把所有要解析的头文件包含进来，这样可以生成一个合并的 XML
    tmp_dir = None
    try:
        tmp_dir = tempfile.mkdtemp(prefix="pygccxml_combined_")
        combined_header = os.path.join(tmp_dir, "combined_for_parsing.hpp")
        _write_combined_header(mirrored_header_paths, combined_header)

        print(f"Parsing {len(mirrored_header_paths)} header(s) via combined header...")

        # 使用 source_reader_t 来显式创建 XML 文件；这样可以指定输出文件路径
        source_reader = parser.source_reader_t(xml_generator_config)

        # 如果目标 xml 文件所在目录不存在，先创建
        xml_dir = os.path.dirname(config.XML_FILE_PATH)
        if xml_dir:
            os.makedirs(xml_dir, exist_ok=True)

        # 调用 create_xml_file，指定 destination 为 config.XML_FILE_PATH
        generated_xml = source_reader.create_xml_file(combined_header, destination=config.XML_FILE_PATH)

        # 记录日志
        with open(config.CASTXML_LOG_FILE, 'a', encoding='utf-8') as log_f:
            log_f.write(f"\n--- CastXML Execution Log ({datetime.now()}) ---\n")
            log_f.write(f"Generator: {generator_name} ({generator_path})\n")
            log_f.write(f"Combined header: {combined_header}\n")
            log_f.write(f"Generated XML: {generated_xml}\n")
            log_f.write("--- End CastXML Log ---\n")

        print(f"CastXML finished successfully. Output: {generated_xml}")
        return True

    except Exception as e:
        # 统一捕获所有来自 pygccxml / castxml 的异常（pygccxml 会把 castxml 输出包含在异常中）
        print("\nFATAL ERROR: CastXML execution failed via pygccxml.")
        print("See log file for details.\n")
        with open(config.CASTXML_LOG_FILE, 'a', encoding='utf-8') as log_f:
            log_f.write(f"\n--- CastXML Execution FAILED ({datetime.now()}) ---\n")
            log_f.write("Exception:\n")
            log_f.write(str(e) + "\n")
            log_f.write("--- End CastXML Log ---\n")
        return False

    finally:
        # 清理临时目录（保留 log 与生成的 XML）
        if tmp_dir and os.path.isdir(tmp_dir):
            try:
                shutil.rmtree(tmp_dir)
            except Exception:
                pass


if __name__ == '__main__':
    # 切到脚本目录，确保相对路径正确
    os.chdir(os.path.dirname(os.path.abspath(__file__)))

    ok = run_castxml()
    if not ok:
        # 失败时返回非零退出码，便于 CI/脚本检测
        sys.exit(1)
    else:
        sys.exit(0)
