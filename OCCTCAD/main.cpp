#include "OCCTSerializer.h"
#include "Neo4jAdapter.h"
#include <BRepPrimAPI_MakeBox.hxx> // 创建一个 OCCT 对象
#include <TopoDS_Shape.hxx>

int main() {
    // 1. 初始化反射系统 (必须)
    initialize_reflection();

    // 2. 创建一个 OCCT 对象 (例如一个立方体)
    TopoDS_Shape box_shape = BRepPrimAPI_MakeBox(10, 20, 30).Shape();

    // 3. 实例化序列化器
    OCCTSerializer serializer(ReflectionRegistry::instance());

    // 4. 序列化对象
    IntermediateGraph graph = serializer.serialize(box_shape);

    // 5. 生成数据库查询
    Neo4jAdapter adapter;
    std::vector<std::string> queries = adapter.generate_cypher_queries(graph);

    // 6. 执行查询 (伪代码)
    // for (const auto& query : queries) {
    //     neo4j_connection.run(query, parameters);
    // }

    for (const auto& r : queries) {
        std::cout << r << std::endl;
    }

    return 0;
}