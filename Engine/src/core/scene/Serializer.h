#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <functional>
#include "entt.hpp"
#include "core/entities/Entity.h"
#include "core/components/MComponent.h"

template<typename Type>
struct SerializerInternal {
    static nlohmann::json Serialize(const void* instance) {
        if (!instance) {
            return nlohmann::json::object();
        }
        return nlohmann::json(*static_cast<const Type*>(instance));
    }
};

#define REGISTER_COMPONENT(Type, Name, HookFunction) \
    component_factory[Name] = [](Entity e, const nlohmann::json& data) { \
        e.getRegistry()->emplace_or_replace<Type>(e, data.get<Type>()); \
        HookFunction(e); \
    }; \
    entt::meta<Type>() \
        .type(entt::type_id<Type>().hash()) \
        .prop("name"_hs, std::string(Name)) \
        .func<&SerializerInternal<Type>::Serialize>("serialize"_hs) \
        
class Logger;

class Serializer 
{
public:
    Serializer();
    ~Serializer();

    static nlohmann::json diffJson(
        const nlohmann::json& prefab,
        const nlohmann::json& current
    ) {
        nlohmann::json out = nlohmann::json::object();

        for (auto& [key, value] : current.items()) {
            if (!prefab.contains(key) || prefab[key] != value) {
                out[key] = value;
            }
        }

        return out;
    }

    nlohmann::json loadJson(const std::string& path);
    nlohmann::json saveEntity(Entity entity);

    entt::entity loadEntity(
        const nlohmann::json& data, 
        entt::registry& world, 
        std::unordered_map<uint32_t, Entity>& sceneMap,
        entt::entity parent = entt::null
    );

private:
    using ComponentLoader = std::function<void(Entity, const nlohmann::json&)>;
    std::map<std::string, ComponentLoader> component_factory;
    Logger* m_logger;
};