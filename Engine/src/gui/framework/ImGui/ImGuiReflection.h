#pragma once

#include "core/features/Serializable.h"

#include <glm/glm.hpp>
#include <string>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include "core/features/Object.h"

// Draws any DEFINE_TYPE struct (components, graphics settings, ...) as a label | value table.
// Each field goes to the DrawField overload for its type, the FieldMeta passed to member(...) picks the widget.
// To support a new field type add a DrawField overload here and in ImGuiReflection.cpp,
// fields without one are listed as unsupported instead of failing to compile.
namespace ImGuiReflection {
    // true when the user changed the value this frame
    bool DrawField(const char* name, bool& value, const FieldMeta& meta);
    bool DrawField(const char* name, int& value, const FieldMeta& meta);
    bool DrawField(const char* name, uint32_t& value, const FieldMeta& meta);
    bool DrawField(const char* name, float& value, const FieldMeta& meta);
    bool DrawField(const char* name, glm::ivec3& value, const FieldMeta& meta);
    bool DrawField(const char* name, glm::vec3& value, const FieldMeta& meta);
    bool DrawField(const char* name, glm::vec4& value, const FieldMeta& meta);
    bool DrawField(const char* name, std::string& value, const FieldMeta& meta);

    void DrawUnsupported(const char* name);

    // a named concept instead of an inline requires in DrawFields, msvc evaluates the inline one
    // inside the generic visitor lambda as false for fields that pass a FieldMeta
    template<typename Value>
    concept HasFieldDrawer = requires(Value& value, const FieldMeta& meta) { DrawField("", value, meta); };

    // the label | value table the fields are drawn into
    bool BeginFields(const char* id);
    void EndFields();

    // the fields of object as rows, call between BeginFields and EndFields
    template<typename T>
    bool DrawFields(T& object) {
        bool changed = false;
        T::VisitFields([&](const char* name, auto memberPtr, auto&&... meta) {
            using Value = std::remove_reference_t<decltype(object.*memberPtr)>;
            if constexpr (HasFieldDrawer<Value>) {
                changed |= DrawField(name, object.*memberPtr, ResolveFieldMeta(meta...));
            } else {
                DrawUnsupported(name);
            }
        });
        return changed;
    }

    // a whole struct in its own table
    template<typename T>
    bool Draw(T& object, const char* id = T::GetStaticType()) {
        if (!BeginFields(id)) {
            return false;
        }
        bool changed = DrawFields(object);
        EndFields();
        return changed;
    }
}

// Draws objects whose type is only known at runtime (a type_index and a pointer).
// Types are registered up front, like the component and event lua bindings.
class ImGuiReflectionRegistry : public Object {
public:
    ImGuiReflectionRegistry() { setFootprint("ImGuiReflectionRegistry", sizeof(ImGuiReflectionRegistry), MemorySystem::Gui); }

    template<typename T>
    void add() {
        m_drawers[typeid(T)] = [](void* object) { return ImGuiReflection::Draw(*static_cast<T*>(object)); };
    }

    bool contains(std::type_index type) const { return m_drawers.contains(type); }

    // false when nothing changed or the type was never registered
    bool draw(std::type_index type, void* object) const {
        auto it = m_drawers.find(type);
        return it != m_drawers.end() && it->second(object);
    }

private:
    std::unordered_map<std::type_index, bool(*)(void*)> m_drawers;
};
