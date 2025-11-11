#include "OCCTSerializer.h"
#include <TCollection_AsciiString.hxx>
#include <iostream>

OCCTSerializer::OCCTSerializer(const ReflectionRegistry& registry) : m_registry(registry){}

IntermediateGraph OCCTSerializer::serialize(const Handle(Standard_Transient)& object) {
	m_graph = {};
	m_visited_objects.clear();
	m_next_temp_id = 0;

	if (!object.IsNull()) {
		serialize_recursize(object);
	}

	return m_graph;
}

IntermediateGraph OCCTSerializer::serialize(const TopoDS_Shape& shape) {
	Handle(TopoDS_TShape) tshape_handle = shape.TShape();
	return this->serialize(tshape_handle);
}

void OCCTSerializer::serialize_recursize(const Handle(Standard_Transient)& object) {
	if (m_visited_objects.count(object.get())) {
		return;
	}

	const std::string type_name = object->DynamicType()->Name();

	const TypeDescriptor* type_desc = m_registry.get_type(type_name);
	if (!type_desc) {
		std::cerr << "Warning: Type '" << type_name << "' not found in ReflectionRegistry. Skipping object." << std::endl;
		return;
	}

	IntermediateNode node;
	node.temp_id = this->m_next_temp_id++;
	node.type_name = type_name;
	node.neo4j_label = type_desc->neo4j_label;

	m_visited_objects[object.get()] = node.temp_id;
	std::any obj_any = object;

	for (const auto& [prop_name, prop_desc] : type_desc->properties) {
		std::any prop_value_any = prop_desc.getter(obj_any);
		if (prop_value_any.type() == typeid(Handle(Standard_Transient))) {
			const auto& child_handle = std::any_cast<const Handle(Standard_Transient)&>(prop_value_any);
			if (!child_handle.IsNull()) {
				this->serialize_recursize(child_handle);

				IntermediateRelationship rel;
				rel.from_node_id = node.temp_id;
				rel.to_node_id = m_visited_objects[child_handle.get()];
				rel.relationship_name = prop_desc.relationship_name;
				m_graph.relationships.push_back(std::move(rel));
			}
		}
		else
		{
			node.properties[prop_name] = covert_simple_type_to_any(prop_value_any);
		}
	}

	m_graph.nodes.push_back(std::move(node));
}

std::any OCCTSerializer::covert_simple_type_to_any(const std::any& value) {
	const auto& type = value.type();
	if (type == typeid(int)) {
		return std::any_cast<int>(value);
	}
	if (type == typeid(double)) {
		return std::any_cast<double>(value);
	}
	if (type == typeid(bool)) {
		return std::any_cast<bool>(value);
	}
	if (type == typeid(TCollection_AsciiString)) {
		return std::string(std::any_cast<const TCollection_AsciiString&>(value).ToCString());
	}
	return {};
}