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

// In tests/test_serializer.cpp

TEST_F(SerializerTest, SerializeEdgeWithUnderlyingGeometry) {
    // 1. Create geometry
    gp_Ax1 axis(gp_Pnt(0, 0, 0), gp_Dir(1, 0, 0));
    Handle(Geom_Line) line = new Geom_Line(axis);
    TopoDS_Edge edge = BRepBuilderAPI_MakeEdge(line);

    // 2. Serialize
    IntermediateGraph graph = serializer.serialize(edge);

    // 3. Assertions for the new, more detailed graph structure
    // We now expect nodes for: TopoDS_Edge, Geom_Line, gp_Ax1, gp_Pnt, gp_Dir
    // Note: gp_Pnt and gp_Dir might be serialized as properties of gp_Ax1,
    // depending on their reflection data. Let's assume for now they are also relationships.
    // If not, the expected node count will be smaller.

    // Let's print the graph to see what we actually get. This is the best way to debug.
    std::cout << "--- Generated Graph for Edge ---" << std::endl;
    std::cout << "Node Count: " << graph.nodes.size() << std::endl;
    for (const auto& node : graph.nodes) {
        std::cout << "  Node " << node.temp_id << ": " << node.type_name << std::endl;
    }
    std::cout << "Relationship Count: " << graph.relationships.size() << std::endl;
    for (const auto& rel : graph.relationships) {
        std::cout << "  Rel: " << rel.from_node_id << " -[" << rel.relationship_name << "]-> " << rel.to_node_id << std::endl;
    }
    std::cout << "--------------------------------" << std::endl;

    // Based on our change, we expect Geom_Line to have a relationship to gp_Ax1.
    const IntermediateNode* edge_node = findNodeByType(graph, "TopoDS_Edge");
    const IntermediateNode* line_node = findNodeByType(graph, "Geom_Line");
    const IntermediateNode* ax1_node = findNodeByType(graph, "gp_Ax1");

    ASSERT_NE(edge_node, nullptr);
    ASSERT_NE(line_node, nullptr);
    ASSERT_NE(ax1_node, nullptr); // This is the new crucial check

    bool found_line_to_axis = false;
    for (const auto& rel : graph.relationships) {
        if (rel.from_node_id == line_node->temp_id && rel.to_node_id == ax1_node->temp_id) {
            EXPECT_EQ(rel.relationship_name, "HAS_GP_AX1");
            found_line_to_axis = true;
        }
    }
    EXPECT_TRUE(found_line_to_axis) << "Relationship from Geom_Line to its gp_Ax1 position not found.";

    // The original test for Edge -> Geometry is still valid via special_handler
    bool found_edge_to_geom = false;
    for (const auto& rel : graph.relationships) {
        if (rel.from_node_id == edge_node->temp_id && rel.to_node_id == line_node->temp_id) {
            EXPECT_EQ(rel.relationship_name, "GEOMETRY");
            found_edge_to_geom = true;
        }
    }
    EXPECT_TRUE(found_edge_to_geom);
}


