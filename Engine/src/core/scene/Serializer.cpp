#include "Serializer.h"
#include "logging/Logger.h"
#include "core/features/ServiceLocator.h"

#include <entt/entt.hpp>
using namespace entt::literals;

Serializer::Serializer()
{
	m_logger = &ServiceLocator::GetService<Logger>("Engine_LoggerPSD");

    REGISTER_COMPONENT(NameComponent, "NameComponent");
    REGISTER_COMPONENT(TransformComponent, "TransformComponent");
    REGISTER_COMPONENT(ModelComponent, "ModelComponent");
    REGISTER_COMPONENT(PrefabComponent, "PrefabComponent");
}


Serializer::~Serializer()
{

}


nlohmann::json Serializer::loadJson(const std::string &path)
{
    std::ifstream file(path);
    nlohmann::json j;
    file >> j;
    return j;
}


nlohmann::json Serializer::saveEntity(Entity entity, entt::registry& world)
{
    nlohmann::json data = nlohmann::json::object();
    entt::entity handle = entity;

    const bool isPrefab = entity.hasComponent<PrefabComponent>();

    nlohmann::json prefabComponents;

    if (isPrefab) {
        auto& prefab = entity.getComponent<PrefabComponent>();
        data["prefab_ref"] = prefab.prefabPath;
        data["overrides"]["components"] = nlohmann::json::object();

        try {
            auto prefabJson = loadJson(prefab.prefabPath);
            prefabComponents = prefabJson.value("components", nlohmann::json::object());
        } 
        catch (...) {
            m_logger->error("Failed to load prefab during save: {}", prefab.prefabPath);
        }
    } else {
        data["components"] = nlohmann::json::object();
    }

    for (auto [id, storage] : world.storage()) {
        if (!storage.contains(handle)) {
            continue;
        }

        if (id == entt::type_id<PrefabComponent>().hash()) {
            continue;
        }

        if (id == entt::type_id<RelationshipComponent>().hash()) {
            continue;
        }

        auto type = entt::resolve(id);
        if (!type) {
            continue;
        }

        auto serializeFunc = type.func("serialize"_hs);
        if (!serializeFunc) {
            continue;
        }

        const void* componentPtr = storage.value(handle);

        entt::meta_any result = serializeFunc.invoke({}, componentPtr);

        if (!result) {
            continue;
        }

        auto prop = type.prop("name"_hs);
        std::string name = prop
            ? prop.value().cast<std::string>()
            : type.info().name().data();

        auto currentJson = result.cast<nlohmann::json>();

        if (isPrefab) {
            if (prefabComponents.contains(name)) {
                auto diff = diffJson(prefabComponents[name], currentJson);
                if (!diff.empty()) {
                    data["overrides"]["components"][name] = diff;
                }
            } else {
                // Component not in prefab: always override
                data["overrides"]["components"][name] = currentJson;
            }
        } else {
            data["components"][name] = currentJson;
        }
    }

    // remove empty overrides
    if (isPrefab && data["overrides"]["components"].empty()) {
        data["overrides"].erase("components");
    }

    // serialize children
    if (entity.hasComponent<RelationshipComponent>()) {
        auto& rel = entity.getComponent<RelationshipComponent>();
        for (auto childHandle : rel.children) {
            Entity child(childHandle, world);
            data["children"].push_back(saveEntity(child, world));
        }
    }

    return data;
}


entt::entity Serializer::loadEntity(
    const nlohmann::json& data, 
    entt::registry& world, 
    std::unordered_map<uint32_t, Entity>& sceneMap, 
    entt::entity parent
) {
    entt::entity entity = world.create();
    sceneMap[static_cast<uint32_t>(entity)] = Entity(entity, world);

    nlohmann::json mergedData;
    try {
        if (data.contains("prefab_ref") && data["prefab_ref"].is_string()) {
            std::string path = data["prefab_ref"];
            mergedData = loadJson(path);
            world.emplace_or_replace<PrefabComponent>(entity, path);
            
            if (data.contains("overrides") && data["overrides"].is_object()) {
                mergedData.merge_patch(data["overrides"]);
            }
        } else {
            mergedData = data;
        }
    } 
    catch (const std::exception& e) {
        m_logger->error("Failed to parse entity data: " + std::string(e.what()));
        return entity; 
    }

    if (mergedData.contains("components") && mergedData["components"].is_object()) {
        for (auto& [name, compData] : mergedData["components"].items()) {
            auto it = component_factory.find(name);
            if (it != component_factory.end()) {
                try {
                    it->second(entity, world, compData);
                } 
                catch (const std::exception& e) {
                    m_logger->error("Error loading component '" + name + "': " + e.what());
                }
            } else {
                m_logger->warn("Unknown component type found in save file: " + name);
            }
        }
    }

    if (parent != entt::null) {
        // avoid duplicates if the prefab already had a RelationshipComponent
        auto& childRel = world.get_or_emplace<RelationshipComponent>(entity);
        childRel.parent = parent;

        auto& parentRel = world.get_or_emplace<RelationshipComponent>(parent);
        
        // add to children only if not already present
        if (std::find(parentRel.children.begin(), parentRel.children.end(), entity) == parentRel.children.end()) {
            parentRel.children.push_back(entity);
        }
    }

    // recursively load child
    if (mergedData.contains("children") && mergedData["children"].is_array()) {
        for (const auto& childData : mergedData["children"]) {
            if (!childData.is_null()) {
                loadEntity(childData, world, sceneMap, entity); 
            }
        }
    }

    return entity;
}
