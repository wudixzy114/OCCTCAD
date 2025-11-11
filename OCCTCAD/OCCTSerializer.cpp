#include "OCCTSerializer.h"
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
	Handle(TopoDS_TShape) tshape_handle = shape.TShape();
	return this->serialize(tshape_handle);
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
	}

	IntermediateNode node;
	node.temp_id = m_next_temp_id++;
	node.type_name = type_name;
	// 如果找到了描述符，使用它的 neo4j_label，否则使用类型名
	node.neo4j_label = type_desc ? type_desc->neo4j_label : type_name;

	m_visited_shapes[tshape_ptr] = node.temp_id;
	if (type_desc) {
		std::any shape_any;

		// 确保 TopoDS::Xyz() 函数的结果被正确放入 shape_any
		switch (shape_type_enum) {
		case TopAbs_VERTEX: shape_any = TopoDS::Vertex(shape); break;
		case TopAbs_EDGE:   shape_any = TopoDS::Edge(shape);   break;
		case TopAbs_FACE:   shape_any = TopoDS::Face(shape);   break;
		}

		for (const auto& [prop_name, prop_desc] : type_desc->properties) {
			if (!prop_desc.is_relationship) {
				try {
					std::any prop_value = prop_desc.getter(shape_any);
					node.properties[prop_name] = covert_simple_type_to_any(prop_value);
				}
				catch (const std::bad_any_cast& e) {
					std::cerr << "Error getting property '" << prop_name << "' for type '" << type_name << "': " << e.what() << std::endl;
				}
			}
			// 我们将在下一步处理关系
		}
	}

	m_graph.nodes.push_back(std::move(node));
	return node.temp_id;
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
	// --- 简单 C++ 类型 ---
	if (type == typeid(int) || type == typeid(Standard_Integer)) return std::any_cast<int>(value);
	if (type == typeid(double) || type == typeid(Standard_Real)) return std::any_cast<double>(value);
	if (type == typeid(bool) || type == typeid(Standard_Boolean)) return std::any_cast<bool>(value);
	if (type == typeid(TopAbs_Orientation) || type == typeid(TopAbs_ShapeEnum)) {
		// 枚举类型可以转换为整数存储
		return static_cast<int>(std::any_cast<int>(value));
	}
	// --- OCCT 字符串类型 ---
	if (type == typeid(TCollection_AsciiString)) {
		return std::string(std::any_cast<const TCollection_AsciiString&>(value).ToCString());
	}

	// --- OCCT 几何值类型 (作为复杂属性) ---
	if (type == typeid(gp_Pnt)) {
		const auto& p = std::any_cast<const gp_Pnt&>(value);
		// 将其转换为一个 map，这可以很容易地被序列化为 JSON
		std::unordered_map<std::string, double> pnt_map;
		pnt_map["x"] = p.X();
		pnt_map["y"] = p.Y();
		pnt_map["z"] = p.Z();
		return pnt_map;
	}
	if (type == typeid(gp_XYZ)) {
		const auto& xyz = std::any_cast<const gp_XYZ&>(value);
		std::unordered_map<std::string, double> xyz_map;
		xyz_map["x"] = xyz.X();
		xyz_map["y"] = xyz.Y();
		xyz_map["z"] = xyz.Z();
		return xyz_map;
	}

	std::cerr << "Warning: Unhandled simple type in convert_simple_type_to_any: " << type.name() << std::endl;
	return {};
}