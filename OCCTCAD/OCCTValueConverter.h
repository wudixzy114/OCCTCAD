#pragma once

#include <any>
#include <string>
#include <unordered_map>
#include <vector>

// Forward declarations of OCCT types
class TCollection_AsciiString;
class gp_Pnt;
class gp_XYZ;
class gp_Dir;
class gp_Vec;
class gp_Ax1;
class gp_Ax2;

using PropertyMap = std::unordered_map<std::string, std::any>;


namespace OCCTValueConverter {

	/**
	 * @brief Converts a given OCCT value type (non-Standard_Transient) into a
	 *        standard, serializable C++ type wrapped in std::any.
	 *
	 * This function is the bridge between OCCT's in-memory value types and a
	 * generic, persistent format.
	 *
	 * @param value The value to convert, wrapped in std::any.
	 * @return The converted value, wrapped in std::any. If the type is not
	 *         supported, returns an empty std::any.
	 */
	std::any to_serializable(const std::any& value);

	// We will add the reverse function later
	// std::any from_serializable(const std::any& value, const std::string& target_occt_type);

} // namespace OCCTValueConverter