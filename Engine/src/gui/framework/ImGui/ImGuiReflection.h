#pragma once

#include "core/features/Serializable.h"

#include <glm/glm.hpp>
#include <functional>
#include <string>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include "core/features/Object.h"

// Draws any DEFINE_TYPE struct (components, graphics settings, ...) as a label | value table.
// To support a new field type add a DrawField overload here and in ImGuiReflection.cpp,
// fields without one are listed as unsupported instead of failing to compile.
namespace ImGuiReflection {
    enum class FieldWidget { Auto, Color, Button, Combo, FilePath, ModelPath, TexturePath };

    // the FieldMeta of a member and the widget picked for it
    struct FieldView : FieldMeta {
        FieldWidget widget { FieldWidget::Auto };

        FieldView(const FieldMeta& meta = {}, FieldWidget widget = FieldWidget::Auto) : FieldMeta(meta), widget(widget) {}
    };

    FieldView InferField(const char* owner, const char* name, std::type_index type, const FieldMeta& meta);

    struct FieldStyle {
        std::type_index type { typeid(void) };
        std::function<bool(const char* name, void* value, const FieldView& view)> draw;
    };

    using FieldStyles = std::unordered_map<std::string, FieldStyle>;

    // fn is bool(const char* name, Value& value, const FieldView& view), wrap it in a FieldRow to line up with the other fields
    template<typename Value, typename Fn>
    FieldStyle Style(Fn fn) {
        return FieldStyle{ typeid(Value), [fn = std::move(fn)](const char* name, void* value, const FieldView& view) {
            return fn(name, *static_cast<Value*>(value), view);
        } };
    }

    // label cell, then the value cell with a full width item, readOnly fields stay disabled until it goes out of scope
    class FieldRow {
    public:
        FieldRow(const char* name, const FieldView& view);
        ~FieldRow();
        FieldRow(const FieldRow&) = delete;
        FieldRow& operator=(const FieldRow&) = delete;

    private:
        bool m_disabled;
    };

    // X Y Z drags with colored buttons that reset the axis
    FieldStyle AxisStyle(float resetValue = 0.0f, const char* label = nullptr);

    // a uint32_t texture id shown as the image at the value column width
    FieldStyle TextureStyle();

    // editable path, Browse and the assets that can be picked for kind, no table row
    bool AssetPath(const char* id, std::string& path, FieldWidget kind);

    // true when the user changed the value this frame
    bool DrawField(const char* name, bool& value, const FieldView& view);
    bool DrawField(const char* name, int& value, const FieldView& view);
    bool DrawField(const char* name, uint32_t& value, const FieldView& view);
    bool DrawField(const char* name, float& value, const FieldView& view);
    bool DrawField(const char* name, glm::ivec3& value, const FieldView& view);
    bool DrawField(const char* name, glm::vec3& value, const FieldView& view);
    bool DrawField(const char* name, glm::vec4& value, const FieldView& view);
    bool DrawField(const char* name, std::string& value, const FieldView& view);

    void DrawUnsupported(const char* name);

    // a named concept instead of an inline requires in DrawFields, msvc evaluates the inline one
    // inside the generic visitor lambda as false for fields that pass a FieldMeta
    template<typename Value>
    concept HasFieldDrawer = requires(Value& value, const FieldView& view) { DrawField("", value, view); };

    // the label | value table the fields are drawn into
    bool BeginFields(const char* id);
    void EndFields();

    // the fields of object as rows, call between BeginFields and EndFields
    // changedField is set to the name of the field the user changed this frame
    template<typename T>
    bool DrawFields(T& object, const FieldStyles* styles = nullptr, const char** changedField = nullptr) {
        bool changed = false;
        T::VisitFields([&](const char* name, auto memberPtr, auto&&... meta) {
            using Value = std::remove_reference_t<decltype(object.*memberPtr)>;
            FieldView view = InferField(T::GetStaticType(), name, typeid(Value), ResolveFieldMeta(meta...));
            bool fieldChanged = false;
            const FieldStyle* style = nullptr;
            if (styles) {
                auto it = styles->find(name);
                if (it != styles->end() && it->second.type == typeid(Value)) {
                    style = &it->second;
                }
            }

            if (style) {
                fieldChanged = style->draw(name, &(object.*memberPtr), view);
            } else if constexpr (HasFieldDrawer<Value>) {
                fieldChanged = DrawField(name, object.*memberPtr, view);
            } else {
                DrawUnsupported(name);
            }

            if (fieldChanged && changedField) {
                *changedField = name;
            }
            changed |= fieldChanged;
        });
        return changed;
    }

    // a whole struct in its own table
    template<typename T>
    bool Draw(T& object, const char* id = T::GetStaticType(), const FieldStyles* styles = nullptr, const char** changedField = nullptr) {
        if (!BeginFields(id)) {
            return false;
        }
        bool changed = DrawFields(object, styles, changedField);
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
    void add(ImGuiReflection::FieldStyles styles = {}) {
        m_drawers[typeid(T)] = [styles = std::move(styles)](void* object) {
            return ImGuiReflection::Draw(*static_cast<T*>(object), T::GetStaticType(), &styles);
        };
    }

    bool contains(std::type_index type) const { return m_drawers.contains(type); }

    // false when nothing changed or the type was never registered
    bool draw(std::type_index type, void* object) const {
        auto it = m_drawers.find(type);
        return it != m_drawers.end() && it->second(object);
    }

private:
    std::unordered_map<std::type_index, std::function<bool(void*)>> m_drawers;
};
