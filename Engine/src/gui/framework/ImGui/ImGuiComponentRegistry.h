#pragma once

#include "gui/framework/ImGui/ImGuiReflection.h"
#include "core/entities/Entity.h"

#include <functional>
#include <string>
#include <typeinfo>
#include <unordered_map>
#include <vector>

// Draws every registered component an entity has as a collapsing header, DEFINE_TYPE fields go through ImGuiReflection.
// styles replaces the widget of the named fields, the rest are picked from their type and FieldMeta.
// draw replaces the whole body, onFieldChanged runs for the named field after an edit, onChanged after any edit.
// Removing and adding go through the EntitySerializer hooks when the type has them, so a component behaves like it was loaded from a scene.
class ImGuiComponentRegistry : public Object {
public:
    template<typename T>
    struct Options {
        bool removable { true };
        bool addable { false };
        ImGuiReflection::FieldStyles styles;
        std::function<bool(Entity, T&)> draw;
        std::function<void(Entity, T&)> extras;
        std::unordered_map<std::string, std::function<void(Entity, T&)>> onFieldChanged;
        std::function<void(Entity, T&)> onChanged;
    };

    ImGuiComponentRegistry() { setFootprint("ImGuiComponentRegistry", sizeof(ImGuiComponentRegistry), MemorySystem::Gui); }

    template<typename T>
    void add(const char* label, Options<T> options = {}) {
        constexpr bool reflected = requires { T::GetStaticType(); };

        Entry entry;
        entry.label = label;
        if constexpr (reflected) {
            entry.typeName = T::GetStaticType();
        } else {
            entry.typeName = typeid(T).name();
        }
        entry.removable = options.removable;
        entry.addable = options.addable;
        entry.has = [](Entity entity) { return entity.hasComponent<T>(); };
        entry.emplace = [](Entity entity) { entity.addComponent<T>(); };
        entry.remove = [](Entity entity) { entity.removeComponent<T>(); };
        entry.draw = [options = std::move(options), label](Entity entity) {
            T& component = entity.getComponent<T>();
            bool changed = false;
            const char* changedField = nullptr;
            if (options.draw) {
                changed = options.draw(entity, component);
            } else if constexpr (reflected) {
                changed = ImGuiReflection::Draw(component, label, &options.styles, &changedField);
            }
            if (options.extras) {
                options.extras(entity, component);
            }
            if (changedField) {
                auto fieldHook = options.onFieldChanged.find(changedField);
                if (fieldHook != options.onFieldChanged.end()) {
                    fieldHook->second(entity, component);
                }
            }
            if (changed && options.onChanged) {
                options.onChanged(entity, component);
            }
        };
        m_entries.push_back(std::move(entry));
    }

    void draw(Entity entity);
    void drawAddMenu(Entity entity);

private:
    struct Entry {
        const char* label { nullptr };
        std::string typeName;
        bool removable { true };
        bool addable { false };
        std::function<bool(Entity)> has;
        std::function<void(Entity)> emplace;
        std::function<void(Entity)> remove;
        std::function<void(Entity)> draw;
    };

    void _add(const Entry& entry, Entity entity);
    void _remove(const Entry& entry, Entity entity);

    std::vector<Entry> m_entries;
};
