#include "ImGuiLeftSidebarWidget.h"
#include <windows.h>
#include <shobjidl.h> 
#include "Texture.h"
#include "../../src/core/scene/SceneManager.h"
#include "../../src/core/events/EventManager.h"
#include "../../src/core/components/MComponent.h"
#include "../../src/core/components/CameraComponent.h"
#include "../../src/window/appwindow.h"
#include "../../src/graphics/utils/Utils.h"

static ImGuiTreeNodeFlags base_flags = ImGuiTreeNodeFlags_OpenOnArrow
| ImGuiTreeNodeFlags_OpenOnDoubleClick
| ImGuiTreeNodeFlags_SpanAvailWidth;

static void DrawVec3Control(const std::string& label, glm::vec3& values, float resetValue = 0.0f, float columnWidth = 100.0f)
{
    ImGuiIO& io = ImGui::GetIO();
    auto boldFont = io.Fonts->Fonts[0];

    ImGui::PushID(label.c_str());

    ImGui::Columns(2);
    ImGui::SetColumnWidth(0, columnWidth);
    ImGui::Text(label.c_str());
    ImGui::NextColumn();

    //ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0, 0 });

    //float lineHeight = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
    float lineHeight = 30.0f;
    ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.2f, 0.2f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
    ImGui::PushFont(boldFont);
    if (ImGui::Button("X", buttonSize))
        values.x = resetValue;
    ImGui::PopFont();
    ImGui::PopStyleColor(3);

    ImGui::SameLine();
    ImGui::DragFloat("##X", &values.x, 0.1f, 0.0f, 0.0f, "%.2f");
    ImGui::PopItemWidth();
    ImGui::SameLine();

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.3f, 0.8f, 0.3f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
    ImGui::PushFont(boldFont);
    if (ImGui::Button("Y", buttonSize))
        values.y = resetValue;
    ImGui::PopFont();
    ImGui::PopStyleColor(3);

    ImGui::SameLine();
    ImGui::DragFloat("##Y", &values.y, 0.1f, 0.0f, 0.0f, "%.2f");
    ImGui::PopItemWidth();
    ImGui::SameLine();

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.2f, 0.35f, 0.9f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
    ImGui::PushFont(boldFont);
    if (ImGui::Button("Z", buttonSize))
        values.z = resetValue;
    ImGui::PopFont();
    ImGui::PopStyleColor(3);

    ImGui::SameLine();
    ImGui::DragFloat("##Z", &values.z, 0.1f, 0.0f, 0.0f, "%.2f");
    ImGui::PopItemWidth();

    ImGui::PopStyleVar();

    ImGui::Columns(1);

    ImGui::PopID();
}


ImGuiLeftSidebarWidget::ImGuiLeftSidebarWidget() : LeftSidebarWidget()
{
}

void ImGuiLeftSidebarWidget::AddComponentDialog(Entity& entity) {
#if defined(_WIN32)
    std::string path = Utils::fileDialog();
#elif defined(__APPLE__) && defined(__MACH__)
    // macOS specific code
#elif defined(__linux__)
    // Linux specific code
#else
    // Unknown or unsupported platform
#endif


    if (!path.empty()) {
        entity.addComponent<ModelComponent>();
        //NOTE: disable for opengl since it doesn't like buffer generation on a separate thread
#define USE_THREAD        
#ifdef USE_THREAD
        AsyncEvent event(path);
        auto func = [&entity](AsyncEvent& event) {
            printf("unimplemented async event");
        };
        EventManager::getInstance().Queue(event, func);
#else
        ModelLoadEvent event(path, entity);
        EventManager::getInstance().Publish(event);
#endif
        ModelComponent& component = entity.getComponent<ModelComponent>();
        if (component.path == "None") {
            ImGui::OpenPopup("Model loading error");
            errorPopupOpen = true;
        }
    }
}

