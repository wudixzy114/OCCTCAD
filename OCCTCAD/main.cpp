#include "ManualReflection.h"
#include "OCCTSerializer.h"
#include "Neo4jAdapter.h"
#include <BRepPrimAPI_MakeBox.hxx>
#include <TopoDS_Shape.hxx>
#include <iostream>

int main() {
    // 1. 初始化反射
    initialize_reflection();

    // 2. 创建一个简单的立方体 (Solid -> Shell -> 6 Faces -> ... -> Vertices)
    TopoDS_Shape box = BRepPrimAPI_MakeBox(10.0, 20.0, 30.0).Shape();

    // 3. 序列化
    OCCTSerializer serializer(ReflectionRegistry::instance());
    IntermediateGraph graph = serializer.serialize(box);

    // 4. 输出统计信息
    std::cout << "Serialization Complete!" << std::endl;
    std::cout << "Nodes created: " << graph.nodes.size() << std::endl;
    std::cout << "Relationships created: " << graph.relationships.size() << std::endl;

    // 5. 生成 Cypher (可选，用于调试)
    Neo4jAdapter adapter;
    std::vector<std::string> queries = adapter.generate_cypher_queries(graph);
    std::cout << "\nGenerated Cypher Query (Preview):" << std::endl;
    if (!queries.empty()) {
        std::cout << queries[0].substr(0, 500) << "..." << std::endl;
    }

    return 0;
}