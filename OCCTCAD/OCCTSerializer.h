#pragma once

#include "ManualReflection.h"
#include <Standard_Transient.hxx>
#include <Standard_Handle.hxx>
#include <any>
#include <string>
#include <vector>
#include <unordered_map>
#include <TopoDS_Shape.hxx>

struct IntermediateNode {
	int64_t temp_id;
	std::string type_name;
	std::string neo4j_label;
	std::unordered_map<std::string, std::any> properties;
};

struct IntermediateRelationship {
	int64_t from_node_id;
	int64_t to_node_id;
	std::string relationship_name;
};

struct IntermediateGraph {
	std::vector<IntermediateNode> nodes;
	std::vector<IntermediateRelationship> relationships;
};

class OCCTSerializer
{
public:
	OCCTSerializer(const ReflectionRegistry& registry);

	IntermediateGraph serialize(const Handle(Standard_Transient)& object);

	IntermediateGraph serialize(const TopoDS_Shape& shape);

private:
	int64_t serialize_shape_recursive(const TopoDS_Shape& shape);

	void serialize_recursize(const Handle(Standard_Transient)& object);

	std::any covert_simple_type_to_any(const std::any& value);

	const ReflectionRegistry& m_registry;
	IntermediateGraph m_graph;

	std::unordered_map<const TopoDS_TShape*, int64_t> m_visited_shapes;
	std::unordered_map<Standard_Transient*, int64_t> m_visited_objects;
	int64_t m_next_temp_id = 0;
};

