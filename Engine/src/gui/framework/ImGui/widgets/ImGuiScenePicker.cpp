#include "ImGuiScenePicker.h"

#include "core/scene/SceneManager.h"
#include "core/features/ServiceLocator.h"
#include "graphics/utils/Utils.h"
#include "logging/Logger.h"
#include <imgui.h>
#include <imgui_stdlib.h>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <vector>

namespace {
    constexpr const char* PopupName = "Open Scene";

    struct SceneEntry {
        std::string name;
        std::string path;
    };

    bool s_openRequested = false;
    int s_lastDrawnFrame = -1;
    std::string s_filter;
    std::string s_selectedName;
    std::string s_selectedPath;
    std::vector<SceneEntry> s_files;

    Logger& logger() { return ServiceLocator::GetService<Logger>("Engine_LoggerSPD"); }

    void RefreshFiles() {
        s_files.clear();
        std::error_code error;
        for (const auto& file : std::filesystem::directory_iterator(Scene::SceneDirectory, error)) {
            if (file.is_regular_file(error) && file.path().extension() == ".json") {
                s_files.push_back({ file.path().stem().string(), file.path().generic_string() });
            }
        }
        std::sort(s_files.begin(), s_files.end(), [](const SceneEntry& a, const SceneEntry& b) { return a.name < b.name; });
    }

    // the scene files plus loaded scenes that were never saved
    std::vector<SceneEntry> Entries() {
        std::vector<SceneEntry> entries = s_files;
        SceneManager& sceneManager = SceneManager::getInstance();
        for (uint32_t id : sceneManager.listIDs()) {
            Scene* scene = sceneManager.getScene(id);
            if (!scene) {
                continue;
            }
            bool listed = std::any_of(entries.begin(), entries.end(), [&](const SceneEntry& entry) { return entry.name == scene->getName(); });
            if (!listed) {
                entries.push_back({ scene->getName(), scene->getPath() });
            }
        }
        return entries;
    }

    bool Matches(const std::string& name) {
        if (s_filter.empty()) {
            return true;
        }
        auto lower = [](unsigned char c) { return static_cast<char>(std::tolower(c)); };
        std::string haystack = name;
        std::string needle = s_filter;
        std::transform(haystack.begin(), haystack.end(), haystack.begin(), lower);
        std::transform(needle.begin(), needle.end(), needle.begin(), lower);
        return haystack.find(needle) != std::string::npos;
    }
}

namespace ImGuiScenePicker {
    void open() {
        s_openRequested = true;
    }

    // can be called from several widgets at the root of the frame, only the first call each frame draws.
    void draw() {
        int frame = ImGui::GetFrameCount();
        if (s_lastDrawnFrame == frame) {
            return;
        }
        s_lastDrawnFrame = frame;

        SceneManager& sceneManager = SceneManager::getInstance();
        Scene* active = sceneManager.getActiveScene();

        if (s_openRequested) {
            s_openRequested = false;
            RefreshFiles();
            s_filter.clear();
            s_selectedName = active ? active->getName() : "";
            s_selectedPath = active ? active->getPath() : "";
            ImGui::OpenPopup(PopupName);
        }

        float fontSize = ImGui::GetFontSize();
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(fontSize * 40.0f, fontSize * 24.0f), ImGuiCond_Appearing);
        if (!ImGui::BeginPopupModal(PopupName, nullptr, ImGuiWindowFlags_NoSavedSettings)) {
            return;
        }

        bool openRequested = false;
        std::string openName;
        std::string openPath;

        if (ImGui::IsWindowAppearing()) {
            ImGui::SetKeyboardFocusHere();
        }
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputTextWithHint("##filter", "Search scenes", &s_filter);

        float footerHeight = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
        ImGuiTableFlags tableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp;
        if (ImGui::BeginTable("##scenes", 3, tableFlags, ImVec2(0.0f, -footerHeight))) {
            ImGui::TableSetupColumn("Scene", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, fontSize * 4.5f);
            ImGui::TableSetupColumn("File", ImGuiTableColumnFlags_WidthStretch, 1.5f);
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableHeadersRow();

            for (const SceneEntry& entry : Entries()) {
                if (!Matches(entry.name)) {
                    continue;
                }
                Scene* loaded = sceneManager.getScene(entry.name);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::PushID(entry.name.c_str());
                bool selected = s_selectedName == entry.name;
                if (ImGui::Selectable(entry.name.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick)) {
                    s_selectedName = entry.name;
                    s_selectedPath = entry.path;
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                        openRequested = true;
                        openName = entry.name;
                        openPath = entry.path;
                    }
                }
                ImGui::PopID();

                ImGui::TableNextColumn();
                if (loaded && loaded == active) {
                    ImGui::TextColored(ImVec4(0.45f, 0.8f, 0.45f, 1.0f), "Active");
                } else if (loaded) {
                    ImGui::TextDisabled("Loaded");
                }

                ImGui::TableNextColumn();
                if (entry.path.empty()) {
                    ImGui::TextDisabled("not saved");
                } else {
                    ImGui::TextDisabled("%s", entry.path.c_str());
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("%s", entry.path.c_str());
                    }
                }
            }
            ImGui::EndTable();
        }

        if (ImGui::Button("Browse...")) {
            std::string path = Utils::fileDialog();
            if (!path.empty()) {
                openRequested = true;
                openName = std::filesystem::path(path).stem().string();
                openPath = path;
            }
        }
        ImGui::SameLine();

        float buttonWidth = fontSize * 5.0f;
        float buttonsWidth = buttonWidth * 2.0f + ImGui::GetStyle().ItemSpacing.x;
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(ImGui::GetContentRegionAvail().x - buttonsWidth, 0.0f));
        if (ImGui::Button("Cancel", ImVec2(buttonWidth, 0.0f)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(s_selectedName.empty());
        if (ImGui::Button("Open", ImVec2(buttonWidth, 0.0f))) {
            openRequested = true;
            openName = s_selectedName;
            openPath = s_selectedPath;
        }
        ImGui::EndDisabled();

        if (openRequested) {
            openScene(openName, openPath);
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    bool openScene(const std::string& name, const std::string& path) {
        SceneManager& sceneManager = SceneManager::getInstance();
        if (sceneManager.getScene(name)) {
            return sceneManager.setActiveScene(name);
        }
        if (path.empty()) {
            return false;
        }

        Scene* previous = sceneManager.getActiveScene();
        std::string previousName = previous ? previous->getName() : "";

        // addScene makes the new scene active straight away
        Scene* scene = sceneManager.addScene(name);
        if (!scene->loadScene(path)) {
            logger().error("Could not open scene {} from {}", name, path);
            sceneManager.removeScene(name);
            if (!previousName.empty()) {
                sceneManager.setActiveScene(previousName);
            }
            return false;
        }
        return sceneManager.setActiveScene(name);
    }

    void newScene() {
        SceneManager& sceneManager = SceneManager::getInstance();
        std::string name = "Untitled";
        for (int i = 1; sceneManager.getScene(name); i++) {
            name = "Untitled " + std::to_string(i);
        }
        sceneManager.addScene(name);
        sceneManager.setActiveScene(name);
    }

    bool saveActiveScene() {
        Scene* scene = SceneManager::getInstance().getActiveScene();
        return scene && scene->saveScene(scene->savePath());
    }
}
