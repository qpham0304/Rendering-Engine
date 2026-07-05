#include "ImGuiLeftSidebarWidget.h"
#include <windows.h>
#include <shobjidl.h> 
#include "core/features/Texture.h"
#include "core/scene/SceneManager.h"
#include "core/events/EventManager.h"
#include "core/components/MComponent.h"
#include "window/appwindow.h"
#include "graphics/utils/Utils.h"
#include "core/features/ServiceLocator.h"
#include "logging/Logger.h"
#include "core/resources/managers/modelManager.h"

static ImGuiTreeNodeFlags base_flags = ImGuiTreeNodeFlags_OpenOnArrow
| ImGuiTreeNodeFlags_OpenOnDoubleClick
| ImGuiTreeNodeFlags_SpanAvailWidth;

ImGuiLeftSidebarWidget::ImGuiLeftSidebarWidget() : LeftSidebarWidget()
{
    m_logger = &ServiceLocator::GetService<Logger>("Engine_LoggerSPD");
    modelManager = &ServiceLocator::GetService<ModelManager>("ModelManager");
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
        EventManager::getInstance().queue(event, func);
#else
        ModelLoadEvent event(path, entity);
        EventManager::getInstance().publish(event);
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
    // SceneManager& sceneManager = sceneManager.getInstance();
    // Scene* scene = sceneManager.getActiveScene();
    // std::vector<Entity> selectedEntities = scene->getSelectedEntities();
    // selectedEntity = &selectedEntities[0];

    // ImGui::Begin("Properties");
    // if (selectedEntity && selectedEntity->hasComponent<MLightComponent>()) {
    //     TransformComponent& transform = selectedEntity->getComponent<TransformComponent>();
    //     MLightComponent& light = selectedEntity->getComponent<MLightComponent>();
    //     light.position = transform.translateVec;
    //     if (ImGui::TreeNodeEx(std::to_string(selectedEntity->getID()).c_str(), base_flags)) {
    //         ImGui::DragFloat3("Color", glm::value_ptr(light.color), 0.5f, 10000.0f, 0);
    //         ImGui::TreePop();
    //     }
    // }
    // ImGui::End();
}

