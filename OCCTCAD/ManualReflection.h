#pragma once
#include <string>
#include <vector>	
#include <unordered_map>
#include <functional>
#include <any>

class gp_Pnt;
class gp_Lin;

using GenericGetter = std::function<std::any(const std::any&)>;
using GenericSetter = std::function<void (std::any&, const std::any&)> ;

// 描述一个 C++ 成员变量（属性），包含其名称 (coord)、类型名 (gp_XYZ)、是否是关系，以及最核心的 GenericGetter 和 GenericSetter。
struct PropertyDescriptor {
	std::string name;
	std::string type_name;
	GenericGetter getter;
	GenericSetter setter;

	bool is_relationship = false;
	std::string relationship_name;
};

// 描述一个 C++ 类，包含其名称、Neo4j Label (gp_Pnt) 和所有属性的映射
struct TypeDescriptor {
	std::string name;
	std::string neo4j_label;
	std::unordered_map<std::string, PropertyDescriptor> properties;
};

class ReflectionRegistry {
public:
	static ReflectionRegistry& instance() {
		static ReflectionRegistry inst;
		return inst;
	}

	void register_type(TypeDescriptor descriptor) {
		m_types[descriptor.name] = std::move(descriptor);
	}

	const TypeDescriptor* get_type(const std::string& name) const {
		auto it = m_types.find(name);
		if (it != m_types.end()) {
			return &it->second;
		}
		return nullptr;
	}

	ReflectionRegistry(const ReflectionRegistry&) = delete;
	ReflectionRegistry& operator=(const ReflectionRegistry&) = delete;

private:
	ReflectionRegistry() = default;
	std::unordered_map<std::string, TypeDescriptor> m_types;
};

void initialize_reflection();

