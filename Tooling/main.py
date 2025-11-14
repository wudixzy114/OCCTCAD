#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
主生成脚本：整合 CastXML -> 解析 XML -> 生成 ReflectionAccessor.h 与 RTTR_Registration.cpp
使用说明：在 Tooling 目录下运行本脚本（脚本会切换到自身目录）。
"""

import os
import re
import sys
import tempfile
from datetime import datetime
from heuristics import HeuristicProcessor

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
        """内部方法：根据解析结果生成 Accessor.h 和我们自己的 GeneratedReflection.cpp。"""
        print("--- [Step 3/3] Generating C++ Code Files ---")

        # ==========================================================
        #  Accessor.h 生成逻辑 (完全保持不变)
        # ==========================================================

        accessor_lines = [
            '#pragma once',
            '// Generated by Tooling/generate_reflection.py',
            f'// Timestamp: {datetime.now()}',
            '',
            '// --- Forward Declarations for OCCT types ---',
            *[f'class {cls.name};' for cls in self.classes_to_reflect],
            '',
            '// --- Original OCCT Headers ---',
            *[f'#include <{os.path.basename(h)}>' for h in self.headers_to_process],
            '',
            f'class {config.ACCESSOR_CLASS_NAME} {{',
            'public:'
        ]

        for cls in self.classes_to_reflect:
            class_name = cls.name

            # 再次过滤：跳过模板实例与 STL 名称等
            if not class_name or '<' in class_name or class_name.startswith('std::'):
                print(f"Skipping non-class/template/STL type: {class_name}")
                continue

            accessor_lines.append(f'\n    // --- Accessors for {class_name} ---')
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


            except RuntimeError as e:
                print(f"Warning: Could not process variables for class {class_name}: {e}")

        accessor_lines.append('};')

        # ==========================================================
        #  GeneratedReflection.cpp 生成逻辑 (全新)
        # ==========================================================

        all_handler_includes = set()
        generated_handlers = {}  # map cls_name -> { "func_name": ..., "code": ... }

        # --- Part 1: Heuristically generate special handlers ---
        for cls in self.classes_to_reflect:
            processor = HeuristicProcessor(cls)
            handler_code, includes = processor.generate_handler()

            if handler_code:
                handler_name = f"handle_{cls.name}_serialization"
                generated_handlers[cls.name] = {
                    "func_name": handler_name,
                    "code": handler_code
                }
                all_handler_includes.update(includes)

        custom_reflection_lines = [
            '// Generated by Tooling/generate_reflection.py',
            f'// Timestamp: {datetime.now()}',
            '',
            '#include "ManualReflection.h"',
            '#include "ReflectionAccessor.h"',
            '',
            '// We must include the headers to have the full type definitions',
            *[f'#include <{os.path.basename(h)}>' for h in self.headers_to_process],
            '',
            '// Implementation of the initialization function declared in ManualReflection.h',
            'void initialize_reflection() {',
            '    auto& registry = ReflectionRegistry::instance();',
        ]

        # --- Part 2: Write the generated handler functions to the file ---
        for cls_name, handler_info in generated_handlers.items():
            custom_reflection_lines.extend([
                f'// Auto-generated heuristic handler for {cls_name}',
                f'void {handler_info["func_name"]}(const std::any& obj_any, OCCTSerializer& serializer, IntermediateNode& node) {{',
                handler_info["code"],
                '}\n'
            ])

        # --- Part 3: Generate the main initialization function ---
        custom_reflection_lines.extend([
            'void initialize_reflection() {',
            '    auto& registry = ReflectionRegistry::instance();',
        ])

        for cls in self.classes_to_reflect:
            class_name = cls.name
            if not class_name or '<' in class_name or class_name.startswith('std::'):
                continue

            custom_reflection_lines.append(f'\n    // --- Registering {class_name} ---')
            custom_reflection_lines.append(f'    {{')
            custom_reflection_lines.append(f'        TypeDescriptor desc;')
            custom_reflection_lines.append(f'        desc.name = "{class_name}";')
            custom_reflection_lines.append(f'        desc.neo4j_label = "{class_name}";')

            try:
                for var in cls.variables(allow_empty=True):
                    if not getattr(var, "name", None) or var.name.startswith('the') or var.access_type != 'private':
                        continue

                    var_name = var.name
                    prop_name = var_name.lstrip('my') or var_name

                    # 尽可能获取干净的类型名
                    var_type_str_full = var.decl_type.decl_string  # e.g., "const ::gp_XYZ &"
                    var_type_str_clean = declarations.remove_const(var.decl_type).decl_string.replace(" &",
                                                                                                      "")  # e.g., "::gp_XYZ"

                    func_suffix = f"{class_name}_{var_name}"
                    getter_name = f"{config.ACCESSOR_CLASS_NAME}::get_{func_suffix}"
                    setter_name = f"{config.ACCESSOR_CLASS_NAME}::set_{func_suffix}"

                    if class_name.startswith("TopoDS_"):
                        core_type_name = class_name.replace("TopoDS_", "")
                        rel_name = f"HAS_{core_type_name.upper()}"
                        custom_reflection_lines.append(f'        desc.relationship_name_as_child = "{rel_name}";')

                    custom_reflection_lines.append(f'        {{')
                    custom_reflection_lines.append(f'            PropertyDescriptor prop;')
                    custom_reflection_lines.append(f'            prop.name = "{prop_name}";')
                    custom_reflection_lines.append(f'            prop.type_name = "{var_type_str_clean}";')

                    # --- 生成类型擦除的 Getter Lambda ---
                    custom_reflection_lines.append(
                        f'            prop.getter = [] (const std::any& obj_any) -> std::any {{')
                    custom_reflection_lines.append(
                        f'                const auto& obj = std::any_cast<const {class_name}&>(obj_any);')
                    custom_reflection_lines.append(f'                return std::any({getter_name}(obj));')
                    custom_reflection_lines.append(f'            }};')

                    # --- 生成类型擦除的 Setter Lambda ---
                    custom_reflection_lines.append(
                        f'            prop.setter = [] (std::any& obj_any, const std::any& val_any) {{')
                    custom_reflection_lines.append(
                        f'                auto& obj = std::any_cast<{class_name}&>(obj_any);')
                    custom_reflection_lines.append(
                        f'                const auto& val = std::any_cast<const {var_type_str_clean}&>(val_any);')
                    custom_reflection_lines.append(f'                {setter_name}(obj, val);')
                    custom_reflection_lines.append(f'            }};')

                    # --- 关系/属性推断 ---
                    is_rel = False
                    rel_name = ""

                    # 规则 1: 检查是否是 Handle 类型
                    # 匹配 Handle<...>, Handle(...) 和 Handle_...
                    handle_match = re.search(r'Handle[(_<]\s*([_a-zA-Z0-9:]+)\s*[)>]', var_type_str_full, re.IGNORECASE)
                    if not handle_match and 'Handle_' in var_type_str_full:
                        # 尝试匹配 Handle_... 形式
                        handle_match = re.search(r'Handle_([_a-zA-Z0-9:]+)', var_type_str_full)

                    if handle_match:
                        target_type = handle_match.group(1).strip().split("::")[-1]
                        is_rel = True
                        rel_name = f"HAS_{target_type.upper()}"
                        print(
                            f"DEBUG: '{class_name}::{var_name}' (type: {var_type_str_full}) -> MATCHED as Handle. Relationship to: {target_type}")
                    else:
                        # 规则 2: 如果不是 Handle，检查是否是值类型或原生类型
                        try:
                            var_type_base = declarations.remove_declarated(
                                declarations.remove_reference(declarations.remove_const(var.decl_type)))
                            var_type_name_base = getattr(var_type_base, "decl_string",
                                                         str(var_type_base)).strip().lstrip('::')

                            if var_type_name_base in config.VALUE_TYPES:
                                print(
                                    f"DEBUG: '{class_name}::{var_name}' (base type: {var_type_name_base}) -> MATCHED as Value Type.")
                            elif var_type_name_base in config.PRIMITIVE_TYPES:
                                print(
                                    f"DEBUG: '{class_name}::{var_name}' (base type: {var_type_name_base}) -> MATCHED as Primitive Type.")
                            else:
                                print(
                                    f"DEBUG: '{class_name}::{var_name}' (base type: {var_type_name_base}) -> NO MATCH. Defaulting to attribute.")
                            is_rel = False  # 无论是 Value 还是 Primitive 还是 No Match，都不是关系
                        except Exception as e_inner:
                            print(
                                f"DEBUG: Type parsing failed for '{class_name}::{var_name}'. Defaulting to attribute. Error: {e_inner}")
                            is_rel = False

                    # 根据推断结果生成代码
                    if is_rel:
                        custom_reflection_lines.append('            prop.is_relationship = true;')
                        custom_reflection_lines.append(f'            prop.relationship_name = "{rel_name}";')
                    else:
                        custom_reflection_lines.append('            prop.is_relationship = false;')

                    custom_reflection_lines.append(f'            desc.properties["{prop_name}"] = std::move(prop);')
                    custom_reflection_lines.append(f'        }}')

            except RuntimeError as e:
                print(f"Warning: Could not process variables for class {class_name} for Custom Reflection: {e}")

            custom_reflection_lines.append(f'        registry.register_type(std::move(desc));')
            custom_reflection_lines.append(f'    }}')

        custom_reflection_lines.append('}')  # 结束 initialize_reflection 函数

        # 写入文件
        try:
            with open(config.ACCESSOR_H_FILE, 'w', encoding='utf-8') as f:
                f.write('\n'.join(accessor_lines))
            print(f"Successfully generated {config.ACCESSOR_H_FILE}")

            with open(config.CUSTOM_REFLECTION_CPP_FILE, 'w', encoding='utf-8') as f:
                f.write('\n'.join(custom_reflection_lines))
            print(f"Successfully generated {config.CUSTOM_REFLECTION_CPP_FILE}")
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
