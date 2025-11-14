#include <gtest/gtest.h>
#include "../OCCTSErializer.h"
#include "../OCCTValueConverter.h"
#include <gp_Pnt.hxx>
#include <gp_Dir.hxx>
#include <gp_Ax1.hxx>

class ValueConversionTest : public ::testing::Test {
protected:

};

TEST_F(ValueConversionTest, HandlesStandardPrimitives) {
	// Test Integer
	std::any int_val = Standard_Integer(42);
	std::any converted_int = OCCTValueConverter::to_serializable(int_val);
	ASSERT_EQ(converted_int.type(), typeid(int64_t));
	EXPECT_EQ(std::any_cast<int64_t>(converted_int), 42);

	// Test Real
	std::any real_val = Standard_Real(3.14159);
	std::any converted_real = OCCTValueConverter::to_serializable(real_val);
	ASSERT_EQ(converted_real.type(), typeid(double));
	EXPECT_DOUBLE_EQ(std::any_cast<double>(converted_real), 3.14159);

	// Test Boolean
	std::any bool_val_true = Standard_Boolean(true);
	std::any converted_bool_true = OCCTValueConverter::to_serializable(bool_val_true);
	ASSERT_EQ(converted_bool_true.type(), typeid(bool));
	EXPECT_TRUE(std::any_cast<bool>(converted_bool_true));

	std::any bool_val_false = Standard_Boolean(false);
	std::any converted_bool_false = OCCTValueConverter::to_serializable(bool_val_false);
	ASSERT_EQ(converted_bool_false.type(), typeid(bool));
	EXPECT_FALSE(std::any_cast<bool>(converted_bool_false));
}

TEST_F(ValueConversionTest, HandlesTCollectionAsciiString) {
	TCollection_AsciiString occt_str("Hello, OCCT!");
	std::any str_val = occt_str;
	std::any converted_str = OCCTValueConverter::to_serializable(str_val);
	ASSERT_EQ(converted_str.type(), typeid(std::string));
	EXPECT_EQ(std::any_cast<std::string>(converted_str), "Hello, OCCT!");
}

// New tests for gp_ types
TEST_F(ValueConversionTest, HandlesGpPnt) {
	gp_Pnt p(1.0, 2.5, -3.0);
	std::any converted = OCCTValueConverter::to_serializable(std::any(p));
	ASSERT_EQ(converted.type(), typeid(PropertyMap));

	const auto& map = std::any_cast<const PropertyMap&>(converted);
	ASSERT_TRUE(map.count("x") && map.count("y") && map.count("z"));
	EXPECT_DOUBLE_EQ(std::any_cast<double>(map.at("x")), 1.0);
	EXPECT_DOUBLE_EQ(std::any_cast<double>(map.at("y")), 2.5);
	EXPECT_DOUBLE_EQ(std::any_cast<double>(map.at("z")), -3.0);
}

TEST_F(ValueConversionTest, HandlesGpDir) {
	gp_Dir d(0.0, 1.0, 0.0);
	std::any converted = OCCTValueConverter::to_serializable(std::any(d));
	ASSERT_EQ(converted.type(), typeid(PropertyMap));

	const auto& map = std::any_cast<const PropertyMap&>(converted);
	ASSERT_TRUE(map.count("x") && map.count("y") && map.count("z"));
	EXPECT_DOUBLE_EQ(std::any_cast<double>(map.at("y")), 1.0);
}

TEST_F(ValueConversionTest, HandlesGpAx1) {
	gp_Pnt loc(1, 2, 3);
	gp_Dir dir(0, 0, 1);
	gp_Ax1 ax(loc, dir);

	std::any converted = OCCTValueConverter::to_serializable(std::any(ax));
	ASSERT_EQ(converted.type(), typeid(PropertyMap));

	const auto& map = std::any_cast<const PropertyMap&>(converted);
	ASSERT_TRUE(map.count("location") && map.count("direction"));

	// Test nested conversion
	const auto& loc_any = map.at("location");
	const auto& dir_any = map.at("direction");
	ASSERT_EQ(loc_any.type(), typeid(PropertyMap));
	ASSERT_EQ(dir_any.type(), typeid(PropertyMap));

	const auto& loc_map = std::any_cast<const PropertyMap&>(loc_any);
	EXPECT_DOUBLE_EQ(std::any_cast<double>(loc_map.at("x")), 1.0);
}

TEST_F(ValueConversionTest, HandlesUnsupportedType) {
	struct UnknownType {};
	std::any unknown_val = UnknownType{};
	std::any converted = OCCTValueConverter::to_serializable(unknown_val);
	EXPECT_FALSE(converted.has_value());
}
