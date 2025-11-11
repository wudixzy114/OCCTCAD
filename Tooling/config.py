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

# --- Phase 1: Type Classification ---
# Types in this set will be serialized as complex properties (e.g., JSON objects)
# on their parent node, rather than becoming separate nodes in the graph.
# 凡是在此集合中的类型，都会被序列化为其父节点的复杂属性，而不是独立的图节点。
VALUE_TYPES = {
    "gp_Pnt",
    "gp_Dir",
    "gp_Vec",
    "gp_Ax1",
    "gp_Ax2",
    "gp_Ax3",
    "gp_XYZ",
    "gp_Trsf",
    "gp_Quaternion",
    "TopLoc_Location"  # TopoDS_Shape::myLocation 的实际类型
}

# Simple C++ types that can be directly stored in the database.
# 简单 C++ 类型，可以直接存入数据库。
PRIMITIVE_TYPES = {
    "Standard_Real",  # a.k.a. double
    "Standard_Integer",  # a.k.a. int
    "Standard_Boolean",  # a.k.a. bool
    "TopAbs_Orientation",
    "TopAbs_ShapeEnum"
}