void ImGuiLeftSidebarWidget::ErrorModal(const char* message) {
    if (ImGui::BeginPopupModal("Model loading error", &errorPopupOpen, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text(message);
        ImGui::Separator();

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
        ImGui::PopStyleVar();

        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - ImGui::GetWindowContentRegionMax().x / 2);
        if (ImGui::Button("OK", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void displayMatrix(glm::mat4& matrix) {
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            ImGui::PushID(row * 4 + col);  // unique ID for each input
            ImGui::PushItemWidth(100.0);
            ImGui::InputFloat(("##m" + std::to_string(row) + std::to_string(col)).c_str(), &matrix[row][col], 0.0f, 0.0f, "%.3f");
            ImGui::PopItemWidth();
            ImGui::PopID();
            if (col < 3) {
                ImGui::SameLine();
            }
        }
    }
}

void ImGuiLeftSidebarWidget::AddItemButton(const std::string&& label) {
    if (ImGui::Button(label.c_str(), ImVec2(-1, 0))) {
        SceneManager::getInstance().getActiveScene()->addEntity();
    }
}

void ImGuiLeftSidebarWidget::LightTab()
{
    SceneManager& sceneManager = sceneManager.getInstance();
    Scene* scene = sceneManager.getActiveScene();
    std::vector<Entity> selectedEntities = scene->getSelectedEntities();
    selectedEntity = &selectedEntities[0];

    ImGui::Begin("Properties");
    if (selectedEntity && selectedEntity->hasComponent<MLightComponent>()) {
        TransformComponent& transform = selectedEntity->getComponent<TransformComponent>();
        MLightComponent& light = selectedEntity->getComponent<MLightComponent>();
        light.position = transform.translateVec;
        if (ImGui::TreeNodeEx(std::to_string(selectedEntity->getID()).c_str(), base_flags)) {
            ImGui::DragFloat3("Color", glm::value_ptr(light.color), 0.5f, 10000.0f, 0);
            ImGui::TreePop();
        }
    }
    ImGui::End();
}

void ImGuiLeftSidebarWidget::EntityTab() {
    ImGui::End();
    SceneManager& sceneManager = sceneManager.getInstance();
    Scene* scene = sceneManager.getActiveScene();

    if (ImGui::Begin("Scenes")) {
        AddItemButton("+ Add Entity");

        Timer timer("component event", true);

        for (auto& [uuid, entity] : scene->entities) {
            ImGuiTreeNodeFlags node_flags = base_flags;
            ImGui::PushID(std::to_string(uuid).c_str());

            std::string name = entity.getComponent<NameComponent>().name;
            if (entity.hasComponent<ModelComponent>() && name == "Entity") {
                auto& model = entity.getComponent<ModelComponent>().model;
                if (std::shared_ptr modelPtr = model.lock()) {
                    name = modelPtr->getFileName();
                }
            }

            std::string addModelTex = "Add Model Async(unavailable on current platform)";

            if (selectedEntity == &entity) {
                node_flags |= ImGuiTreeNodeFlags_Selected;
            }

            bool open = ImGui::TreeNodeEx(name.c_str(), node_flags);
            bool showPopup = ImGui::BeginPopupContextItem("Add Component");
            bool showTextInput = false;

            if (showTextInput) {
                ImGui::PushID(std::to_string(entity.getID()).c_str());
                NameComponent& nameComponent = entity.getComponent<NameComponent>();
                static char str1[128] = "";
                //ImGui::InputTextWithHint("input text (w/ hint)", "enter text here", str1, IM_ARRAYSIZE(str1));
                ImGui::InputText("Edit Text", str1, sizeof(str1));
                nameComponent.name = str1;

                ImGui::InputText("Edit Text", str1, sizeof(str1));

                // Optionally, add a button to confirm and hide the input field
                if (ImGui::Button("Confirm")) {
                    NameComponent& nameComponent = entity.getComponent<NameComponent>();
                    nameComponent.name = str1;
                    showTextInput = false;
                }

                ImGui::SameLine();

                if (ImGui::Button("Cancel")) {
                    showTextInput = false;
                }
                ImGui::PopID();
            }

            if (showPopup) {
                if (ImGui::MenuItem("Rename")) {
                    showTextInput = true;
                }

                if (ImGui::MenuItem("Load Model blocking")) {
                    std::string path = Utils::fileDialog();
                    if (!path.empty()) {
                        ModelLoadEvent event(path, entity);
                        EventManager::getInstance().Publish(event);
                    }
                }

                ImGui::BeginDisabled(true);
                if (ImGui::MenuItem(addModelTex.c_str())) {
                    AddComponentDialog(entity);
                }
                ImGui::EndDisabled();

                if (ImGui::MenuItem("Add Light")) {
                    auto& light = entity.addComponent<MLightComponent>();
                    light.color = glm::vec3(500, 500, 400);
                    light.position = entity.getComponent<TransformComponent>().translateVec;
                    entity.getComponent<NameComponent>().name = "light";
                }

                if (ImGui::MenuItem("Add Camera")) {
                    //light.position = transform.translateVec;
                    TransformComponent& transform = entity.getComponent<TransformComponent>();
                    NameComponent& name = entity.getComponent<NameComponent>();
                    name.name = "camera";
                    entity.addComponent<CameraComponent>(
                        AppWindow::width,
                        AppWindow::height,
                        glm::vec3(transform.translateVec),
                        glm::vec3(0.5, -0.2, -1.0f)
                    );
                    entity.onCameraComponentAdded();    // have entity subscribe to a component added event
                }

                ImGui::BeginDisabled(!entity.hasComponent<ModelComponent>());
                if (ImGui::MenuItem("Load Animation")) {
                    std::string path = Utils::fileDialog();
                    if (!path.empty()) {
                        AnimationLoadEvent event(path, entity);
                        EventManager::getInstance().Publish(event);
                    }
                }
                ImGui::EndDisabled();

                if (ImGui::MenuItem("Delete Entity")) {
                    scene->removeEntity(uuid);
                }

                ImGui::EndPopup();
            }

            if (open) {
                if (entity.hasComponent<ModelComponent>()) {
                    addModelTex = "Change Model";
                    std::string modelPath = "Path: " + entity.getComponent<ModelComponent>().path;
                    ImGui::Text(modelPath.c_str());
                }

                ImGui::Text(std::string("id: " + std::to_string(uuid)).c_str());

                //ImGui::SameLine();
                //if (ImGui::Button(addModelTex.c_str())) {
                //    AddComponentDialog(entity);
                //}

                TransformComponent& transform = entity.getComponent<TransformComponent>();
                glm::mat4 matrix = transform.getModelMatrix();
                displayMatrix(matrix);

                if (ImGui::DragFloat3("Position", glm::value_ptr(transform.translateVec), 0.2f, -20.0f, 20.0f)) {
                    transform.updateTransform();
                }

                if (ImGui::DragFloat3("Scale", glm::value_ptr(transform.scaleVec), 0.2f, -20.0f, 20.0f)) {
                    transform.updateTransform();
                }

                if (ImGui::DragFloat3("Rotation", glm::value_ptr(transform.rotateVec), 0.2f, -180.0f, 180.0f)) {
                    transform.updateTransform();
                }
                ImGui::TreePop();
            }



            if (ImGui::IsItemHovered() && !showPopup) {
                if (ImGui::IsAnyItemHovered()) {
                    ImGui::BeginTooltip();
                    //ImGui::Text(path.c_str());
                    ImGui::EndTooltip();
                }
            }

            // currently support single entity selection
            if (ImGui::IsItemClicked()) {
                scene->selectEntities({ entity });
                Entity ent = scene->getSelectedEntities()[0];
                selectedEntity = &ent;
            }

            ImGui::PopID();
        }
    }
    ImGui::End();
}

void ImGuiLeftSidebarWidget::ModelsTab()
{
    SceneManager& sceneManager = sceneManager.getInstance();

    if (ImGui::Begin("Models Browser")) {
        for (auto [uuid, model] : sceneManager.models) {
            ImGui::PushID(uuid.c_str());
            ImGuiTreeNodeFlags node_flags = base_flags;

            if (selectedModel == uuid) {
                node_flags |= ImGuiTreeNodeFlags_Selected;
            }

            std::string displayPath = (model->getPath().empty() ? uuid : model->getPath());
            bool open = (ImGui::TreeNodeEx(displayPath.c_str(), node_flags));
            bool showPopup = ImGui::BeginPopupContextItem("Add Component");
            if (showPopup) {
                if (ImGui::MenuItem("Copy Path")) {
                    ImGui::SetClipboardText(model->getPath().c_str());
                }

                if (ImGui::MenuItem("Load Model")) {
                    std::string uuid = Utils::fileDialog();
                    if (!uuid.empty()) {
                        sceneManager.addModel(uuid);
                    }
                }

                if (ImGui::MenuItem("Delete Model")) {
                    sceneManager.removeModel(uuid);
                }

                ImGui::EndPopup();
            }

            if (open) {
                ImGui::TreePop();
            }

            if (ImGui::IsItemHovered() && !showPopup) {
                if (ImGui::IsAnyItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text(uuid.c_str());
                    ImGui::EndTooltip();
                }
            }

            if (ImGui::IsItemClicked()) {
                selectedModel = uuid;
            }
        }
        ImGui::PopID();
        ImGui::End();
    }

}

void ImGuiLeftSidebarWidget::render()
{
    Scene* scene = SceneManager::getInstance().getActiveScene();

    ImGui::BeginGroup();
    if (scene) {
        ErrorModal("Error loading Model");
        EntityTab();
        LightTab();
        ModelsTab();
    }
    ImGui::EndGroup();
}
