#include <gtest/gtest.h>
#include "OCCTSerializer.h"
#include "Neo4jAdapter.h"
#include <BRepPrimAPI_MakeBox.hxx>
#include <cpr/cpr.h> // For direct DB interaction in test

class Neo4jIntegrationTest : public ::testing::Test {
protected:
	void SetUp() override {
		initialize_reflection();
		clear_database();
	}

	void TearDown() override {
		clear_database();
	}

	void clear_database() {
		Neo4jAdapter::Config config;
		json payload = {
			{"statements", {{{"statement", "MATCH (n) DETACH DELETE n"}}}}
		};
		cpr::Post(cpr::Url{ "http://localhost:7474/db/neo4j/tx/commit" },
			cpr::Header{ {"Authorization", "Basic bmVvNGo6cGFzc3dvcmQ="} },
			cpr::Body{ payload.dump() });
	}

	OCCTSerializer serializer{ ReflectionRegistry::instance() };
	Neo4jAdapter adapter{ {} };
};

TEST_F(Neo4jIntegrationTest, SaveBoxToNeo4j) {
	// 1. Arrange
	TopoDS_Shape box = BRepPrimAPI_MakeBox(10, 20, 30);
	IntermediateGraph graph = serializer.serialize(box);
	ASSERT_FALSE(graph.nodes.empty());

	// 2. Act
	bool success = adapter.save_graph(graph);
	ASSERT_TRUE(success);

	// 3. Assert: Query the database directly to verify
	json query_payload = {
		{"statements", {{
			{"statement", "MATCH (s:TopoDS_Solid) RETURN count(s) AS solid_count"},
			{"resultDataContents", {"row"}}
		}}}
	};

	auto r = cpr::Post(cpr::Url{ "http://localhost:7474/db/neo4j/tx/commit" },
		cpr::Header{
						  {"Authorization", "Basic bmVvNGo6cGFzc3dvcmQ="},
						  // --- FIX HERE: ADD THE CONTENT-TYPE HEADER ---
						  {"Content-Type", "application/json"}
		},
		cpr::Body{ query_payload.dump() });

	ASSERT_EQ(r.status_code, 200);
	json result = json::parse(r.text);
	ASSERT_TRUE(result["errors"].empty());

	int64_t solid_count = result["results"][0]["data"][0]["row"][0];
	EXPECT_EQ(solid_count, 1);
}