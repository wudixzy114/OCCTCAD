// tests/test_serializer.cpp
#include <gtest/gtest.h>
#include "OCCTSerializer.h"
#include "ManualReflection.h" // Needed to initialize the registry

// OCCT headers for creating test geometry
#include <gp_Pnt.hxx>
#include <gp_Lin.hxx>
#include <Geom_Line.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <TopoDS_Edge.hxx>

const IntermediateNode* findNodeByType(const IntermediateGraph& graph, const std::string& type_name) {
	for (const auto& node : graph.nodes) {
		if (node.type_name == type_name) {
			return &node;
		}
	}
	return nullptr;
}

class SerializerTest : public ::testing::Test {
protected:
	void SetUp() override {
		initialize_reflection();
	}

	OCCTSerializer serializer{ ReflectionRegistry::instance() };
};

TEST_F(SerializerTest, SerializeEdgeWithUnderlyingGeometry) {
	// 1. Create a simple OCCT Edge based on a line
	gp_Pnt p1(0, 0, 0);
	gp_Pnt p2(10, 0, 0);
	gp_Dir dir(gp_Vec(p1, p2));
	gp_Lin lin(p1, dir);
	Handle(Geom_Line) line = new Geom_Line(lin);
	TopoDS_Edge edge = BRepBuilderAPI_MakeEdge(line);

	// 2. Serialize the edge
	IntermediateGraph graph = serializer.serialize(edge);

	// 3. Assertions: Verify the generated graph structure
	// We expect 3 nodes: Edge, its underlying Line, and the Line's gp_Lin
	ASSERT_EQ(graph.nodes.size(), 3)
		<< "Expected 3 nodes: TopoDS_Edge, Geom_Line, and gp_Lin.";

	const IntermediateNode* edge_node = findNodeByType(graph, "TopoDS_Edge");
	const IntermediateNode* line_node = findNodeByType(graph, "Geom_Line");
	const IntermediateNode* gp_lin_node = findNodeByType(graph, "gp_Lin");

	ASSERT_NE(edge_node, nullptr) << "TopoDS_Edge node not found.";
	ASSERT_NE(line_node, nullptr) << "Geom_Line node not found.";
	ASSERT_NE(gp_lin_node, nullptr) << "gp_Lin node not found.";

	// We expect 2 relationships: Edge -> Line, and Line -> gp_Lin
	ASSERT_EQ(graph.relationships.size(), 2)
		<< "Expected 2 relationships: Edge->Geometry and Geometry->Definition.";

	bool found_edge_to_geom = false;
	bool found_geom_to_def = false;

	for (const auto& rel : graph.relationships) {
		if (rel.from_node_id == edge_node->temp_id && rel.to_node_id == line_node->temp_id) {
			EXPECT_EQ(rel.relationship_name, "GEOMETRY");
			found_edge_to_geom = true;
		}
		if (rel.from_node_id == line_node->temp_id && rel.to_node_id == gp_lin_node->temp_id) {
			// This relationship comes from the reflection system's 'Handle' detection
			EXPECT_EQ(rel.relationship_name, "HAS_GP_LIN");
			found_geom_to_def = true;
		}
	}

	EXPECT_TRUE(found_edge_to_geom) << "Relationship from Edge to its Geometry not found.";
	EXPECT_TRUE(found_geom_to_def) << "Relationship from Geom_Line to its gp_Lin definition not found.";
}


