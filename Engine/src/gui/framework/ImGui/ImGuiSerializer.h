#pragma once

#include <imgui.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <string>

class ImGuiJsonAdapter {
public:
    static bool DrawJsonNode(const std::string& label, nlohmann::json& json) {
        bool modified = false;
        ImGui::PushID(label.c_str());

        if (json.is_object()) {
            if (ImGui::TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                for (auto& [key, value] : json.items()) {
                    modified |= DrawJsonNode(key, value);
                }
                ImGui::TreePop();
            }
        }
        else if (json.is_array()) {
            // Handle spatial vectors like glm::vec2, vec3, vec4
            if (json.size() >= 2 && json.size() <= 4 && 
                std::all_of(json.begin(), json.end(), [](const nlohmann::json& el) { return el.is_number(); })) 
            {
                modified |= DrawVectorControl(label, json);
            } 
            else {
                if (ImGui::TreeNode(label.c_str())) {
                    for (size_t i = 0; i < json.size(); ++i) {
                        modified |= DrawJsonNode("[" + std::to_string(i) + "]", json[i]);
                    }
                    ImGui::TreePop();
                }
            }
        }
        else if (json.is_boolean()) {
            bool val = json.get<bool>();
            if (ImGui::Checkbox(label.c_str(), &val)) {
                json = val;
                modified = true;
            }
        }
        else if (json.is_number_float()) {
            float val = json.get<float>();
            if (ImGui::DragFloat(label.c_str(), &val, 0.1f)) {
                json = val;
                modified = true;
            }
        }
        else if (json.is_number_integer()) {
            int val = json.get<int>();
            if (ImGui::DragInt(label.c_str(), &val, 1.0f)) {
                json = val;
                modified = true;
            }
        }
        else if (json.is_string()) {
            std::string val = json.get<std::string>();
            char buffer[256];
            strncpy(buffer, val.c_str(), sizeof(buffer));
            buffer[sizeof(buffer) - 1] = '\0';

            if (ImGui::InputText(label.c_str(), buffer, sizeof(buffer))) {
                json = std::string(buffer);
                modified = true;
            }
        }

        ImGui::PopID();
        return modified;
    }

private:
    static bool DrawVectorControl(const std::string& label, nlohmann::json& json) {
        bool modified = false;
        size_t size = json.size();

        if (size == 2) {
            float vec[2] = { json[0].get<float>(), json[1].get<float>() };
            if (ImGui::DragFloat2(label.c_str(), vec, 0.1f)) {
                json = { vec[0], vec[1] };
                modified = true;
            }
        } 
        else if (size == 3) {
            float vec[3] = { json[0].get<float>(), json[1].get<float>(), json[2].get<float>() };
            if (ImGui::DragFloat3(label.c_str(), vec, 0.1f)) {
                json = { vec[0], vec[1], vec[2] };
                modified = true;
            }
        }
        else if (size == 4) {
            float vec[4] = { json[0].get<float>(), json[1].get<float>(), json[2].get<float>(), json[3].get<float>() };
            if (ImGui::DragFloat4(label.c_str(), vec, 0.1f)) {
                json = { vec[0], vec[1], vec[2], vec[3] };
                modified = true;
            }
        }
        return modified;
    }
};