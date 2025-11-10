import os
import re
import shutil
from datetime import datetime
import config

# --- 配置 ---
# 请确保此路径指向您的 OCCT 头文件根目录
OCCT_INC_PATH = config.OCCT_INC_PATH
CLASS_LIST_FILE = config.CLASS_LIST_FILE

# 确保 MIRRORED_DIR 使用脚本的绝对路径来计算相对位置
MIRRORED_DIR = config.MIRRORED_DIR

ACCESSOR_CLASS_NAME = config.ACCESSOR_CLASS_NAME
FRIEND_DECLARATION = f"friend class {ACCESSOR_CLASS_NAME};"
FORWARD_DECLARATION = f"class {ACCESSOR_CLASS_NAME};"
INDENTATION = '    '
LOG_FILE = config.MIRROR_LOG_FILE


def strip_comments(text):
    """
    用等长的空格替换C/C++注释内容，保留换行符，避免注释干扰正则匹配。
    """

    def replacer(match):
        s = match.group(0)
        # 用空格替换所有非换行符的字符
        return re.sub('[^\n]', ' ', s)

    pattern = re.compile(r'//.*?$|/\*.*?\*/', re.DOTALL | re.MULTILINE)
    return re.sub(pattern, replacer, text)


def safe_mkdir(path):
    """创建目录及其所有父目录，如果已存在则忽略。"""
    if not os.path.exists(path):
        os.makedirs(path)


def process_header_injection(source_path, target_path, log_file_handle):
    """
    处理单个文件：复制到目标路径，并注入前向声明和 friend 声明。
    """
    # 1. 复制文件到目标位置
    safe_mkdir(os.path.dirname(target_path))
    try:
        shutil.copy2(source_path, target_path)
    except Exception as e:
        log_file_handle.write(f"!!! ERROR copying file {source_path} to {target_path}: {e}\n\n")
        return False

    # 2. 读取副本内容
    try:
        with open(target_path, 'r', encoding='utf-8', errors='ignore') as f:
            original_content = f.read()
    except Exception as e:
        log_file_handle.write(f"!!! ERROR reading target file {target_path}: {e}\n\n")
        return False

    sanitized_content = strip_comments(original_content)
    made_changes = False
    log_entries = []
    modification_events = []  # 存储 (position, type, text, final_line_num)

    # --- 步骤 1: 确定是否需要以及在哪里添加前向声明 ---
    forward_decl_needed = not re.search(fr'class\s+{ACCESSOR_CLASS_NAME}\s*;', sanitized_content)

    if forward_decl_needed:
        # 查找第一个类/结构体的开始位置，作为搜索 #include 的上限
        first_class_match = re.search(r'^\s*(?:class|struct)\s+', sanitized_content, re.MULTILINE)
        search_limit = first_class_match.start() if first_class_match else len(original_content)

        last_include_match = None
        # 只在文件头部区域（第一个类定义之前）查找最后一个 #include
        for match in re.finditer(r'^\s*#include\s*["<].*[">]', original_content[:search_limit], re.MULTILINE):
            last_include_match = match

        if last_include_match:
            # 插入点在最后一个 #include 之后
            insert_pos = last_include_match.end()
            text_to_insert = f"\n\n{FORWARD_DECLARATION}\n"
        else:
            # 查找 Header Guard 或 #pragma once 之后
            header_guard_match = re.search(
                r'^\s*(?:#ifndef\s+[\w_]+\s*\n\s*#define\s+[\w_]+|#pragma\s+once).*',
                original_content, re.MULTILINE)
            if header_guard_match:
                insert_pos = header_guard_match.end()
                text_to_insert = f"\n\n{FORWARD_DECLARATION}\n"
            else:
                # 极端情况：文件头部
                insert_pos = 0
                text_to_insert = f"{FORWARD_DECLARATION}\n\n"

        # --- 精确行号计算 (Forward Declaration) ---
        anchor_line_num = original_content.count('\n', 0, insert_pos) + 1

        # 计算插入文本中，FORWARD_DECLARATION 之前有多少个换行符
        decl_index = text_to_insert.find(FORWARD_DECLARATION)
        newlines_offset = text_to_insert[:decl_index].count('\n') if decl_index != -1 else 0

        final_line_num = anchor_line_num + newlines_offset
        # ---------------------------------------------

        log_entries.append(f"  + Added forward declaration: '{FORWARD_DECLARATION}' near line {final_line_num}")
        modification_events.append((insert_pos, 'FORWARD_DECL', text_to_insert, final_line_num))
        made_changes = True

    # --- 步骤 2: 查找所有需要添加 friend 的类 ---
    class_pattern = re.compile(r'(^\s*(?:class|struct)\s+(?:[\w_]+\s+)*([\w_]+)\s*(?::[^{]*)?\{)', re.MULTILINE)

    # 插入文本：保证 public 访问，并确保独占新行
    insertion_text = f"\npublic:\n{INDENTATION}{FRIEND_DECLARATION}\n"

    class_matches = list(class_pattern.finditer(sanitized_content))

    for i, match in enumerate(class_matches):
        class_name = match.group(2)
        start, end = match.span(0)  # end is position right after '{'

        scope_start = end
        scope_end = class_matches[i + 1].start() if i + 1 < len(class_matches) else len(sanitized_content)

        if FRIEND_DECLARATION not in sanitized_content[scope_start:scope_end]:
            # --- 精确行号计算 (Friend Declaration) ---
            # 锚点行号是 '{' 之后的行号 (即 match.end() 的位置)
            anchor_line_num = original_content.count('\n', 0, end) + 1

            # 计算插入文本中，FRIEND_DECLARATION 之前有多少个换行符
            decl_index = insertion_text.find(FRIEND_DECLARATION)
            newlines_offset = insertion_text[:decl_index].count('\n') if decl_index != -1 else 0

            final_line_num = anchor_line_num + newlines_offset
            # ---------------------------------------------

            modification_events.append((end, 'FRIEND_DECL', insertion_text, final_line_num))

            log_entries.append(f"  - Added friend to class '{class_name}' near line {final_line_num}")
            made_changes = True

    # --- 步骤 3, 4, 5: 构建最终文件内容，写入文件和日志 ---
    if not made_changes:
        return False

    # 排序确保修改顺序正确
    modification_events.sort(key=lambda x: x[0])

    new_content_parts = []
    last_pos = 0
    for pos, type_, text, _ in modification_events:
        new_content_parts.append(original_content[last_pos:pos])
        new_content_parts.append(text)
        last_pos = pos
    new_content_parts.append(original_content[last_pos:])

    final_content = "".join(new_content_parts)

    try:
        with open(target_path, 'w', encoding='utf-8') as f:
            f.write(final_content)

        log_file_handle.write(f"--- MODIFIED FILE: {target_path} ---\n")

        # 按照日志中的行号进行排序，使输出更清晰
        log_entries.sort(key=lambda x: int(re.search(r'line (\d+)', x).group(1)))

        for entry in log_entries:
            log_file_handle.write(f"{entry}\n")
        log_file_handle.write("\n")
        return True
    except Exception as e:
        log_file_handle.write(f"!!! ERROR writing to file {target_path}: {e}\n\n")
        return False


