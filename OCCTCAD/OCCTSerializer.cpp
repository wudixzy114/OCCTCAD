#include "OCCTSerializer.h"
#include "OCCTValueConverter.h"
#include <TCollection_AsciiString.hxx>
#include <iostream>
#include <TopAbs_ShapeEnum.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Wire.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shell.hxx>
#include <TopoDS_Solid.hxx>
#include <TopoDS_CompSolid.hxx>
#include <TopoDS_Compound.hxx> 
#include <TopoDS_Iterator.hxx>
#include <BRep_Tool.hxx>
#include <Geom_Curve.hxx>
#include <Geom_Surface.hxx>
#include <TopLoc_Location.hxx>

std::string shape_enum_to_string(TopAbs_ShapeEnum type) {
	switch (type) {
	case TopAbs_COMPOUND:   return "TopoDS_Compound";
	case TopAbs_COMPSOLID:  return "TopoDS_CompSolid";
	case TopAbs_SOLID:      return "TopoDS_Solid";
	case TopAbs_SHELL:      return "TopoDS_Shell";
	case TopAbs_FACE:       return "TopoDS_Face";
	case TopAbs_WIRE:       return "TopoDS_Wire";
	case TopAbs_EDGE:       return "TopoDS_Edge";
	case TopAbs_VERTEX:     return "TopoDS_Vertex";
	case TopAbs_SHAPE:      return "TopoDS_Shape";
	default:                return "Unknown_Shape";
	}
}

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
	m_graph = {};
	m_visited_shapes.clear();
	m_next_temp_id = 0;

	if (!shape.IsNull()) {
		serialize_shape_recursive(shape);
	}

	return m_graph;
}

int64_t OCCTSerializer::serialize_shape_recursive(const TopoDS_Shape& shape)
{
	if (shape.IsNull()) {
		return -1;
	}

	const TopoDS_TShape* tshape_ptr = shape.TShape().get();
	if (m_visited_shapes.count(tshape_ptr)) {
		return m_visited_shapes.at(tshape_ptr);
	}

	TopAbs_ShapeEnum shape_type_enum = shape.ShapeType();
	const std::string type_name = shape_enum_to_string(shape_type_enum);

	const TypeDescriptor* type_desc = m_registry.get_type(type_name);
	if (!type_desc) {
		if (type_name != "TopoDS_Shape") {
			std::cerr << "Warning: Type '" << type_name << "' not found in ReflectionRegistry. Skipping shape." << std::endl;
		}
		return -1;
	}

	IntermediateNode node;
	node.temp_id = m_next_temp_id++;
	node.type_name = type_name;
	node.neo4j_label = type_desc ? type_desc->neo4j_label : type_name;
	m_visited_shapes[tshape_ptr] = node.temp_id;

	std::any shape_any;

	switch (shape_type_enum) {
	case TopAbs_VERTEX: shape_any = TopoDS::Vertex(shape); break;
	case TopAbs_EDGE:   shape_any = TopoDS::Edge(shape);   break;
	case TopAbs_WIRE:   shape_any = TopoDS::Wire(shape);   break;
	case TopAbs_FACE:   shape_any = TopoDS::Face(shape);   break;
	case TopAbs_SHELL:  shape_any = TopoDS::Shell(shape);  break;
	case TopAbs_SOLID:  shape_any = TopoDS::Solid(shape);  break;
	case TopAbs_COMPSOLID: shape_any = TopoDS::CompSolid(shape); break;
	case TopAbs_COMPOUND:  shape_any = TopoDS::Compound(shape);  break;
	default:            shape_any = shape; break;
	}

	// Call the special handler if it exists
	if (type_desc->special_handler) {
		type_desc->special_handler(shape_any, *this, node);
	}

	const int64_t current_node_id = node.temp_id;
	m_graph.nodes.push_back(std::move(node));

	TopoDS_Iterator it(shape, Standard_True, Standard_False);

	for (; it.More(); it.Next()) {
		const TopoDS_Shape& child_shape = it.Value();
		int64_t child_node_id = serialize_shape_recursive(child_shape);
		if (child_node_id != -1) {
			std::string rel_name = "HAS_SUB_SHAPE";

			const std::string child_type_name = shape_enum_to_string(child_shape.ShapeType());
			const TypeDescriptor* child_type_desc = m_registry.get_type(child_type_name);
			if (child_type_desc && !child_type_desc->relationship_name_as_child.empty()) {
				rel_name = child_type_desc->relationship_name_as_child;
			}

			IntermediateRelationship rel;
			rel.from_node_id = current_node_id;
			rel.to_node_id = child_node_id;
			rel.relationship_name = rel_name;

			m_graph.relationships.push_back(rel);
		}
	}

	return current_node_id;
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
	const int64_t current_node_id = node.temp_id;
	std::any obj_any = object;

	// =================== DEBUG BLOCK: START ===================
	std::cout << "\n[DEBUG] Serializing Transient Object: " << type_name << " (Temp ID: " << node.temp_id << ")" << std::endl;
	// =================== DEBUG BLOCK: END ===================

	// 1. Process reflective properties (attributes and relationships)
	for (const auto& [prop_name, prop_desc] : type_desc->properties) {
		std::any prop_value_any;
		try {
			prop_value_any = prop_desc.getter(obj_any);
		}
		catch (const std::bad_any_cast& e) {
			std::cerr << "Error during getter for " << type_name << "::" << prop_name << ": " << e.what() << std::endl;
			continue; // Skip this property
		}
		if (prop_desc.is_relationship) {
			serialize_and_link_property(prop_value_any, current_node_id, prop_desc);
		}
		else
		{
			node.properties[prop_name] = OCCTValueConverter::to_serializable(prop_value_any);
		}
	}

	// 2. Call special handler if it exists
	if (type_desc->special_handler) {
		type_desc->special_handler(obj_any, *this, node);
	}

	m_graph.nodes.push_back(std::move(node));
}

