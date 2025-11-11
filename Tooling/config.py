import os

# --- 路径配置 ---
# 脚本所在目录
TOOLING_DIR = os.path.dirname(os.path.abspath(__file__))
# 项目根目录
PROJECT_DIR = os.path.normpath(os.path.join(TOOLING_DIR, '..'))

# OCCT 安装路径
OCCT_INC_PATH = r'D:\OCCT\opencascade-7.9.2-vc14-64\inc'

# --- 输入文件 ---
CLASS_LIST_FILE = os.path.join(TOOLING_DIR, 'class_list.txt')

# --- 中间产物目录 ---
MIRRORED_DIR = os.path.join(PROJECT_DIR, 'MirroredHeaders')
GENERATED_DIR = os.path.join(PROJECT_DIR, 'Generated')
XML_FILE_PATH = os.path.join(GENERATED_DIR, 'occt_api.xml')

# --- 输出文件 ---
SOURCE_DIR = os.path.join(PROJECT_DIR, 'OCCTCAD')
ACCESSOR_H_FILE = os.path.join(SOURCE_DIR, 'ReflectionAccessor.h')
CUSTOM_REFLECTION_CPP_FILE = os.path.join(SOURCE_DIR, 'GeneratedReflection.cpp')

# --- 反射配置 ---
ACCESSOR_CLASS_NAME = 'ReflectionAccessor'

# --- 日志配置 ---
CASTXML_LOG_FILE = os.path.join(TOOLING_DIR, 'log', 'generation_log.txt')
MIRROR_LOG_FILE = os.path.join(TOOLING_DIR, 'log', 'mirror_log.txt')

COMPILER_PATH = r"C:/msys64/ucrt64/bin/g++.exe"
