#include "ComponentToLuaBindings.h"
#include "core/scene/EntitySerializer.h"
#include "logging/Logger.h"

void ComponentToLuaBindings::exportComponents(sol::state& luaState, Logger* logger)
{
    luaState.new_usertype<Entity>("Entity",
        "id", [](Entity& self) {
            return self.getID();
        },
        "getComponent", [logger](Entity& self, const std::string& componentName, sol::this_state state) -> sol::object {
            sol::state_view lua(state);
            auto it = componentGetters.find(componentName);
            if (it == componentGetters.end()) {
                logger->error("Lua failed to get component '{}', export it in ComponentToLuaBindings", componentName);
                return sol::object(lua, sol::nil);
            }
            return it->second(lua, self);
        },
        "addComponent", [logger](Entity& self, const std::string& componentName, sol::object luaTable, sol::this_state state) -> sol::object {
            sol::state_view lua(state);
            auto it = componentAdders.find(componentName);
            if (it == componentAdders.end()) {
                logger->error("Failed to add component: '{}' is not exported to lua", componentName);
                return sol::object(lua, sol::nil);
            }
            return it->second(lua, self, luaTable);
        },
        "removeComponent", [logger](Entity& self, const std::string& componentName) {
            // the serializer destroyer runs the component's unload hook before removing it
            const auto& destroyers = EntitySerializer::getComponentDestroyer();
            auto it = destroyers.find(componentName);
            if (it == destroyers.end()) {
                logger->error("Failed to remove component: '{}' is not registered", componentName);
                return;
            }
            it->second(self);
        }
    );

    _registerComponent<NameComponent>(luaState);
    _registerComponent<ModelComponent>(luaState);
    _registerComponent<PrefabComponent>(luaState);
    _registerComponent<SpriteComponent>(luaState);
    _registerComponent<AnimationComponent>(luaState);
    _registerComponent<AnimationStateComponent>(luaState);
    _registerComponent<SkinningComponent>(luaState);
    _registerComponent<CameraComponent>(luaState);
    _registerComponent<ScriptComponent>(luaState);
    _registerComponent<ColliderComponent>(luaState);
    _registerComponent<ParticleEmitter>(luaState);

    // fields are edited in place, the model matrix has to be rebuilt after changing them
    auto transform = _registerComponent<TransformComponent>(luaState);
    transform.set("getModelMatrix", &TransformComponent::getModelMatrix);
    transform.set("updateTransform", &TransformComponent::updateTransform);
    transform.set("translate", &TransformComponent::translate);
    transform.set("rotate", &TransformComponent::rotate);
    transform.set("scale", &TransformComponent::scale);
}
