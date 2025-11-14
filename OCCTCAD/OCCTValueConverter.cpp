#include "OCCTValueConverter.h"
#include <Standard_Integer.hxx>
#include <Standard_Real.hxx>
#include <Standard_Boolean.hxx>
#include <TCollection_AsciiString.hxx>
#include <gp_Pnt.hxx>
#include <gp_XYZ.hxx>
#include <gp_Dir.hxx>
#include <gp_Vec.hxx>
#include <gp_Ax1.hxx>
#include <gp_Ax2.hxx>
#include <iostream>

namespace OCCTValueConverter {
	std::any to_serializable(const std::any& value) {
		if (!value.has_value()) {
			return {};
		}
		const auto& type = value.type();

		// --- Primitive Types ---
		if (type == typeid(Standard_Integer)) return std::any(static_cast<int64_t>(std::any_cast<Standard_Integer>(value)));
		if (type == typeid(int)) return std::any(static_cast<int64_t>(std::any_cast<int>(value)));
		if (type == typeid(Standard_Real)) return std::any(std::any_cast<Standard_Real>(value));
		if (type == typeid(double)) return std::any(std::any_cast<double>(value));
		if (type == typeid(Standard_Boolean)) return std::any(std::any_cast<Standard_Boolean>(value));
		if (type == typeid(bool)) return std::any(std::any_cast<bool>(value));

		// --- String Type ---
		if (type == typeid(TCollection_AsciiString)) {
			return std::any(std::string(std::any_cast<const TCollection_AsciiString&>(value).ToCString()));
		}

		// --- Geometric Point/Vector Types ---
		if (type == typeid(gp_Pnt)) {
			const auto& p = std::any_cast<const gp_Pnt&>(value);
			return std::any(PropertyMap{ {"x", p.X()}, {"y", p.Y()}, {"z", p.Z()} });
		}
		if (type == typeid(gp_XYZ)) {
			const auto& xyz = std::any_cast<const gp_XYZ&>(value);
			return std::any(PropertyMap{ {"x", xyz.X()}, {"y", xyz.Y()}, {"z", xyz.Z()} });
		}
		if (type == typeid(gp_Dir)) {
			const auto& dir = std::any_cast<const gp_Dir&>(value);
			return std::any(PropertyMap{ {"x", dir.X()}, {"y", dir.Y()}, {"z", dir.Z()} });
		}
		if (type == typeid(gp_Vec)) {
			const auto& vec = std::any_cast<const gp_Vec&>(value);
			return std::any(PropertyMap{ {"x", vec.X()}, {"y", vec.Y()}, {"z", vec.Z()} });
		}

		// --- Geometric Axis Types ---
		if (type == typeid(gp_Ax1)) {
			const auto& ax = std::any_cast<const gp_Ax1&>(value);
			return std::any(PropertyMap{
				{"location", to_serializable(std::any(ax.Location()))},
				{"direction", to_serializable(std::any(ax.Direction()))}
				});
		}
		if (type == typeid(gp_Ax2)) {
			const auto& ax = std::any_cast<const gp_Ax2&>(value);
			return std::any(PropertyMap{
				{"location", to_serializable(std::any(ax.Location()))},
				{"main_direction", to_serializable(std::any(ax.Direction()))},
				{"x_direction", to_serializable(std::any(ax.XDirection()))},
				{"y_direction", to_serializable(std::any(ax.YDirection()))}
				});
		}

		// --- Enums ---
		// Handle enums explicitly to avoid ambiguity.
		// Example for TopAbs_Orientation, add more as needed.
		// Note: You might already have similar logic. This makes it central.
		// if (type == typeid(TopAbs_Orientation)) {
		//     return std::any(static_cast<int32_t>(std::any_cast<TopAbs_Orientation>(value)));
		// }

		// --- Fallback for unhandled types ---
		std::cerr << "Warning: Unhandled type in OCCTValueConverter::to_serializable: " << type.name() << std::endl;
		return {};
	}
}