#include "ImGuiReflection.h"

#include "core/features/ServiceLocator.h"
#include "core/resources/managers/TextureManager.h"
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

    // label cell, then the value cell with a full width item, readOnly fields are disabled until it goes out of scope
    struct Row {
        bool disabled;

        Row(const char* name, const FieldMeta& meta) : disabled(meta.readOnly) {
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
            if (disabled) {
                ImGui::BeginDisabled();
            }
        }

        ~Row() {
            if (disabled) {
                ImGui::EndDisabled();
            }
            ImGui::PopID();
        }
    };

    bool HasRange(const FieldMeta& meta) { return meta.min < meta.max; }
    const char* Format(const FieldMeta& meta) { return meta.format ? meta.format : "%.3f"; }
    ImGuiSliderFlags Flags(const FieldMeta& meta) { return meta.logarithmic ? ImGuiSliderFlags_Logarithmic : ImGuiSliderFlags_None; }

    // a texture id drawn at the value column width with its own aspect
    void DrawTexture(const char* name, uint32_t textureID, const FieldMeta& meta) {
        FieldMeta shown = meta;
        shown.readOnly = false;     // a disabled image is drawn dimmed
        Row row(name, shown);

        TextureManager* textureManager = &ServiceLocator::GetService<TextureManager>("TextureManagerVulkan");
        Texture* texture = textureID != 0 ? textureManager->getTexture(textureID) : nullptr;
        if (!texture || texture->width() == 0) {
            ImGui::TextDisabled("none");
            return;
        }
        float width = ImGui::GetContentRegionAvail().x;
        float height = width * static_cast<float>(texture->height()) / static_cast<float>(texture->width());
        ImGui::Image((ImTextureID)textureManager->inspectTexture(textureID), ImVec2(width, height));
    }
}

namespace ImGuiReflection {
    bool DrawField(const char* name, bool& value, const FieldMeta& meta) {
        Row row(name, meta);
        if (meta.widget == FieldWidget::Button) {
            bool clicked = ImGui::Button(DisplayName(name), ImVec2(-FLT_MIN, 0.0f));
            value |= clicked;
            return clicked;
        }
        return ImGui::Checkbox("##value", &value);
    }

    bool DrawField(const char* name, int& value, const FieldMeta& meta) {
        Row row(name, meta);
        if (HasRange(meta) && meta.widget != FieldWidget::Drag) {
            return ImGui::SliderInt("##value", &value, static_cast<int>(meta.min), static_cast<int>(meta.max), "%d", Flags(meta));
        }
        return ImGui::DragInt("##value", &value, std::max(meta.speed, 0.1f), static_cast<int>(meta.min), static_cast<int>(meta.max));
    }

    bool DrawField(const char* name, uint32_t& value, const FieldMeta& meta) {
        if (meta.widget == FieldWidget::Texture) {
            DrawTexture(name, value, meta);
            return false;
        }
        Row row(name, meta);
        uint32_t minValue = static_cast<uint32_t>(meta.min);
        uint32_t maxValue = static_cast<uint32_t>(meta.max);
        if (HasRange(meta) && meta.widget != FieldWidget::Drag) {
            return ImGui::SliderScalar("##value", ImGuiDataType_U32, &value, &minValue, &maxValue, "%u", Flags(meta));
        }
        return ImGui::DragScalar("##value", ImGuiDataType_U32, &value, std::max(meta.speed, 0.1f), &minValue, HasRange(meta) ? &maxValue : nullptr);
    }

    bool DrawField(const char* name, float& value, const FieldMeta& meta) {
        Row row(name, meta);
        if (HasRange(meta) && meta.widget != FieldWidget::Drag) {
            return ImGui::SliderFloat("##value", &value, meta.min, meta.max, Format(meta), Flags(meta));
        }
        return ImGui::DragFloat("##value", &value, meta.speed, meta.min, meta.max, Format(meta), Flags(meta));
    }

    bool DrawField(const char* name, glm::ivec3& value, const FieldMeta& meta) {
        Row row(name, meta);
        return ImGui::DragInt3("##value", &value[0], std::max(meta.speed, 0.1f), static_cast<int>(meta.min), static_cast<int>(meta.max));
    }

    bool DrawField(const char* name, glm::vec3& value, const FieldMeta& meta) {
        Row row(name, meta);
        if (meta.widget == FieldWidget::Color) {
            return ImGui::ColorEdit3("##value", &value[0]);
        }
        return ImGui::DragFloat3("##value", &value[0], meta.speed, meta.min, meta.max, Format(meta));
    }

    bool DrawField(const char* name, glm::vec4& value, const FieldMeta& meta) {
        Row row(name, meta);
        if (meta.widget == FieldWidget::Color) {
            return ImGui::ColorEdit4("##value", &value[0]);
        }
        return ImGui::DragFloat4("##value", &value[0], meta.speed, meta.min, meta.max, Format(meta));
    }

    bool DrawField(const char* name, std::string& value, const FieldMeta& meta) {
        Row row(name, meta);
        if (meta.widget != FieldWidget::FilePath) {
            return ImGui::InputText("##value", &value);
        }

        // file name and Browse on one row, the owner loads the new path on its next update
        std::string fileName = std::filesystem::path(value).filename().string();
        float buttonWidth = ImGui::CalcTextSize("Browse").x + ImGui::GetStyle().FramePadding.x * 2.0f;
        ImGui::SetNextItemWidth(std::max(ImGui::GetContentRegionAvail().x - buttonWidth - ImGui::GetStyle().ItemSpacing.x, 1.0f));
        ImGui::BeginDisabled();
        ImGui::InputText("##fileName", &fileName, ImGuiInputTextFlags_ReadOnly);
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("%s", value.c_str());
        }
        ImGui::SameLine();
        if (ImGui::Button("Browse")) {
            std::string path = Utils::fileDialog();
            if (!path.empty()) {
                value = path;
                return true;
            }
        }
        return false;
    }

    void DrawUnsupported(const char* name) {
        Row row(name, FieldMeta{});
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
