# OCCTCAD

> **基于 OpenCascade 的 CAD → Neo4j 实验。**
> 与 [CAD_DB](../CAD_DB) 配套的轻量版本，专注 OCCT 路径。

## 项目定位

CAD_DB 项目同时支持 ACIS 和 OCCT 两条几何内核路径，但两者 API 差异很大。
OCCTCAD 是 OCCT 路径的独立实验：

- 用更现代的 C++ 包装（OCCT 7.x）
- 探索 `Generated/` 目录的自动生成反射代码用法
- 与 Neo4j 4.x 适配器的接口简化

## 仓库结构

```
OCCTCAD/
├─ OCCTCAD/
│  ├─ OCCTCAD.vcxproj         # Visual Studio 项目
│  ├─ main.cpp                # 程序入口
│  ├─ OCCTSerializer.cpp      # OCCT 几何 → JSON/图节点
│  ├─ OCCTValueConverter.cpp  # OCCT 类型 → 通用值
│  ├─ Neo4jAdapter.cpp        # Neo4j 客户端封装
│  ├─ ReflectionAccessor.h    # OCCT 反射访问（用 Generated/）
│  ├─ GeneratedReflection.cpp # 自动生成的反射样板
│  ├─ ManualReflection.h      # 手工补充的反射
│  └─ test_*.cpp              # 单元测试
├─ external/                  # OCCT 第三方依赖
├─ Generated/                 # OCCT wrapper 生成的代码
├─ MirroredHeaders/           # 镜像的 OCCT 头文件
├─ Tooling/                   # 工具脚本
├─ docker/                    # Docker 构建
├─ test/                      # 集成测试
└─ OCCTCAD.slnx               # 解决方案（新格式）
```

## 技术栈

- **语言**：C++17
- **CAD 内核**：OpenCascade 7.x
- **图数据库**：Neo4j 4.x（C++ 驱动）
- **构建**：Visual Studio (slnx) + Docker
- **平台**：Windows / Linux（Docker）

## 关键模块

| 模块 | 作用 |
|---|---|
| `OCCTSerializer` | 把 `TopoDS_Shape` 序列化为图节点 / JSON |
| `OCCTValueConverter` | OCCT 类型（`gp_Pnt` / `TopoDS_Edge` 等）→ 通用值 |
| `Neo4jAdapter` | Neo4j 连接 + Cypher 异步执行 |
| `ReflectionAccessor` | 用 `Generated/` 的反射代码访问 OCCT 内部 |
| `GeneratedReflection` | 自动生成的反射样板（基于 OCCT 头文件） |
| `ManualReflection` | 手工补充的反射（处理自动生成覆盖不到的情况） |

## 已完成

- ✅ OCCT STEP / IGES 读取
- ✅ `TopoDS_Shape` → Neo4j 节点
- ✅ 基础拓扑查询（共用面、邻接）
- ✅ Docker 化构建

## 进行中

- ⏳ 反射生成器的稳定化
- ⏳ 与 CAD_DB 后端的协议统一

## 本地构建

```bash
# 方式 1：Visual Studio
# 打开 OCCTCAD.slnx

# 方式 2：Docker
cd docker
docker build -t occtcad .
```

## 配套

- 主项目：[CAD_DB](../CAD_DB)
- 早期原型：[DBCAD](../DBCAD)

## License

项目代码 MIT；OCCT 遵守其原始 LGPL 2.1 license。