void ImGuiLeftSidebarWidget::EntityTab() {
    SceneManager& sceneManager = SceneManager::getInstance();
    Scene* scene = sceneManager.getActiveScene();
    if (!scene) {
        m_logger->warn("No active scene found");
        return;
    }

    ImGui::Begin("Scenes");
    static char filterBuffer[256] = "";
    ImGui::InputTextWithHint("##Search", ICON_FA_SEARCH " Search...", filterBuffer, IM_ARRAYSIZE(filterBuffer));
    ImGui::SameLine();
    AddItemButton("+ Add Entity");
    ImGui::Separator();

    auto selectedEntities = scene->getSelectedEntities();
    auto entities = scene->getEntitiesWith<TransformComponent>();

    for (auto& entity : entities) {
        uint32_t currentID = entity.getID();
        NameComponent& nameComponent = entity.getComponent<NameComponent>();
        std::string filterStr = filterBuffer;

        if (!filterStr.empty()) {
            auto it = std::search(
                nameComponent.name.begin(), nameComponent.name.end(),
                filterStr.begin(), filterStr.end(),
                [](char a, char b) { return std::tolower(a) == std::tolower(b); }
            );
            if (it == nameComponent.name.end()) {
                continue;
            }
        }

        ImGui::PushID(currentID);

        if (entity.hasComponent<ModelComponent>() && nameComponent.name == "Entity") {
            uint32_t modelID = entity.getComponent<ModelComponent>().modelID;
            Model* model = const_cast<Model*>(modelManager->getModel(modelID));
            if (model) {
                nameComponent.name = model->path;
            }
        }

        bool is_selected = false;
        for (const auto& sel : selectedEntities) {
            if (sel.getID() == currentID) {
                is_selected = true;
                break;
            }
        }

        ImGuiTreeNodeFlags node_flags = base_flags | ImGuiTreeNodeFlags_OpenOnArrow;
        if (is_selected) {
            node_flags |= ImGuiTreeNodeFlags_Selected;
        }

        if (is_selected && ImGui::IsMouseDoubleClicked(0) && ImGui::IsItemHovered()) {
            ImGui::SetNextItemOpen(!ImGui::GetStateStorage()->GetBool(ImGui::GetID(nameComponent.name.c_str())));
        }

        bool treeNodeOpen = ImGui::TreeNodeEx(nameComponent.name.c_str(), node_flags);

        if (ImGui::IsItemClicked()) {
            scene->selectEntities({ entity });
        }

        if (ImGui::BeginPopupContextItem("Add Component")) {
            _RenameMenuItem(entity);
            _DuplicateMenuItem(entity);
            _AddModelMenuItem(entity, "Add Model Async");
            _AddLightMenuItem(entity);
            _AddCameraMenuItem(entity);
            _LoadAnimationMenuItem(entity);
            _DeleteEntityMenuItem(entity, scene);
            ImGui::EndPopup();
        }

        if (treeNodeOpen) {
            if (entity.hasComponent<ModelComponent>()) {
                auto& modelComp = entity.getComponent<ModelComponent>();
                Model* model = const_cast<Model*>(modelManager->getModel(modelComp.modelID));

                if (model) {
                    std::string fullPath = modelComp.path;
                    float availWidth = ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x;
                    std::string displayPath = fullPath;

                    if (ImGui::CalcTextSize(fullPath.c_str()).x > availWidth) {
                        while (!displayPath.empty() && ImGui::CalcTextSize((displayPath + "...").c_str()).x > availWidth) {
                            displayPath.pop_back();
                        }
                        displayPath += "...";
                    }
                    
                    ImGui::Separator();
                    ImGui::TextDisabled("ID: %u", currentID);
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
                    ImGui::TextUnformatted(displayPath.c_str());
                    ImGui::PopStyleColor();

                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("%s", fullPath.c_str());
                    }

                    ImGui::Indent(10.0f);
                    for (uint32_t meshID : model->meshIDs) {
                        std::string meshLabel = "Mesh " + std::to_string(meshID);
                        
                        if (ImGui::Selectable(meshLabel.c_str(), false, ImGuiSelectableFlags_SpanAllColumns)) {
                            scene->selectMesh(meshID);
                        }
                        
                    }
                    ImGui::Unindent(10.0f);
                }
            }
            ImGui::Separator();
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
   
    ImGui::End();
}

void ImGuiLeftSidebarWidget::ModelsTab()
{
    SceneManager& sceneManager = sceneManager.getInstance();
    ImGui::Begin("Models Browser");
    for (uint32_t& id : modelManager->listIDs()) {
        std::string uuid = std::to_string(id);
        ImGui::PushID(uuid.c_str());
        ImGuiTreeNodeFlags node_flags = base_flags;

        if (selectedModel == uuid) {
            node_flags |= ImGuiTreeNodeFlags_Selected; 
        }

        Model* model = modelManager->getModel(id);
        std::string displayPath = (model->path.empty() ? uuid : model->path);
        bool open = (ImGui::TreeNodeEx(displayPath.c_str(), node_flags));
        bool showPopup = ImGui::BeginPopupContextItem("Add Component");
        if (showPopup) {
            if (ImGui::MenuItem("Copy Path")) {
                ImGui::SetClipboardText(model->path.c_str());
            }

            if (ImGui::MenuItem("Load Model")) {
                std::string uuid = Utils::fileDialog();
                if (!uuid.empty()) {
                    m_logger->warn("ImGuiLeftSideBar::ModelsTab load model unimplemented");
                }
            }

            if (ImGui::MenuItem("Delete Model")) {
                m_logger->warn("ImGuiLeftSideBar::ModelsTab Delete model unimplemented");
            }

            ImGui::EndPopup();
        }

        if (open) {
            ImGui::TreePop();
        }

        if (ImGui::IsItemHovered() && !showPopup) {
            if (ImGui::IsAnyItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("%s", displayPath.c_str());
                ImGui::EndTooltip();
            }
        }

        if (ImGui::IsItemClicked()) {
            selectedModel = uuid;
        }
        ImGui::PopID();
    }
    ImGui::End();
}

void ImGuiLeftSidebarWidget::MeshesTab()
{
}

void ImGuiLeftSidebarWidget::ScenesTab()
{
}

void ImGuiLeftSidebarWidget::render()
{
    if(!m_isVisible) {
        return;
    }

    Scene* scene = SceneManager::getInstance().getActiveScene();
    if (scene) {
        ImGui::BeginGroup();
        ErrorModal("Error loading Model");
        ScenesTab();
        EntityTab();
        LightTab();
        ModelsTab();
        ImGui::EndGroup();
    }
}

#pragma region menu
void ImGuiLeftSidebarWidget::_EntityTabMenu()
{
}

void ImGuiLeftSidebarWidget::_EntityContent()
{
}

void ImGuiLeftSidebarWidget::_RenameMenuItem(Entity& entity)
{
    if (ImGui::MenuItem("Load Model blocking")) {
        std::string path = Utils::fileDialog();
        if (!path.empty()) {
            ModelLoadEvent event(path, entity);
            EventManager::getInstance().publish(event);
        }
    }
}

void ImGuiLeftSidebarWidget::_DuplicateMenuItem(Entity &entity)
{
    if (ImGui::MenuItem("Duplicate")) {
        Scene* scene = SceneManager::getInstance().getActiveScene();
        if(!scene) {
            return;
        }
        scene->duplicateEntity(entity);
    }
}

void ImGuiLeftSidebarWidget::_AddModelMenuItem(Entity& entity, std::string_view text)
{
    ImGui::BeginDisabled(true);
    if (ImGui::MenuItem(text.data())) {
        AddComponentDialog(entity);
    }
    ImGui::EndDisabled();
}

void ImGuiLeftSidebarWidget::_AddLightMenuItem(Entity& entity)
{
    if (ImGui::MenuItem("Add Light")) {
        entity.getComponent<NameComponent>().name = "light";
        LightComponent& light = entity.addComponent<LightComponent>(glm::vec4(200, 200, 200, 1.0), 15.0f, 1.0f);
    }
}

void ImGuiLeftSidebarWidget::_AddCameraMenuItem(Entity& entity)
{
    if (ImGui::MenuItem("Add Camera")) {
        //light.position = transform.translateVec;
        //TransformComponent& transform = entity.getComponent<TransformComponent>();
        //NameComponent& name = entity.getComponent<NameComponent>();
        //name.name = "camera";
        //entity.addComponent<CameraComponent>(
        //    AppWindow::width,
        //    AppWindow::height,
        //    glm::vec3(transform.translateVec),
        //    glm::vec3(0.5, -0.2, -1.0f)
        //);
        //entity.onCameraComponentAdded();    // have entity subscribe to a component added event
        m_logger->warn("add camera not implemented yet");
    }
}

void ImGuiLeftSidebarWidget::_LoadAnimationMenuItem(Entity &entity)
{
    ImGui::BeginDisabled(!entity.hasComponent<ModelComponent>());
    if (ImGui::MenuItem("Load Animation")) {
        std::string path = Utils::fileDialog();
        if (!path.empty()) {
            AnimationLoadEvent event(path, entity);
            EventManager::getInstance().publish(event);
        }
    }
    ImGui::EndDisabled();
}

void ImGuiLeftSidebarWidget::_DeleteEntityMenuItem(Entity &entity, Scene* scene)
{
    if (ImGui::MenuItem("Delete Entity")) {
        scene->removeEntity(entity.getID());
    }
}
#pragma endregion menu