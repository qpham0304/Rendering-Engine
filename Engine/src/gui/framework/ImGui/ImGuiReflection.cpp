#include "ImGuiReflection.h"

#include "core/features/ServiceLocator.h"
#include "core/resources/managers/TextureManager.h"
#include "core/resources/managers/ModelManager.h"
#include "core/features/Texture.h"
#include "graphics/utils/Utils.h"
#include <imgui.h>
#include <imgui_stdlib.h>
#include <algorithm>
#include <cctype>
#include <filesystem>

namespace {
    const char* DisplayName(const char* fieldName) {
        static std::unordered_map<const char*, std::string> cache;
        auto it = cache.find(fieldName);
        if (it != cache.end()) {
            return it->second.c_str();
        }

        std::string label;
        for (const char* c = fieldName; *c; c++) {
            if (c == fieldName) {
                label += static_cast<char>(std::toupper(static_cast<unsigned char>(*c)));
                continue;
            }
            if (std::isupper(static_cast<unsigned char>(*c)) && !std::isupper(static_cast<unsigned char>(*(c - 1)))) {
                label += ' ';
            }
            label += *c;
        }
        return cache.emplace(fieldName, std::move(label)).first->second.c_str();
    }

    bool HasRange(const FieldMeta& meta) { return meta.min < meta.max; }
    const char* Format(const FieldMeta& meta) { return meta.format ? meta.format : "%.3f"; }
    ImGuiSliderFlags Flags(const FieldMeta& meta) { return meta.logarithmic ? ImGuiSliderFlags_Logarithmic : ImGuiSliderFlags_None; }

    bool IsPrimitive(const std::string& path) { return path.rfind("$prim$", 0) == 0; }

    std::string AssetName(const std::string& path) {
        if (IsPrimitive(path)) {
            return path.substr(6);
        }
        return std::filesystem::path(path).filename().string();
    }

    std::string Lower(const char* text) {
        std::string lower(text);
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return lower;
    }

    bool Contains(const std::string& text, std::initializer_list<const char*> words) {
        return std::any_of(words.begin(), words.end(), [&](const char* word) { return text.find(word) != std::string::npos; });
    }

    bool EndsWith(const std::string& text, std::initializer_list<const char*> suffixes) {
        return std::any_of(suffixes.begin(), suffixes.end(), [&](const char* suffix) {
            std::string_view s(suffix);
            return text.size() >= s.size() && text.compare(text.size() - s.size(), s.size(), s) == 0;
        });
    }

    struct FieldKey {
        const char* owner;
        const char* name;
        std::type_index type;
        bool operator==(const FieldKey&) const = default;
    };

    struct FieldKeyHash {
        size_t operator()(const FieldKey& key) const {
            size_t hash = std::hash<const void*>{}(key.owner);
            hash ^= std::hash<const void*>{}(key.name) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            hash ^= key.type.hash_code() + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            return hash;
        }
    };

    ImGuiReflection::FieldWidget PickWidget(const char* owner, const char* name, std::type_index type, const FieldMeta& meta) {
        using ImGuiReflection::FieldWidget;
        std::string field = Lower(name);

        if (type == typeid(bool) && meta.action) {
            return FieldWidget::Button;
        }
        if (type == typeid(int) && meta.options) {
            return FieldWidget::Combo;
        }
        if ((type == typeid(glm::vec3) || type == typeid(glm::vec4)) && Contains(field, { "color", "colour", "tint", "albedo" })) {
            return FieldWidget::Color;
        }
        if (type == typeid(std::string) && EndsWith(field, { "path", "file", "map" })) {
            std::string qualified = Lower(owner) + "." + field;
            if (Contains(qualified, { "texture", "sprite", "image", "map" })) {
                return FieldWidget::TexturePath;
            }
            if (Contains(qualified, { "model", "mesh" })) {
                return FieldWidget::ModelPath;
            }
            return FieldWidget::FilePath;
        }
        return FieldWidget::Auto;
    }

