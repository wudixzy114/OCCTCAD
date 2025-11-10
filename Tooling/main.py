#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
主生成脚本：整合 CastXML -> 解析 XML -> 生成 ReflectionAccessor.h 与 RTTR_Registration.cpp
使用说明：在 Tooling 目录下运行本脚本（脚本会切换到自身目录）。
"""

import os
import sys
import tempfile
from datetime import datetime

from pygccxml import utils, parser, declarations

import config


def read_header_list():
    """从 class_list.txt 读取头文件列表（去注释、空行，规范化分隔符）。"""
    headers = []
    if not os.path.exists(config.CLASS_LIST_FILE):
        raise FileNotFoundError(f"Class list file not found: {config.CLASS_LIST_FILE}")
    with open(config.CLASS_LIST_FILE, 'r', encoding='utf-8') as f:
        for line in f:
            line = line.strip()
            if line and not line.startswith('#'):
                headers.append(line.replace('/', os.sep).replace('\\', os.sep))
    return headers


def find_classes_in_xml(global_ns, headers_to_process):
    """
    从 XML 的全局命名空间中，根据头文件列表自动发现所有定义的类。
    规则：
      - 只匹配类定义所在文件（比较 cls.location.file_name 与 headers_to_process 的镜像路径）
      - 跳过模板实例（名字包含 '<'）、以及 std:: 等 STL / helper 类型
    """
    found_classes = []
    target_headers = {os.path.normpath(os.path.join(config.MIRRORED_DIR, h)) for h in headers_to_process}

    for cls in global_ns.classes():
        # 跳过模板实例和 STL 命名空间的类型
        if not cls.name or '<' in cls.name or cls.name.startswith('std::'):
            continue

        # 有些 declaration 可能没有 location（内置/编译器生成等），跳过
        if not cls.location or not getattr(cls.location, "file_name", None):
            continue

        cls_file_path = os.path.normpath(cls.location.file_name)
        if cls_file_path in target_headers:
            found_classes.append(cls)

    return found_classes


class ReflectionGenerator:
    """
    一个内聚的类，封装了从 XML 解析到代码生成的完整流程。
    """

    def __init__(self):
        self.headers_to_process = read_header_list()
        self.global_ns = None
        self.classes_to_reflect = []
        self.class_name_set = set()
        self.xml_config = None

    def _set_xml_config(self):
        try:
            generator_path, generator_name = utils.find_xml_generator()
            # 使用找到的 generator_name / path，并传入 compiler_path
            xml_config_ = parser.xml_generator_configuration_t(
                xml_generator=generator_name,
                xml_generator_path=generator_path,
                include_paths=[config.MIRRORED_DIR, config.OCCT_INC_PATH],
                compiler_path=config.COMPILER_PATH,
                cflags="-std=c++17 -DHAVE_RTTI -DHAVE_CPP11 -DOCC_CONVERT_SIGNALS"
            )
            self.xml_config = xml_config_
            return True
        except Exception as e:
            print(f"\nFATAL ERROR: Get xml config failed: {e}")
            return False

    def _run_castxml(self):
        """内部方法：执行 CastXML 生成 XML 文件（通过组合头文件）。"""
        print("--- [Step 1/3] Running CastXML ---")
        os.makedirs(config.GENERATED_DIR, exist_ok=True)
        if not self.headers_to_process:
            print("Warning: No headers in class_list.txt. Skipping CastXML.")
            return True

        mirrored_paths = [os.path.join(config.MIRRORED_DIR, h) for h in self.headers_to_process]
        missing = [p for p in mirrored_paths if not os.path.exists(p)]
        if missing:
            print("\nFATAL ERROR: Mirrored header files not found:")
            for m in missing:
                print("  -", m)
            return False

        try:
            with tempfile.TemporaryDirectory(prefix="pygccxml_") as tmp_dir:
                combined_header = os.path.join(tmp_dir, "combined.hpp")
                # 将所有头文件绝对路径以 #include "..." 写入临时组合头
                with open(combined_header, 'w', encoding='utf-8') as f:
                    f.write("// Auto-generated combined header for pygccxml / castxml\n")
                    for p in mirrored_paths:
                        f.write(f'#include "{os.path.abspath(str(p))}"\n')

                print(f"Parsing {len(mirrored_paths)} headers via combined header...")
                # 使用 source_reader_t 的 create_xml_file 将会把解析输出到 config.XML_FILE_PATH
                src_reader = parser.source_reader_t(self.xml_config)
                # create_xml_file(signature) -> (source, destination)
                src_reader.create_xml_file(combined_header, config.XML_FILE_PATH)

            print(f"CastXML successful. Output: {config.XML_FILE_PATH}")
            return True
        except Exception as e:
            print(f"\nFATAL ERROR: CastXML execution failed: {e}")
            # 追加回溯信息也可以写到日志文件（config.CASTXML_LOG_FILE）以便调试
            return False

    def _parse_xml_and_discover_classes(self):
        """内部方法：解析 XML 并发现类。"""
        print("--- [Step 2/3] Parsing XML and Discovering Classes ---")
        if not os.path.exists(config.XML_FILE_PATH):
            print("Error: XML file not found. Cannot proceed.")
            return False

        try:
            # parse_xml_file( xml_file, xml_config )
            decls = parser.parse_xml_file(config.XML_FILE_PATH, self.xml_config)
            self.global_ns = declarations.get_global_namespace(decls)
            self.classes_to_reflect = find_classes_in_xml(self.global_ns, self.headers_to_process)
            self.class_name_set = {cls.name for cls in self.classes_to_reflect}
            print(f"Discovered {len(self.classes_to_reflect)} classes to reflect.")
            return True
        except Exception as e:
            print(f"Error parsing XML file: {e}")
            return False

    def _generate_code_files(self):
        """内部方法：根据解析结果生成 Accessor 和 RTTR 文件。"""
        print("--- [Step 3/3] Generating C++ Code Files ---")

        accessor_lines = [
            '#pragma once',
            '// Generated by Tooling/generate_reflection.py',
            f'// Timestamp: {datetime.now()}',
            '',
            '#include <rttr/type>',
            '// --- Original OCCT Headers ---',
            *[f'#include <{os.path.basename(h)}>' for h in self.headers_to_process],
            '',
            f'class {config.ACCESSOR_CLASS_NAME} {{',
            'public:'
        ]

        rttr_lines = [
            '// Generated by Tooling/generate_reflection.py',
            f'// Timestamp: {datetime.now()}',
            '',
            '#include "ReflectionAccessor.h"',
            '#include <rttr/registration.h>',
            '',
            'RTTR_REGISTRATION {',
        ]

        for cls in self.classes_to_reflect:
            class_name = cls.name

            # 再次过滤：跳过模板实例与 STL 名称等
            if not class_name or '<' in class_name or class_name.startswith('std::'):
                print(f"Skipping non-class/template/STL type: {class_name}")
                continue

            accessor_lines.append(f'\n    // --- Accessors for {class_name} ---')
            rttr_lines.append(f'\n    // --- Registration for {class_name} ---')
            rttr_lines.append(f'    rttr::registration::class_<{class_name}>("{class_name}")')
            rttr_lines.append(f'        (rttr::metadata("neo4j_label", "{class_name}"))')

            try:
                for var in cls.variables(allow_empty=True):
                    # guard: 有些 declaration 可能没有 name
                    if not getattr(var, "name", None):
                        continue

                    # 过滤掉以 the 开头或非 private 成员（按你的原逻辑）
                    if var.name.startswith('the') or var.access_type != 'private':
                        continue

                    # 成员类型与名称
                    var_type_str = var.decl_type.decl_string
                    var_name = var.name
                    func_suffix = f"{class_name}_{var_name}"
                    prop_name = var_name.lstrip('my') or var_name

                    # 生成 Accessor
                    accessor_lines.append(
                        f"    static const {var_type_str}& get_{func_suffix}(const {class_name}& obj) {{ return obj.{var_name}; }}")
                    accessor_lines.append(
                        f"    static void set_{func_suffix}({class_name}& obj, const {var_type_str}& value) {{ obj.{var_name} = value; }}")

                    # 生成 RTTR property 语句（引用到 Accessor）
                    accessors = f"&{config.ACCESSOR_CLASS_NAME}::get_{func_suffix}, &{config.ACCESSOR_CLASS_NAME}::set_{func_suffix}"
                    prop_line = f'        .property("{prop_name}", {accessors})'

                    # 关系推断：去除 const/ref/typedef，取裸名并匹配 class_name_set
                    try:
                        var_type = var.decl_type
                        var_type = declarations.remove_const(var_type)
                        var_type = declarations.remove_reference(var_type)
                        var_type = declarations.remove_declarated(var_type)
                        # decl_string 可能包含命名空间 -> 取最后一段做简单匹配
                        var_type_name = getattr(var_type, "decl_string", str(var_type)).split("::")[-1]
                        if var_type_name in self.class_name_set:
                            rel_name = f"HAS_{var_type_name.upper()}"
                            prop_line += f'\n            (rttr::metadata("neo4j_relationship", "{rel_name}"))'
                    except Exception:
                        # 解析类型失败，不影响整个生成流程
                        pass

                    rttr_lines.append(prop_line)

            except RuntimeError as e:
                print(f"Warning: Could not process variables for class {class_name}: {e}")

            rttr_lines.append('    ;')  # 结束该 class_ 的注册

        accessor_lines.append('};')
        rttr_lines.append('}')  # 结束 RTTR_REGISTRATION

        # 写入文件
        try:
            with open(config.ACCESSOR_H_FILE, 'w', encoding='utf-8') as f:
                f.write('\n'.join(accessor_lines))
            print(f"Successfully generated {config.ACCESSOR_H_FILE}")

            with open(config.RTTR_CPP_FILE, 'w', encoding='utf-8') as f:
                f.write('\n'.join(rttr_lines))
            print(f"Successfully generated {config.RTTR_CPP_FILE}")
            return True
        except Exception as e:
            print(f"Error writing generated files: {e}")
            return False

    def run(self):
        """执行完整的生成流程。"""
        if not self._set_xml_config():
            return False
        if not self._run_castxml():
            return False
        if not self._parse_xml_and_discover_classes():
            return False
        if not self._generate_code_files():
            return False
        return True


if __name__ == '__main__':
    os.chdir(os.path.dirname(os.path.abspath(__file__)))
    generator = ReflectionGenerator()
    if not generator.run():
        sys.exit(1)
