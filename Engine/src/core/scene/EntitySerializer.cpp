#include "EntitySerializer.h"
#include "logging/Logger.h"
#include "core/features/ServiceLocator.h"

using namespace entt::literals;

// LoadHook determines what needs to be done after a component is loaded, deferred until every component of the entity is added
// UnloadHook determines what needs to be done before a component is removed
#define REGISTER_COMPONENT(Type, LoadHook, UnloadHook) \
	component_factory[#Type] = [](Entity e, const nlohmann::json& data) -> std::function<void()> { \
		e.getRegistry()->emplace_or_replace<Type>(e, data.get<Type>()); \
		return [e]() { LoadHook(e); }; \
	}; \
	component_destroyer[#Type] = [](Entity e) { \
		if (e.hasComponent<Type>()) { \
			UnloadHook(e); \
			e.removeComponent<Type>(); \
		} \
	}; \
	entt::meta<Type>() \
		.type(entt::type_id<Type>().hash()) \
		.prop("name"_hs, std::string(#Type)) \
		.func<&SerializerInternal<Type>::Serialize>("serialize"_hs) \
		.func<&SerializerInternal<Type>::TypePtrID>("typePtrID"_hs)

#define NO_DEFINE [](Entity entity) {}

// components missing here run their hooks after these, in any order
const std::vector<std::string> hookExecutionPriority = {
	"NameComponent",
	"TransformComponent",
	"PrefabComponent",
	"ModelComponent",
	"SpriteComponent",
	"AnimationComponent",
	"SkinningComponent",
	"AnimationStateComponent",
	"ColliderComponent",
	"CameraComponent",
	"ScriptComponent"
};

EntitySerializer::EntitySerializer(entt::registry& registry, EntityMap& entities)
	: Serializable(),
	  m_registry(registry),
	  m_entities(entities),
	  m_logger(ServiceLocator::GetService<Logger>("Engine_LoggerSPD"))
{
	setFootprint("EntitySerializer", sizeof(EntitySerializer), MemorySystem::Scene);
	RegisterComponents();
}

void EntitySerializer::RegisterComponents()
{
	if (!component_factory.empty()) {
		return;
	}

	REGISTER_COMPONENT(NameComponent, NO_DEFINE, NO_DEFINE);
	REGISTER_COMPONENT(TransformComponent, NO_DEFINE, NO_DEFINE);
	REGISTER_COMPONENT(ModelComponent, Entity::_onModelComponentAdded(), NO_DEFINE);
	REGISTER_COMPONENT(PrefabComponent, NO_DEFINE, NO_DEFINE);
	REGISTER_COMPONENT(SpriteComponent, Entity::_onSpriteComponentAdded(), NO_DEFINE);
	REGISTER_COMPONENT(AnimationStateComponent, Entity::_onAnimationStateComponentAdded(), NO_DEFINE);
	REGISTER_COMPONENT(ScriptComponent, Entity::_onScriptComponentAdded(), Entity::_onScriptComponentRemoved());
	REGISTER_COMPONENT(ColliderComponent, Entity::_onColliderComponentAdded(), Entity::_onColliderComponentRemoved());
	REGISTER_COMPONENT(CameraComponent, Entity::_onCameraComponentAdded(), NO_DEFINE);
	REGISTER_COMPONENT(ParticleEmitter, Entity::_onParticleEmitterAdded(), NO_DEFINE);
	REGISTER_COMPONENT(RibbonComponent, NO_DEFINE, NO_DEFINE);
	REGISTER_COMPONENT(SkinningComponent, Entity::_onSkinningComponentAdded(), NO_DEFINE);
}

void EntitySerializer::serialize(nlohmann::json& j) const
{
	// writes j["entities"] with every root entity, children are nested in their parent
	j["entities"] = nlohmann::json::array();

	for (auto& [id, entity] : m_entities) {
		bool hasParent = false;
		if (entity.hasComponent<RelationshipComponent>()) {
			hasParent = entity.getComponent<RelationshipComponent>().parent != entt::null;
		}

		// children are written inside their parent
		if (!hasParent) {
			j["entities"].push_back(serializeEntity(entity));
		}
	}
}

void EntitySerializer::deserialize(const nlohmann::json& j)
{
	if (!j.is_array()) {
		m_logger.error("EntitySerializer: expected an entity array");
		return;
	}

	for (const auto& entityData : j) {
		if (!entityData.is_null()) {
			deserializeEntity(entityData);
		}
	}
}

nlohmann::json EntitySerializer::serializeEntity(Entity entity) const
{
	nlohmann::json data = nlohmann::json::object();
	entt::entity handle = entity;
	const bool isPrefab = entity.hasComponent<PrefabComponent>();
	nlohmann::json prefabComponents = nlohmann::json::object();

	if (isPrefab) {
		const auto& prefab = entity.getComponent<PrefabComponent>();
		data["prefab_ref"] = prefab.prefabPath;

		nlohmann::json prefabJson;
		if (loadJson(prefab.prefabPath, m_logger, prefabJson)) {
			prefabComponents = prefabJson.value("components", nlohmann::json::object());
		}
	}

	nlohmann::json components = nlohmann::json::object();
	for (auto [id, storage] : m_registry.storage()) {
		if (!storage.contains(handle)) {
			continue;
		}

		// the prefab is written as prefab_ref instead, relationships as children
		if (id == entt::type_id<PrefabComponent>().hash() || id == entt::type_id<RelationshipComponent>().hash()) {
			continue;
		}

		// only components registered with REGISTER_COMPONENT have meta
		auto type = entt::resolve(id);
		if (!type) {
			continue;
		}

		auto serializeFunc = type.func("serialize"_hs);
		if (!serializeFunc) {
			continue;
		}

		auto prop = type.prop("name"_hs);
		std::string name = prop ? prop.value().cast<std::string>() : std::string(type.info().name());

		// nlohmann throws on bad conversions which crash the app if left unhandled, this does not run every frame
		try {
			// must be const void*, entt meta won't match a void* to the Serialize(const void*) parameter
			const void* componentPtr = storage.value(handle);
			entt::meta_any result = serializeFunc.invoke({}, componentPtr);
			if (!result) {
				m_logger.warn("Failed to serialize {}: meta invoke failed", name);
				continue;
			}

			nlohmann::json current = result.cast<nlohmann::json>();

			if (!isPrefab || !prefabComponents.contains(name)) {
				components[name] = current;	// component not in the prefab is always an override
			}
			else {
				nlohmann::json diff = diffJson(prefabComponents.at(name), current);
				if (!diff.empty()) {
					components[name] = diff;
				}
			}
		}
		catch (const nlohmann::json::exception& e) {
			m_logger.warn("Failed to serialize {}: {}", name, e.what());
		}
	}

	if (isPrefab) {
		if (!components.empty()) {
			data["overrides"]["components"] = components;
		}
	}
	else {
		data["components"] = components;
	}

	if (entity.hasComponent<RelationshipComponent>()) {
		for (entt::entity child : entity.getComponent<RelationshipComponent>().children) {
			if (m_registry.valid(child)) {
				data["children"].push_back(serializeEntity(Entity(child, m_registry)));
			}
		}
	}

	return data;
}

entt::entity EntitySerializer::deserializeEntity(const nlohmann::json& data, entt::entity parent)
{
	entt::entity handle = m_registry.create();
	Entity entity(handle, m_registry);
	m_entities[entt::to_integral(handle)] = entity;

	// default components like scene's addentity, the json values below replace these
	entity.addComponent<TransformComponent>();
	entity.addComponent<NameComponent>("Entity");

	// prefab instance: prefab json with the instance overrides patched on top
	nlohmann::json merged = data;
	if (data.contains("prefab_ref") && data["prefab_ref"].is_string()) {
		std::string path = data["prefab_ref"].get<std::string>();
		if (!loadJson(path, m_logger, merged)) {
			merged = nlohmann::json::object();
		}
		m_registry.emplace_or_replace<PrefabComponent>(handle, path);

		if (data.contains("overrides") && data["overrides"].is_object()) {
			merged.merge_patch(data["overrides"]);
		}
	}

	if (merged.contains("components") && merged["components"].is_object()) {
		// add every component first so hooks can see the whole entity
		std::unordered_map<std::string, std::function<void()>> hooks;
		for (const auto& [name, componentData] : merged["components"].items()) {
			auto it = component_factory.find(name);
			if (it == component_factory.end()) {
				m_logger.warn("Unknown component type '{}', register it in EntitySerializer::RegisterComponents", name);
				continue;
			}

			try {
				hooks[name] = it->second(entity, componentData);
			}
			catch (const std::exception& e) {
				m_logger.error("Failed to deserialize {}: {}", name, e.what());
			}
		}

		for (const auto& name : hookExecutionPriority) {
			auto it = hooks.find(name);
			if (it != hooks.end()) {
				_runHook(entity, name, it->second);
				hooks.erase(it);	// prevent double call
			}
		}

		for (auto& [name, hook] : hooks) {
			_runHook(entity, name, hook);
		}
	}

	if (parent != entt::null) {
		// avoid duplicates if the prefab already had a RelationshipComponent
		auto& childRelationship = m_registry.get_or_emplace<RelationshipComponent>(handle);
		childRelationship.parent = parent;

		auto& parentRelationship = m_registry.get_or_emplace<RelationshipComponent>(parent);
		if (std::find(parentRelationship.children.begin(), parentRelationship.children.end(), handle) == parentRelationship.children.end()) {
			parentRelationship.children.push_back(handle);
		}
	}

	if (merged.contains("children") && merged["children"].is_array()) {
		for (const auto& childData : merged["children"]) {
			if (!childData.is_null()) {
				deserializeEntity(childData, handle);
			}
		}
	}

	return handle;
}

void EntitySerializer::_runHook(Entity entity, const std::string& componentName, const std::function<void()>& hook)
{
	try {
		hook();
	}
	catch (const std::exception& e) {
		m_logger.error("Loading {} on entity {} failed: {}", componentName, _describe(entity), e.what());
	}
}

std::string EntitySerializer::_describe(Entity entity)
{
	if (entity.hasComponent<NameComponent>()) {
		return "'" + entity.getComponent<NameComponent>().name + "'";
	}
	return "id " + std::to_string(entity.getID());
}

nlohmann::json EntitySerializer::diffJson(const nlohmann::json& prefab, const nlohmann::json& current)
{
	nlohmann::json out = nlohmann::json::object();

	for (const auto& [key, value] : current.items()) {
		if (!prefab.contains(key) || prefab[key] != value) {
			out[key] = value;
		}
	}

	return out;
}

const std::map<std::string, EntitySerializer::ComponentLoader>& EntitySerializer::getComponentFactory()
{
	RegisterComponents();
	return component_factory;
}

const std::map<std::string, EntitySerializer::ComponentDestroyer>& EntitySerializer::getComponentDestroyer()
{
	RegisterComponents();
	return component_destroyer;
}
