#include "ManualReflection.h"
#include "OCCTSerializer.h"
#include "Neo4jAdapter.h"
#include <BRepPrimAPI_MakeBox.hxx>
#include <TopoDS_Shape.hxx>
#include <iostream>
#include <gtest/gtest.h>

int main(int argc, char** argv) {

    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();

    return 0;
}