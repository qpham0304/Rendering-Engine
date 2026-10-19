#pragma once

#include "Engine/EngineCoreAPI.h"

#include <concepts>
#include <string>
#include <nlohmann/json.hpp>

// optional hook a reflected type can define to rebuild derived data after from_json
template<typename T>
concept HasOnDeserialized = requires(T& t) { t.onDeserialized(); };

template<typename T>
void CallOnDeserialized(T& obj)
{
	if constexpr (HasOnDeserialized<T>) {
		obj.onDeserialized();
	}
}

#define REFLECT_TYPE(ClassName, ...)                                            \
	static const char* GetStaticType() { return #ClassName; }                   \
	template<typename Member>                                                   \
	static void VisitFields(Member&& member) {                                  \
		__VA_ARGS__;                                                            \
	}                                                                           \
	template<typename Json> requires std::same_as<Json, nlohmann::json>         \
	friend void to_json(Json& j, const ClassName& obj) {                        \
		j = Json::object();                                                     \
		auto resolver = [&j, &obj](const char* name, auto memberPtr) {          \
			j[name] = obj.*memberPtr;                                           \
		};                                                                      \
		ClassName::VisitFields(resolver);                                       \
	}                                                                           \
	template<typename Json> requires std::same_as<Json, nlohmann::json>         \
	friend void from_json(const Json& j, ClassName& obj) {                      \
		auto resolver = [&j, &obj](const char* name, auto memberPtr) {          \
			if (j.contains(name)) {                                             \
				j.at(name).get_to(obj.*memberPtr);                              \
			}                                                                   \
		};                                                                      \
		ClassName::VisitFields(resolver);                                       \
		CallOnDeserialized(obj);                                                \
	}

class Logger;

class MYGRAPHICSENGINE_EXPORT Serializable
{
public:
	virtual ~Serializable() = default;
	virtual void serialize(nlohmann::json& j) const = 0;
	virtual void deserialize(const nlohmann::json& j) = 0;

	static bool loadJson(const std::string& path, Logger& logger, nlohmann::json& json);

protected:
	Serializable() = default;
};