    bool AssetItem(const std::string& assetPath, std::string& path) {
        ImGui::PushID(assetPath.c_str());
        bool picked = ImGui::Selectable(AssetName(assetPath).c_str(), assetPath == path);
        if (!IsPrimitive(assetPath) && ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", assetPath.c_str());
        }
        ImGui::PopID();
        if (picked) {
            path = assetPath;
        }
        return picked;
    }

    bool ModelList(std::string& path) {
        bool picked = false;
        ImGui::SeparatorText("Primitives");
        for (const std::string& primitive : ModelManager::listPrimitives()) {
            picked |= AssetItem(primitive, path);
        }
        return picked;
    }

    // only textures loaded from a file, embedded and generated ones can't be loaded again by path
    bool TextureList(std::string& path) {
        TextureManager& textureManager = ServiceLocator::GetService<TextureManager>("TextureManagerVulkan");
        std::vector<std::pair<std::string, uint32_t>> textures;
        for (uint32_t id : textureManager.listIDs()) {
            Texture* texture = textureManager.getTexture(id);
            std::error_code error;
            if (texture && std::filesystem::is_regular_file(texture->path(), error)) {
                textures.emplace_back(texture->path(), id);
            }
        }
        std::sort(textures.begin(), textures.end());

        bool picked = false;
        ImGui::SeparatorText("Loaded textures");
        float thumbSize = ImGui::GetTextLineHeight();
        for (const auto& [texturePath, id] : textures) {
            ImGui::Image((ImTextureID)textureManager.inspectTexture(id), ImVec2(thumbSize, thumbSize), ImVec2(0, 1), ImVec2(1, 0));
            ImGui::SameLine();
            picked |= AssetItem(texturePath, path);
        }
        return picked;
    }

    ImGuiID s_editingID = 0;
    std::string s_editBuffer;
}

namespace ImGuiReflection {
    FieldRow::FieldRow(const char* name, const FieldView& meta) : m_disabled(meta.readOnly) {
        ImGui::PushID(name);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        if (meta.widget != FieldWidget::Button) {
            ImGui::TextUnformatted(DisplayName(name));
        }
        if (meta.tooltip && ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", meta.tooltip);
        }
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (m_disabled) {
            ImGui::BeginDisabled();
        }
    }

    FieldRow::~FieldRow() {
        if (m_disabled) {
            ImGui::EndDisabled();
        }
        ImGui::PopID();
    }

    FieldView InferField(const char* owner, const char* name, std::type_index type, const FieldMeta& meta) {
        static std::unordered_map<FieldKey, FieldWidget, FieldKeyHash> cache;
        FieldKey key { owner, name, type };
        auto it = cache.find(key);
        if (it == cache.end()) {
            it = cache.emplace(key, PickWidget(owner, name, type, meta)).first;
        }

        FieldView view(meta, it->second);
        // ids are handles owned by a manager, typing a number into one breaks the reference
        if (type == typeid(uint32_t) && EndsWith(name, { "ID" })) {
            view.readOnly = true;
        }
        return view;
    }

    FieldStyle TextureStyle() {
        return Style<uint32_t>([](const char* name, uint32_t& textureID, const FieldView& view) {
            FieldView shown = view;
            shown.readOnly = false;     // a disabled image is drawn dimmed
            FieldRow row(name, shown);

            TextureManager* textureManager = &ServiceLocator::GetService<TextureManager>("TextureManagerVulkan");
            Texture* texture = textureID != 0 ? textureManager->getTexture(textureID) : nullptr;
            if (!texture || texture->width() == 0) {
                ImGui::TextDisabled("none");
                return false;
            }
            float width = ImGui::GetContentRegionAvail().x;
            float height = width * static_cast<float>(texture->height()) / static_cast<float>(texture->width());
            ImGui::Image((ImTextureID)textureManager->inspectTexture(textureID), ImVec2(width, height));
            return false;
        });
    }

