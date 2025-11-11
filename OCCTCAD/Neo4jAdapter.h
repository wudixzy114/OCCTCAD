#pragma once
#include "OCCTSerializer.h"
#include <vector>
#include <string>

class Neo4jAdapter {
public:
    // 将中间图转换为 Cypher 查询语句
    std::vector<std::string> generate_cypher_queries(const IntermediateGraph& graph);
};
