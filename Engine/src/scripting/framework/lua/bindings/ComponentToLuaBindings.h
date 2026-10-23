#pragma once

#include <sol/sol.hpp>
#include <functional>
#include <string>
#include <unordered_map>
#include "core/entities/Entity.h"
#include "core/components/MComponent.h"

class Logger;

// exports the Entity usertype and every reflected component to lua
// getComponent hands lua a pointer to the live component, edits apply directly without a copy back
// fetch it each frame instead of caching it, removing components of that type can move it in the entt storage
class ComponentToLuaBindings
{
public:
    static void exportComponents(sol::state& luaState, Logger* logger);

private:
    using ComponentGetter = std::function<sol::object(sol::state_view, Entity)>;
    using ComponentAdder = std::function<sol::object(sol::state_view, Entity, sol::object)>;

    static inline std::unordered_map<std::string, ComponentGetter> componentGetters{};
    static inline std::unordered_map<std::string, ComponentAdder> componentAdders{};

    // automatically resolve cpp component types to export to lua, the fields come from DEFINE_TYPE
    template<typename T>
    static sol::usertype<T> _registerComponent(sol::state& luaState)
    {
        // create a lua type, match cpp type
        auto luaType = luaState.new_usertype<T>(T::GetStaticType());

        // static call to the VisitFields function that each component registers itself to
        T::VisitFields([&luaType](const char* name, auto memberPtr, auto&&...) {
            luaType.set(name, memberPtr);
        });

        // lua reference to the component pointer
        componentGetters[T::GetStaticType()] = [](sol::state_view lua, Entity e) -> sol::object {
            if (!e.hasComponent<T>()) {
                return sol::object(lua, sol::nil);
            }
            return sol::make_object(lua, &e.getComponent<T>());
        };

        // default construct, then copy any matching field of the optional lua table onto it
        componentAdders[T::GetStaticType()] = [](sol::state_view lua, Entity e, sol::object luaTable) -> sol::object {
            T& component = e.getRegistry()->emplace_or_replace<T>(e);

            if (luaTable.is<sol::table>()) {
                sol::table table = luaTable.as<sol::table>();
                T::VisitFields([&](const char* name, auto memberPtr, auto&&...) {
                    using FieldType = std::remove_cvref_t<decltype(component.*memberPtr)>;
                    auto value = table[name].template get<sol::optional<FieldType>>();
                    if (value) {
                        component.*memberPtr = *value;
                    }
                });
            }

            return sol::make_object(lua, &component);
        };

        return luaType;
    }
};