    FieldStyle AxisStyle(float resetValue, const char* label) {
        return Style<glm::vec3>([resetValue, label](const char* name, glm::vec3& value, const FieldView& meta) {
            FieldRow row(label ? label : name, meta);

            static const ImVec4 colors[] = { { 0.8f, 0.1f, 0.15f, 1.0f }, { 0.2f, 0.7f, 0.2f, 1.0f }, { 0.1f, 0.25f, 0.8f, 1.0f } };
            static const ImVec4 hovered[] = { { 0.9f, 0.2f, 0.2f, 1.0f }, { 0.3f, 0.8f, 0.3f, 1.0f }, { 0.2f, 0.35f, 0.9f, 1.0f } };
            static const char* axes[] = { "X", "Y", "Z" };

            float spacing = ImGui::GetStyle().ItemSpacing.x;
            float buttonWidth = ImGui::GetFrameHeight();
            float dragWidth = std::max((ImGui::GetContentRegionAvail().x - spacing * 2.0f) / 3.0f - buttonWidth, 1.0f);

            bool changed = false;
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, ImGui::GetStyle().ItemSpacing.y));
            for (int axis = 0; axis < 3; axis++) {
                ImGui::PushID(axis);
                ImGui::PushStyleColor(ImGuiCol_Button, colors[axis]);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hovered[axis]);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, colors[axis]);
                if (ImGui::Button(axes[axis], ImVec2(buttonWidth, 0.0f))) {
                    value[axis] = resetValue;
                    changed = true;
                }
                ImGui::PopStyleColor(3);
                ImGui::SameLine();
                ImGui::SetNextItemWidth(dragWidth);
                changed |= ImGui::DragFloat("##value", &value[axis], meta.speed, meta.min, meta.max, meta.format ? meta.format : "%.2f");
                if (axis < 2) {
                    ImGui::SameLine(0.0f, spacing);
                }
                ImGui::PopID();
            }
            ImGui::PopStyleVar();
            return changed;
        });
    }

    bool AssetPath(const char* id, std::string& path, FieldWidget kind) {
        ImGui::PushID(id);
        bool changed = false;

        // typed edits apply on enter or when the field loses focus, not every keystroke
        float buttonWidth = ImGui::GetFrameHeight();
        float spacing = ImGui::GetStyle().ItemSpacing.x;
        ImGuiID inputID = ImGui::GetID("##path");
        std::string text = s_editingID == inputID ? s_editBuffer : path;
        ImGui::SetNextItemWidth(std::max(ImGui::GetContentRegionAvail().x - buttonWidth - spacing, 1.0f));
        ImGui::InputText("##path", &text);
        if (ImGui::IsItemActive()) {
            s_editingID = inputID;
            s_editBuffer = text;
        } else if (s_editingID == inputID) {
            s_editingID = 0;
            if (ImGui::IsItemDeactivatedAfterEdit() && text != path) {
                path = text;
                changed = true;
            }
        }
        if (!ImGui::IsItemActive() && ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", path.c_str());
        }

        ImGui::SameLine(0.0f, spacing);
        if (ImGui::Button("...", ImVec2(buttonWidth, 0.0f))) {
            ImGui::OpenPopup("##assets");
        }

        float popupWidth = std::max(ImGui::GetFontSize() * 16.0f, ImGui::GetItemRectMax().x - ImGui::GetCursorScreenPos().x);
        ImGui::SetNextWindowSizeConstraints(ImVec2(popupWidth, 0.0f), ImVec2(FLT_MAX, ImGui::GetFontSize() * 25.0f));
        if (ImGui::BeginPopup("##assets")) {
            if (ImGui::Selectable("Browse...")) {
                std::string picked = Utils::fileDialog();
                if (!picked.empty()) {
                    path = picked;
                    changed = true;
                }
            }
            if (kind != FieldWidget::FilePath && ImGui::Selectable("Clear")) {
                path = "None";
                changed = true;
            }
            if (kind == FieldWidget::ModelPath) {
                changed |= ModelList(path);
            }
            if (kind == FieldWidget::TexturePath) {
                changed |= TextureList(path);
            }
            ImGui::EndPopup();
        }

        ImGui::PopID();
        return changed;
    }

    bool DrawField(const char* name, bool& value, const FieldView& meta) {
        FieldRow row(name, meta);
        if (meta.widget == FieldWidget::Button) {
            bool clicked = ImGui::Button(DisplayName(name), ImVec2(-FLT_MIN, 0.0f));
            value |= clicked;
            return clicked;
        }
        return ImGui::Checkbox("##value", &value);
    }

    bool DrawField(const char* name, int& value, const FieldView& meta) {
        FieldRow row(name, meta);
        if (meta.widget == FieldWidget::Combo && meta.options) {
            return ImGui::Combo("##value", &value, meta.options);
        }
        if (HasRange(meta)) {
            return ImGui::SliderInt("##value", &value, static_cast<int>(meta.min), static_cast<int>(meta.max), "%d", Flags(meta));
        }
        return ImGui::DragInt("##value", &value, std::max(meta.speed, 0.1f), static_cast<int>(meta.min), static_cast<int>(meta.max));
    }

    bool DrawField(const char* name, uint32_t& value, const FieldView& meta) {
        FieldRow row(name, meta);
        uint32_t minValue = static_cast<uint32_t>(meta.min);
        uint32_t maxValue = static_cast<uint32_t>(meta.max);
        if (HasRange(meta)) {
            return ImGui::SliderScalar("##value", ImGuiDataType_U32, &value, &minValue, &maxValue, "%u", Flags(meta));
        }
        return ImGui::DragScalar("##value", ImGuiDataType_U32, &value, std::max(meta.speed, 0.1f), &minValue, HasRange(meta) ? &maxValue : nullptr);
    }

    bool DrawField(const char* name, float& value, const FieldView& meta) {
        FieldRow row(name, meta);
        if (HasRange(meta)) {
            return ImGui::SliderFloat("##value", &value, meta.min, meta.max, Format(meta), Flags(meta));
        }
        return ImGui::DragFloat("##value", &value, meta.speed, meta.min, meta.max, Format(meta), Flags(meta));
    }

    bool DrawField(const char* name, glm::ivec3& value, const FieldView& meta) {
        FieldRow row(name, meta);
        return ImGui::DragInt3("##value", &value[0], std::max(meta.speed, 0.1f), static_cast<int>(meta.min), static_cast<int>(meta.max));
    }

    bool DrawField(const char* name, glm::vec3& value, const FieldView& meta) {
        FieldRow row(name, meta);
        if (meta.widget == FieldWidget::Color) {
            return ImGui::ColorEdit3("##value", &value[0]);
        }
        return ImGui::DragFloat3("##value", &value[0], meta.speed, meta.min, meta.max, Format(meta));
    }

    bool DrawField(const char* name, glm::vec4& value, const FieldView& meta) {
        FieldRow row(name, meta);
        if (meta.widget == FieldWidget::Color) {
            return ImGui::ColorEdit4("##value", &value[0]);
        }
        return ImGui::DragFloat4("##value", &value[0], meta.speed, meta.min, meta.max, Format(meta));
    }

    bool DrawField(const char* name, std::string& value, const FieldView& meta) {
        FieldRow row(name, meta);
        switch (meta.widget) {
            case FieldWidget::FilePath:
            case FieldWidget::ModelPath:
            case FieldWidget::TexturePath:
                return AssetPath("##value", value, meta.widget);
            default:
                return ImGui::InputText("##value", &value);
        }
    }

    void DrawUnsupported(const char* name) {
        FieldRow row(name, FieldMeta{});
        ImGui::TextDisabled("no editor for this type");
    }

    bool BeginFields(const char* id) {
        if (!ImGui::BeginTable(id, 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_PadOuterX)) {
            return false;
        }
        ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 9.0f);
        ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);
        return true;
    }

    void EndFields() {
        ImGui::EndTable();
    }

    // every overload above has to be found by the dispatch in DrawFields, or its fields show as unsupported
    static_assert(HasFieldDrawer<bool>);
    static_assert(HasFieldDrawer<int>);
    static_assert(HasFieldDrawer<uint32_t>);
    static_assert(HasFieldDrawer<float>);
    static_assert(HasFieldDrawer<glm::ivec3>);
    static_assert(HasFieldDrawer<glm::vec3>);
    static_assert(HasFieldDrawer<glm::vec4>);
    static_assert(HasFieldDrawer<std::string>);
}
