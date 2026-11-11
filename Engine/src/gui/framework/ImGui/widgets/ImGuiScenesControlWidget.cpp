#include "ImGuiScenesControlWidget.h"

#include "core/scene/SceneManager.h"
#include "ImGuiScenePicker.h"
#include <imgui.h>

ImGuiScenesControlWidget::ImGuiScenesControlWidget(std::string name)
    : ImGuiWidget(name)
{
    setFootprint("ImGuiScenesControlWidget", sizeof(ImGuiScenesControlWidget), MemorySystem::Gui);
}

void ImGuiScenesControlWidget::render()
{
    if (!m_isVisible) {
        return;
    }

    SceneManager& sceneManager = SceneManager::getInstance();
    Scene* active = sceneManager.getActiveScene();

    auto hint = [](const char* text) {
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", text);
        }
    };

    ImGui::Begin(getName(), &m_isVisible);

    ImGui::SeparatorText("Active Scene");
    ImGui::AlignTextToFramePadding();
    if (active) {
        ImGui::TextUnformatted(active->getName().c_str());
        hint(active->savePath().c_str());
    } else {
        ImGui::TextDisabled("none");
    }

    const ImGuiStyle& style = ImGui::GetStyle();
    float quarter = (ImGui::GetContentRegionAvail().x - style.ItemSpacing.x * 3.0f) * 0.25f;
    if (ImGui::Button("Load...", ImVec2(quarter, 0.0f))) {
        ImGuiScenePicker::open();
    }
    hint("Open Scene (Ctrl+O)");
    ImGui::SameLine();
    ImGui::BeginDisabled(!active);
    if (ImGui::Button("Reload", ImVec2(quarter, 0.0f))) {
        active->requestReload();
    }
    hint("Reload Scene (Ctrl+R)");
    ImGui::SameLine();
    if (ImGui::Button("Save", ImVec2(quarter, 0.0f))) {
        ImGuiScenePicker::saveActiveScene();
    }
    hint("Save Scene (Ctrl+S)");
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Add", ImVec2(quarter, 0.0f))) {
        ImGuiScenePicker::newScene();
    }
    hint("New Scene (Ctrl+N)");

    ImGui::SeparatorText("Scenes");
    std::vector<uint32_t> ids = sceneManager.listIDs();
    bool canRemove = ids.size() > 1;
    for (uint32_t id : ids) {
        Scene* loaded = sceneManager.getScene(id);
        if (!loaded) {
            continue;
        }
        const std::string& name = loaded->getName();
        bool isActive = loaded == active;

        // the header's close button removes the scene like a component, the last one has none
        ImGui::PushID(static_cast<int>(id));
        bool keep = true;
        std::string label = (isActive ? name + "  (active)" : name) + "###scene";
        if (isActive) {
            ImGui::PushStyleColor(ImGuiCol_Header, ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));
        }
        bool open = ImGui::CollapsingHeader(label.c_str(), canRemove ? &keep : nullptr);
        if (isActive) {
            ImGui::PopStyleColor();
        }
        hint(canRemove ? "Right-click for options, x removes the scene (the file is kept)" : "Right-click for options");

        if (ImGui::BeginPopupContextItem("##sceneMenu")) {
            ImGui::TextDisabled("%s", name.c_str());
            ImGui::Separator();
            if (ImGui::MenuItem("Set Active", nullptr, false, !isActive)) {
                sceneManager.setActiveScene(name);
            }
            if (ImGui::MenuItem("Reload")) {
                loaded->requestReload();
            }
            if (ImGui::MenuItem("Save")) {
                loaded->saveScene(loaded->savePath());
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Remove", nullptr, false, canRemove)) {
                keep = false;
            }
            ImGui::EndPopup();
        }

        if (open) {
            ImGui::Indent();
            ImGui::TextDisabled("%s", loaded->getPath().empty() ? "not saved" : loaded->getPath().c_str());
            float third = (ImGui::GetContentRegionAvail().x - style.ItemSpacing.x * 2.0f) / 3.0f;
            ImGui::BeginDisabled(isActive);
            if (ImGui::Button("Set Active", ImVec2(third, 0.0f))) {
                sceneManager.setActiveScene(name);
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button("Reload", ImVec2(third, 0.0f))) {
                loaded->requestReload();
            }
            ImGui::SameLine();
            if (ImGui::Button("Save", ImVec2(third, 0.0f))) {
                loaded->saveScene(loaded->savePath());
            }
            hint(loaded->savePath().c_str());
            ImGui::Unindent();
        }

        if (!keep) {
            sceneManager.removeScene(name);
        }
        ImGui::PopID();
    }
    ImGui::End();

    ImGuiScenePicker::draw();
}
