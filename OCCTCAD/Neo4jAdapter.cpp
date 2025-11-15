#include "Neo4jAdapter.h"
#include <cpr/cpr.h>
#include <iostream>
#include <base64.h>

Neo4jAdapter::Neo4jAdapter(Config config) : m_config(config) {
	m_transaction_endpoint = "http://" + m_config.host + ":" + std::to_string(m_config.port) + "/db/" + m_config.database + "/tx/commit";
	std::string auth_string = m_config.username + ":" + m_config.password;
	m_auth_header = "Basic " + base64_encode(auth_string);
}

void Neo4jAdapter::flatten_properties(const std::string& base_name, const std::any& value, json& output_props)
{
	if (!value.has_value()) {
		return;
	}
	const auto& type = value.type();
	if (type == typeid(std::unordered_map<std::string, std::any>)) {
		const auto& map = std::any_cast<const std::unordered_map<std::string, std::any>&>(value);
		for (const auto& [key, sub_val] : map) {
			// Recursively flatten, e.g., "location" + "_" + "x" -> "location_x"
			flatten_properties(base_name + "_" + key, sub_val, output_props);
		}
	}
	else {
		json primitive_json = any_to_json(value);
		if (!primitive_json.is_null()) {
			output_props[base_name] = primitive_json;
		}
	}
}

json Neo4jAdapter::any_to_json(const std::any& value) {
	if (!value.has_value()) {
		return nullptr;
	}
	const auto& type = value.type();

	if (type == typeid(bool)) return std::any_cast<bool>(value);
	if (type == typeid(int64_t)) return std::any_cast<int64_t>(value);
	if (type == typeid(int32_t)) return std::any_cast<int32_t>(value);
	if (type == typeid(double)) return std::any_cast<double>(value);
	if (type == typeid(std::string)) return std::any_cast<const std::string&>(value);

	if (type == typeid(std::unordered_map<std::string, std::any>)) {
		json obj = json::object();
		const auto& map = std::any_cast<const std::unordered_map<std::string, std::any>&>(value);
		for (const auto& [key, val] : map) {
			obj[key] = any_to_json(val);
		}
		return obj;
	}

	std::cerr << "Warning: Unhandled type in any_to_json: " << type.name() << std::endl;
	return nullptr;
}

json Neo4jAdapter::build_cypher_payload(const IntermediateGraph& graph) {
	// --- 1. Prepare parameters for nodes ---
	json nodes_param = json::array();
	for (const auto& node : graph.nodes) {
		json node_data = json::object();
		node_data["temp_id"] = node.temp_id;
		node_data["label"] = node.neo4j_label;

		json props = json::object();
		for (const auto& [key, value] : node.properties) {
			flatten_properties(key, value, props);
		}
		node_data["properties"] = props;
		nodes_param.push_back(node_data);
	}

	// --- 2. Prepare parameters for relationships ---
	json rels_param = json::array();
	for (const auto& rel : graph.relationships) {
		json rel_data = json::object();
		rel_data["from_id"] = rel.from_node_id;
		rel_data["to_id"] = rel.to_node_id;
		rel_data["name"] = rel.relationship_name;
		rels_param.push_back(rel_data);
	}

	// --- 3. Construct the final payload with multiple statements ---
	json payload = {
		{"statements", json::array()}
	};

	// Statement 1: Create nodes
	payload["statements"].push_back({
		{"statement", R"(
            UNWIND $nodes AS node_data
            CREATE (n)
            SET n = node_data.properties
            SET n.temp_id = node_data.temp_id
			WITH n, node_data 
            CALL apoc.create.addLabels(n, [node_data.label]) YIELD node
            RETURN count(node) AS created_nodes
        )"},
		{"parameters", {{"nodes", nodes_param}}}
		});

	// Statement 2: Create relationships
	if (!rels_param.empty()) {
		payload["statements"].push_back({
			{"statement", R"(
                UNWIND $relationships AS rel_data
                MATCH (from_node {temp_id: rel_data.from_id})
                MATCH (to_node {temp_id: rel_data.to_id})
                CALL apoc.create.relationship(from_node, rel_data.name, {}, to_node) YIELD rel
                RETURN count(rel) AS created_rels
            )"},
			{"parameters", {{"relationships", rels_param}}}
			});
	}

	// Statement 3: Remove temporary IDs
	payload["statements"].push_back({
	   {"statement", R"(
            MATCH (n) WHERE n.temp_id IS NOT NULL
            REMOVE n.temp_id
        )"} // Optimization: Use an index on temp_id for faster matching
		});

	return payload;
}

bool Neo4jAdapter::execute_transaction(const json& payload) {

	// ================== DEBUG BLOCK: START ==================
	std::string payload_str = payload.dump(2); // Dump with indentation for readability
	std::cout << "\n--- Neo4j HTTP Request ---" << std::endl;
	std::cout << "Endpoint: " << m_transaction_endpoint << std::endl;
	std::cout << "Authorization: " << m_auth_header << std::endl;
	// Be careful printing payloads in production, but it's essential for debugging
	if (payload_str.length() < 2000) { // Limit payload printing
		std::cout << "Payload:\n" << payload_str << std::endl;
	}
	else {
		std::cout << "Payload: (Too large to print, size: " << payload_str.length() << " bytes)" << std::endl;
	}
	std::cout << "--------------------------" << std::endl;
	// ================== DEBUG BLOCK: END ====================

	cpr::Response r = cpr::Post(
		cpr::Url{ m_transaction_endpoint },
		cpr::Header{
			{"Authorization", m_auth_header},
			{"Content-Type", "application/json"},
			{"Accept", "application/json; charset=UTF-8"}
		},
		cpr::Body{ payload.dump() },
		cpr::Proxies{}
	);

	// ================== DEBUG BLOCK: START ==================
	std::cout << "\n--- Neo4j HTTP Response ---" << std::endl;
	std::cout << "Status Code: " << r.status_code << std::endl;
	std::cout << "Headers: " << r.header["content-type"] << std::endl;
	std::cout << "Response Text:\n" << r.text << std::endl;
	std::cout << "Error Message: " << r.error.message << std::endl;
	std::cout << "---------------------------" << std::endl;
	// ================== DEBUG BLOCK: END ====================


	if (r.status_code >= 400) {
		std::cerr << "Neo4j transaction failed. Status: " << r.status_code << std::endl;
		std::cerr << "Error: " << r.text << std::endl;
		// You can parse r.text (which is JSON) for more detailed error info
		return false;
	}

	try {
		json result = json::parse(r.text);
		if (result.contains("errors") && !result["errors"].empty()) {
			std::cerr << "Neo4j transaction failed with errors in response:" << std::endl;
			std::cerr << result["errors"].dump(2) << std::endl;
			return false;
		}
	}
	catch (const json::parse_error& e) {
		std::cerr << "Failed to parse Neo4j response: " << e.what() << std::endl;
		return false;
	}

	std::cout << "Neo4j transaction successful." << std::endl;
	return true;
}

bool Neo4jAdapter::save_graph(const IntermediateGraph& graph) {
	if (graph.nodes.empty()) {
		return true;
	}
	json payload = build_cypher_payload(graph);
	return execute_transaction(payload);
}