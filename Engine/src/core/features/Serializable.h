#pragma once

#include "Engine/EngineCoreAPI.h"

#include <concepts>
#include <string>
#include <nlohmann/json.hpp>
#include "core/features/Object.h"

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

// how an editor draws a reflected field
enum class FieldWidget { Auto, Drag, Color, Button, FilePath, Texture };

// optional third argument of a DEFINE_TYPE member, visitors that don't draw ui ignore it
struct FieldMeta {
	FieldWidget widget { FieldWidget::Auto };
	float min { 0.0f };
	float max { 0.0f };				// min and max for slider
	float speed { 0.01f };
	const char* format { nullptr };
	bool logarithmic { false };
	bool readOnly { false };		// option to viewonly, no edit
	const char* tooltip { nullptr };
};

// for resolving optional meta field for visitor, default when member has none
template<typename... Meta>
FieldMeta ResolveFieldMeta(const Meta&... meta) {
	if constexpr (sizeof...(Meta) == 0) {
		return FieldMeta{};
	} else {
		return FieldMeta(meta...);
	}
}

#define DEFINE_TYPE(ClassName, ...)                                            		\
	static const char* GetStaticType() { return #ClassName; }                   	\
	template<typename Member>                                                   	\
	static void VisitFields(Member&& member) {                                  	\
		__VA_ARGS__;                                                            	\
	}                                                                           	\
	template<typename Json> requires std::same_as<Json, nlohmann::json>         	\
	friend void to_json(Json& j, const ClassName& obj) {                        	\
		j = Json::object();                                                     	\
		auto resolver = [&j, &obj](const char* name, auto memberPtr, auto&&...) { 	\
			j[name] = obj.*memberPtr;                                           	\
		};                                                                      	\
		ClassName::VisitFields(resolver);                                       	\
	}                                                                           	\
	template<typename Json> requires std::same_as<Json, nlohmann::json>         	\
	friend void from_json(const Json& j, ClassName& obj) {                      	\
		auto resolver = [&j, &obj](const char* name, auto memberPtr, auto&&...) { 	\
			if (j.contains(name)) {                                             	\
				j.at(name).get_to(obj.*memberPtr);                              	\
			}                                                                   	\
		};                                                                      	\
		ClassName::VisitFields(resolver);                                       	\
		CallOnDeserialized(obj);                                                	\
	}

class Logger;

class MYGRAPHICSENGINE_EXPORT Serializable : public Object
{
public:
	virtual ~Serializable() = default;
	virtual void serialize(nlohmann::json& j) const = 0;
	virtual void deserialize(const nlohmann::json& j) = 0;

	static bool loadJson(const std::string& path, Logger& logger, nlohmann::json& json);

protected:
	Serializable() { setFootprint("Serializable", sizeof(Serializable), MemorySystem::Scene); };
};
