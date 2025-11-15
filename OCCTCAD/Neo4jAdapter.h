#pragma once
#include "OCCTSerializer.h"
#include <string>	
#include <vector>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class Neo4jAdapter {
public:
	struct Config {
		std::string host = "localhost";
		int port = 7474;
		std::string username = "neo4j";
		std::string password = "password";
		std::string database = "neo4j";
	};

	explicit Neo4jAdapter(Config config);

	bool save_graph(const IntermediateGraph& graph);

	json build_cypher_payload(const IntermediateGraph& graph);

private:

	void flatten_properties(
		const std::string& base_name,
		const std::any& value,
		json& output_props
	);

	json any_to_json(const std::any& value);

	bool execute_transaction(const json& payload);

	Config m_config;
	std::string m_transaction_endpoint;
	std::string m_auth_header;
};