int64_t OCCTSerializer::serialize_transient_and_link(const Handle(Standard_Transient)& object, int64_t from_node_id, const std::string& relationship_name)
{
	if (object.IsNull()) {
		return -1;
	}

	serialize_recursize(object);
	const int64_t to_node_id = m_visited_objects.at(object.get());

	IntermediateRelationship rel;
	rel.from_node_id = from_node_id;
	rel.to_node_id = to_node_id;
	rel.relationship_name = relationship_name;
	m_graph.relationships.push_back(std::move(rel));

	return to_node_id;
}

void OCCTSerializer::serialize_and_link_property(
	const std::any& property_value,
	int64_t from_node_id,
	const PropertyDescriptor& prop_desc)
{
	if (!property_value.has_value()) {
		return;
	}

	std::cout << "     [HELPER] serialize_and_link_property called for relationship '" << prop_desc.relationship_name << "'" << std::endl;

	const auto& type = property_value.type();
	if (type == typeid(Handle(Standard_Transient))) {
		const auto& handle = std::any_cast<const Handle(Standard_Transient)&>(property_value);
		if (handle.IsNull()) {
			return;
		}

		// Recursively serialize the target object. If it's already visited, this will do nothing.
		serialize_recursize(handle);

		// The target object is now guaranteed to be in the visited map.
		const int64_t to_node_id = m_visited_objects.at(handle.get());

		// *** CREATE THE RELATIONSHIP HERE ***
		IntermediateRelationship rel;
		rel.from_node_id = from_node_id;
		rel.to_node_id = to_node_id;
		rel.relationship_name = prop_desc.relationship_name; // Use the name from metadata!
		m_graph.relationships.push_back(std::move(rel));

		return;
	}

	// --- CASE 2: The related object is a complex value type (e.g., gp_Lin) ---
	// This object needs its own node in the graph.
	const TypeDescriptor* value_type_desc = m_registry.get_type(prop_desc.type_name);
	if (!value_type_desc) {
		std::cerr << "Warning: Cannot create relationship for property '" << prop_desc.name
			<< "' because its type '" << prop_desc.type_name << "' is not registered." << std::endl;
		return;
	}

	// Since this is a value type, it's not shared and doesn't need a 'visited' check.
	// We create a new node for it every time.
	IntermediateNode value_node;
	value_node.temp_id = m_next_temp_id++;
	value_node.type_name = value_type_desc->name;
	value_node.neo4j_label = value_type_desc->neo4j_label;

	// Recursively fill the properties of this new value-node.
	for (const auto& [sub_prop_name, sub_prop_desc] : value_type_desc->properties) {
		std::any sub_prop_value = sub_prop_desc.getter(property_value); // Note: getter is called on the 'gp_Lin' any, not a handle

		// Assumption: properties of a complex value type are themselves attributes, not further nested relationships.
		// This is a reasonable simplification for types like gp_Lin, gp_Ax1 etc.
		if (!sub_prop_desc.is_relationship) {
			value_node.properties[sub_prop_name] = OCCTValueConverter::to_serializable(sub_prop_value);
		}
	}

	const int64_t to_node_id = value_node.temp_id;
	m_graph.nodes.push_back(std::move(value_node));

	// *** CREATE THE RELATIONSHIP HERE AS WELL ***
	IntermediateRelationship rel;
	rel.from_node_id = from_node_id;
	rel.to_node_id = to_node_id;
	rel.relationship_name = prop_desc.relationship_name;
	m_graph.relationships.push_back(std::move(rel));
}