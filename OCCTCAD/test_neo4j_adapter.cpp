#include <gtest/gtest.h>
#include "Neo4jAdapter.h"

TEST(Neo4jAdapterTest, BuildPayloadCorrectly) {
	// 1. Arrange: Create a simple IntermediateGraph
    IntermediateGraph graph;
    graph.nodes.push_back({ 0, "Geom_Point", "Point", {{"x", 1.0}, {"y", 2.0}} });
    graph.nodes.push_back({ 1, "Geom_Circle", "Circle", {{"radius", 5.0}} });
    graph.relationships.push_back({ 1, 0, "HAS_CENTER" });

    Neo4jAdapter::Config cfg; // Use default config for this test
    Neo4jAdapter adapter(cfg);

    // 2. Act: Build the payload
    json payload = adapter.build_cypher_payload(graph);

    // 3. Assert: Verify the JSON structure
    ASSERT_TRUE(payload.is_object());
    ASSERT_TRUE(payload.contains("statements"));
    ASSERT_TRUE(payload["statements"].is_array());
    ASSERT_EQ(payload["statements"].size(), 3);

    // Check node creation statement
    const auto& node_stmt = payload["statements"][0];
    ASSERT_TRUE(node_stmt["parameters"]["nodes"].is_array());
    ASSERT_EQ(node_stmt["parameters"]["nodes"].size(), 2);
    EXPECT_EQ(node_stmt["parameters"]["nodes"][0]["temp_id"], 0);
    EXPECT_EQ(node_stmt["parameters"]["nodes"][0]["label"], "Point");
    EXPECT_EQ(node_stmt["parameters"]["nodes"][0]["properties"]["x"], 1.0);

    // Check relationship creation statement
    const auto& rel_stmt = payload["statements"][1];
    ASSERT_TRUE(rel_stmt["parameters"]["relationships"].is_array());
    ASSERT_EQ(rel_stmt["parameters"]["relationships"].size(), 1);
    EXPECT_EQ(rel_stmt["parameters"]["relationships"][0]["from_id"], 1);
    EXPECT_EQ(rel_stmt["parameters"]["relationships"][0]["to_id"], 0);
    EXPECT_EQ(rel_stmt["parameters"]["relationships"][0]["name"], "HAS_CENTER");
}