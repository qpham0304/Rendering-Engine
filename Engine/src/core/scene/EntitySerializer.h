#pragma once

#include "Engine/EngineCoreAPI.h"
#include "core/features/Serializable.h"
#include "core/entities/Entity.h"
#include "core/components/MComponent.h"

#include <functional>
#include <map>
#include <typeindex>
#include <unordered_map>

// entt meta "serialize" function, works on the raw storage pointer of the component
template<typename Type>
struct SerializerInternal {
	static nlohmann::json Serialize(const void* instance) {
		if (!instance) {
			return nlohmann::json::object();
		}
		return nlohmann::json(*static_cast<const Type*>(instance));
	}

	static std::type_index TypePtrID() {
		return std::type_index(typeid(Type));
	}
};

class Logger;

// scene json layout:
//   { "components": { "<Type>": {...} }, "children": [ ... ] }
//   { "prefab_ref": "<path>", "overrides": { "components": { "<Type>": { <changed fields> } } }, "children": [ ... ] }
class MYGRAPHICSENGINE_EXPORT EntitySerializer : public Serializable
{
public:
	typedef std::unordered_map<uint32_t, Entity> EntityMap;
	
	using ComponentLoader = std::function<std::function<void()>(Entity, const nlohmann::json&)>;
	using ComponentDestroyer = std::function<void(Entity)>;

	EntitySerializer(entt::registry& registry, EntityMap& entities);
	~EntitySerializer() = default;

	static void RegisterComponents();

	virtual void serialize(nlohmann::json& j) const override;
	virtual void deserialize(const nlohmann::json& j) override;

	nlohmann::json serializeEntity(Entity entity) const;
	entt::entity deserializeEntity(const nlohmann::json& data, entt::entity parent = entt::null);

	// fields of current that are missing or different in prefab
	static nlohmann::json diffJson(const nlohmann::json& prefab, const nlohmann::json& current);

	static const std::map<std::string, ComponentLoader>& getComponentFactory();
	static const std::map<std::string, ComponentDestroyer>& getComponentDestroyer();

private:
	void _runHook(Entity entity, const std::string& componentName, const std::function<void()>& hook);
	static std::string _describe(Entity entity);

private:
	entt::registry& m_registry;
	EntityMap& m_entities;
	Logger& m_logger;

	inline static std::map<std::string, ComponentLoader> component_factory;
	inline static std::map<std::string, ComponentDestroyer> component_destroyer;
};