def mirror_occt_headers():
    """主函数：处理类清单并执行注入。"""

    # 确保我们从脚本位置正确计算相对路径
    script_dir = os.path.dirname(os.path.abspath(__file__))

    print(f"Starting header injection into {MIRRORED_DIR}...")
    safe_mkdir(MIRRORED_DIR)

    total_files = 0
    modified_files = 0

    try:
        log_dir = os.path.dirname(LOG_FILE)

        if log_dir:
            safe_mkdir(log_dir)

        with open(LOG_FILE, 'w', encoding='utf-8') as log_f:
            log_f.write("=" * 80 + "\n")
            log_f.write(f"HEADER MIRRORING LOG: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
            log_f.write(f"Source OCCT Path: {OCCT_INC_PATH}\n")
            log_f.write(f"Target Mirror Path: {MIRRORED_DIR}\n")
            log_f.write("=" * 80 + "\n\n")

            list_path = os.path.join(script_dir, CLASS_LIST_FILE)
            if not os.path.exists(list_path):
                print(f"Error: Class list file not found at {list_path}")
                log_f.write(f"Error: Class list file not found at {list_path}\n")
                return

            with open(list_path, 'r') as f:
                for line in f:
                    line = line.strip()
                    if not line or line.startswith('#'):
                        continue

                    header_rel_path = line

                    # 规范化路径
                    header_rel_path = header_rel_path.replace('/', os.sep)
                    header_rel_path = header_rel_path.replace('\\', os.sep)

                    source_path = os.path.join(OCCT_INC_PATH, header_rel_path)
                    target_path = os.path.join(MIRRORED_DIR, header_rel_path)

                    if not os.path.exists(source_path):
                        log_f.write(f"Warning: Source header not found: {source_path}\n")
                        continue

                    total_files += 1
                    if process_header_injection(source_path, target_path, log_f):
                        modified_files += 1

            summary = f"\nInjection complete. {modified_files}/{total_files} headers were modified and mirrored.\n"
            print(summary)
            log_f.write(summary)

    except Exception as e:
        print(f"\nFATAL ERROR during mirroring process: {e}")
        # 写入日志，以便追踪致命错误
        try:
            with open(LOG_FILE, 'a', encoding='utf-8') as log_f:
                log_f.write(f"\nFATAL ERROR: {e}\n")
        except:
            pass


if __name__ == '__main__':
    mirror_occt_headers()
