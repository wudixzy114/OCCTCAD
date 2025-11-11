#include "Neo4jAdapter.h"
#include <sstream>

std::vector<std::string> Neo4jAdapter::generate_cypher_queries(const IntermediateGraph& graph) {
    std::vector<std::string> queries;

    if (graph.nodes.empty()) {
        return queries;
    }

    // --- 1. 生成创建节点的查询 (使用 UNWIND 批量创建) ---
    std::stringstream create_nodes_query;
    create_nodes_query << "UNWIND $nodes AS node_data\n";
    create_nodes_query << "CREATE (n)\n";
    create_nodes_query << "SET n.temp_id = node_data.temp_id\n";
    create_nodes_query << "SET n += node_data.properties\n";
    create_nodes_query << "CALL apoc.create.addLabels(n, [node_data.label]) YIELD node\n";
    create_nodes_query << "RETURN count(node)";

    queries.push_back(create_nodes_query.str());
    // (这里还需要代码来生成 $nodes 的 JSON 参数)

    // --- 2. 生成创建关系的查询 (使用 UNWIND 批量创建) ---
    std::stringstream create_rels_query;
    create_rels_query << "UNWIND $relationships AS rel_data\n";
    create_rels_query << "MATCH (from_node {temp_id: rel_data.from_id}) \n";
    create_rels_query << "MATCH (to_node {temp_id: rel_data.to_id}) \n";
    create_rels_query << "CALL apoc.create.relationship(from_node, rel_data.name, {}, to_node) YIELD rel\n";
    create_rels_query << "RETURN count(rel)";

    queries.push_back(create_rels_query.str());
    // (这里还需要代码来生成 $relationships 的 JSON 参数)

    // --- 3. (可选) 删除临时 ID ---
    queries.push_back("MATCH (n) WHERE n.temp_id IS NOT NULL REMOVE n.temp_id");

    return queries;
}