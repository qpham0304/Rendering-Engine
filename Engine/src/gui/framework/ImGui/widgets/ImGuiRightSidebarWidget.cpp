#include "ImGuiRightSidebarWidget.h"

#include "core/scene/SceneManager.h"
#include "core/layers/LayerManager.h"
#include "core/components/MComponent.h"
#include "core/components/CubeMapComponent.h"
#include "graphics/utils/Utils.h"
#include "window/AppWindow.h"
#include "logging/Logger.h"
#include "core/features/ServiceLocator.h"
#include "core/components/MComponent.h"
#include "core/features/EngineUtils.h"
#include "core/events/EventManager.h"
#include "core/resources/managers/ModelManager.h"
#include "scripting/ScriptManager.h"
#include "physics/PhysicsManager.h"
#include <filesystem>


ImGuiRightSidebarWidget::ImGuiRightSidebarWidget() 
    :   RightSidebarWidget(),
        popupOpen(false),
        errorPopupOpen(false),
        selectedTexture(0)
{
    m_logger = &ServiceLocator::GetService<Logger>("Engine_LoggerSPD");

}

void ImGuiRightSidebarWidget::_addModelDialog(Entity& entity) {
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

void ImGuiRightSidebarWidget::errorModal(const char* message) {
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

void ImGuiRightSidebarWidget::textInput(std::string* text, std::string message)
{
    static std::string buffer;
        
    ImGui::Columns(2);
    ImGui::SetColumnWidth(0, 120.0f);
    ImGui::Text(message.c_str());
    ImGui::NextColumn();

    std::string fieldID = "##" + message;
    ImGuiID id = ImGui::GetID(std::string(fieldID + "_State").c_str());
    ImGuiStorage* storage = ImGui::GetStateStorage();
    bool isEditing = storage->GetBool(id, false);

    if (!isEditing) {
        ImGui::TextUnformatted(text->c_str());
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 45);
        
        if (ImGui::Button(std::string("Edit" + fieldID).c_str())) {
            buffer = *text; 
            storage->SetBool(id, true);
        }
    } else {
        ImGui::PushItemWidth(-1);
        bool entered = ImGui::InputText(fieldID.c_str(), &buffer, ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::PopItemWidth();

        if (entered || ImGui::Button(std::string("Save" + fieldID).c_str())) {
            *text = buffer;
            storage->SetBool(id, false);
        }
        
        ImGui::SameLine();
        
        if (ImGui::Button(std::string("Cancel" + fieldID).c_str())) {
            storage->SetBool(id, false);
        }
    }
    ImGui::Columns(1);
}


void ImGuiRightSidebarWidget::environmentControl()
{
    if(!scene) {
        m_logger->warn("could not find active scene");
        return;
    }

    auto list = scene->getEntitiesWith<CubeMapComponent>();
    CubeMapComponent* cubeMap = nullptr;

    if (!list.empty() && list[0].hasComponent<CubeMapComponent>()) {
        cubeMap = &list[0].getComponent<CubeMapComponent>();
    }

    ImGui::Begin("Environment Control");

    ImGui::End();
}

void ImGuiRightSidebarWidget::TextureModal(const ImTextureID& id) {
    //ImVec2 appSize = ImGui::GetIO().DisplaySize;
    //ImVec2 popupSize = ImVec2(appSize.y * 0.75f, appSize.y * 0.75f);
    //ImGui::SetNextWindowSize(popupSize);

    ImGuiStyle& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0, 0.0, 0.0, 0.5);
    if (ImGui::BeginPopupModal("Image View", &popupOpen, ImGuiWindowFlags_AlwaysAutoResize)) {
        //ImVec2 availableSize = ImGui::GetContentRegionAvail();
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImVec2 availableSize = ImVec2(viewport->Size.y * 0.75, viewport->Size.y * 0.75);
        float aspectRatio = 1.0 / 1.0;

        ImVec2 displaySize;
        if (availableSize.x / availableSize.y > aspectRatio) {
            displaySize.y = availableSize.y;
            displaySize.x = availableSize.y * aspectRatio;
        }
        else {
            displaySize.x = availableSize.x;
            displaySize.y = availableSize.x / aspectRatio;
        }

        ImGui::Image(
            (ImTextureID)textureManager->inspectTexture(id), 
            displaySize,
            ImVec2(0, 1),
            ImVec2(1, 0)
        );

        ImGui::Separator();
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
        ImGui::PopStyleVar();

        if (ImGui::IsMouseClicked(0) && !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow)) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void ImGuiRightSidebarWidget::layersControl()
{
    ImGui::Begin("Layers");

    ImGui::End();
}

void ImGuiRightSidebarWidget::textureInspector()
{
    if(!scene) {
        m_logger->warn("could not find active scene");
        return;
    }

    ImGui::Begin("Texture View");
    TextureModal((ImTextureID)selectedTexture);
    static float waveSpeed = 2.0f;
    static float waveAmplitude = 0.5f;
    auto selectedEntities = scene->getSelectedEntities();
    for (auto& entity : selectedEntities) {
        ImVec2 wsize = ImGui::GetWindowSize();
        wsize.x /= 5;
        wsize.y = wsize.x;
        if (scene->getSelectedMeshID() != 0) {
            const Mesh* mesh = meshManager->getMesh(scene->getSelectedMeshID());
            MaterialDesc materialDesc = materialManager->getMaterial(mesh->materialID);

            std::vector<uint32_t> ids = {
                materialDesc.albedoIDs[0],
                materialDesc.normalIDs[0],
                materialDesc.metallicIDs[0],
                materialDesc.roughnessIDs[0],
                materialDesc.aoIDs[0],
                materialDesc.emissiveIDs[0],
            };

            bool changed = false;
            if (ImGui::CollapsingHeader("Material Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.5f); 

                changed |= ImGui::SliderFloat2("UV offset", &materialDesc.uv[0], 0.00f, 10.0f);
                changed |= ImGui::ColorEdit4("Albedo Tint", &materialDesc.albedo[0]);
                // changed |= ImGui::SliderFloat4("Normal", &materialDesc.normal[0], -10.0f, 10.0f);
                changed |= ImGui::SliderFloat("Metallic", &materialDesc.metallic, 0.0f, 1.0f);
                changed |= ImGui::SliderFloat("Roughness", &materialDesc.roughness, 0.0f, 1.0f);
                changed |= ImGui::SliderFloat("AO", &materialDesc.ao, 0.0f, 1.0f);
                changed |= ImGui::DragFloat("Emissive", &materialDesc.emissive, 0.1f, 0.0f, 255.0f);

                changed |= ImGui::SliderFloat("Wave Speed", &waveSpeed, 0.0f, 10.0f);
                changed |= ImGui::SliderFloat("Wave Amplitude", &waveAmplitude, 0.0f, 2.0f);
                // Display the resulting vector (Read-only)
                ImGui::Text("Active Normal: %.2f, %.2f, %.2f", materialDesc.normal.x, materialDesc.normal.y, materialDesc.normal.z);
                ImGui::PopItemWidth();
            }

            ImGui::Separator();
            int i = 0;
            for (auto& id : ids) {
                Texture* texture = textureManager->getTexture(id);
                const char* mapNames[] = { "Albedo", "Normal", "Metallic", "Roughness", "AO", "Emissive" };
                ImGui::PushID(texture->path().c_str());
                ImGui::BeginGroup();
                std::string btnId = "thumb_" + std::to_string(i);
                float thumbSize = ImGui::GetFrameHeight() * 2.0f;
                
                if (ImGui::ImageButton(btnId.c_str(), (ImTextureID)textureManager->inspectTexture(ids[i]), ImVec2(thumbSize, thumbSize), ImVec2(0, 1), ImVec2(1, 0))) {
                    selectedTexture = id;
                    ImGui::OpenPopup("Image View");
                    popupOpen = true;
                }
                ImGui::EndGroup();

                ImGui::SameLine();

                ImGui::BeginGroup();
                ImGui::TextDisabled("%s Map", mapNames[i]);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.8f, 1.0f, 1.0f));

                if (ImGui::Selectable(texture->path().c_str(), false, 0, ImVec2(0, 0))) {
                }

                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip(std::string(std::to_string(id) + " Click change texture source").c_str());
                }

                ImGui::PopStyleColor();
                ImGui::EndGroup();
                ImGui::PopID();
                
                if (ImGui::IsItemClicked()) {
                    // selectedTexture = id;
                    // ImGui::OpenPopup("Image View");
                    // popupOpen = true;
                    
                    std::string path = Utils::fileDialog();
                    if(!path.empty()) {
                        switch (i) {    // assume that there are 6 materials in pbr pipelines
                            case 0: materialDesc.albedoIDs[0] = textureManager->loadTexture(path, 1, false); break;
                            case 1: materialDesc.normalIDs[0] = textureManager->loadTexture(path, 1, true); break;
                            case 2: materialDesc.metallicIDs[0] = textureManager->loadTexture(path, 1, true); break;
                            case 3: materialDesc.roughnessIDs[0] = textureManager->loadTexture(path, 1, true); break;
                            case 4: materialDesc.aoIDs[0] = textureManager->loadTexture(path, 1, true); break;
                            case 5: materialDesc.emissiveIDs[0] = textureManager->loadTexture(path, 1, false); break;
                            default: break;
                        }
                        
                        changed |= true;
                    }
                }
                i++;
            }
            
            if(changed) {
                m_meshesToUpdate.push_back(std::make_pair(mesh, materialDesc));
                // materialManager->updateMaterial(mesh->materialID, materialDesc);
            }

            // float time = static_cast<float>(AppWindow::getTime());
            // materialDesc.uv = glm::vec2(time, time);

            // materialDesc.normal.x = std::sin(time * waveSpeed) * waveAmplitude;
            // materialDesc.normal.y = std::cos(time * waveSpeed) * waveAmplitude;
            // materialDesc.normal.z = 1.0f;

            // m_meshesToUpdate.push_back(std::make_pair(mesh, materialDesc));
        }
    }
    ImGui::End();
}

void ImGuiRightSidebarWidget::render()
{
    if(!m_isVisible) {
        return;
    }

    scene = SceneManager::getInstance().getActiveScene();
    if (scene) {
        ImGui::BeginGroup();
        _componentsControl();
        // layersControl();
        textureInspector();
        environmentControl();
        _scenesControl();
        ImGui::EndGroup();
    }
}

void ImGuiRightSidebarWidget::update()
{
    for(auto& [mesh, material] : m_meshesToUpdate) {
        materialManager->updateMaterial(mesh->materialID, material);
    }
    m_meshesToUpdate.clear();
}

void ImGuiRightSidebarWidget::_listTextureManager()
{
	ImGui::Begin("Textures");

    std::vector<uint32_t> ids = textureManager->listIDs();
    for (auto& id : ids) {
        ImGui::Text("%s", std::to_string(id).c_str());
        ImGui::Begin("Texture View");
        ImGui::BeginChild("Image View");
        ImGui::Image((ImTextureID)textureManager->inspectTexture(id), ImVec2(250, 250));
        ImGui::EndChild();
        ImGui::End();
    }

	ImGui::End();
}

void ImGuiRightSidebarWidget::_componentsControl()
{
    if(!scene) {
        m_logger->warn("could not find active scene");
        return;
    }

    const auto& selectedEntities = scene->getSelectedEntities();
    if(selectedEntities.empty()) {
        return;
    }
    
    // ImGuiWindowClass window_class;  // flag to hide the tab bar
    // window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoTabBar;
    // ImGui::SetNextWindowClass(&window_class);
    
    ImGui::PushStyleColor(ImGuiCol_TabActive, ImGui::GetStyleColorVec4(ImGuiCol_WindowBg)); 
    ImGui::PushStyleColor(ImGuiCol_Tab, ImGui::GetStyleColorVec4(ImGuiCol_WindowBg));
    ImGui::PushStyleColor(ImGuiCol_TabHovered, ImVec4(0.3f, 0.3f, 0.35f, 1.0f)); // Subtle hover
    ImGui::PushStyleColor(ImGuiCol_TabUnfocusedActive, ImGui::GetStyleColorVec4(ImGuiCol_WindowBg));

    Entity& entity = const_cast<Entity&>(selectedEntities[0]);
    ImGui::Begin("Components");
    _nameControl(entity);
    _transformControl(entity);
    _modelControl(entity);
    _meshControl(entity);
    _spriteControl(entity);
    _animationControl(entity);
    _scriptControl(entity);
    _colliderControl(entity);
    _particleControl(entity);

    ImGui::Separator();
    if (ImGui::Button("+ Add Component", ImVec2(-1.0f, 0.0f))) {
        ImGui::OpenPopup("AddComponentPopup"); 
    }

    //TODO: currently manually add every single component to display
    // loop through the serializer object to get the list of components
    // use write an adapter to translate nlohman json to imgui component
    float buttonWidth = ImGui::GetContentRegionAvail().x;
    ImGui::SetNextWindowSizeConstraints(ImVec2(buttonWidth, 0.0f), ImVec2(buttonWidth, 500.0f));
    if (ImGui::BeginPopup("AddComponentPopup")) {
        ImGui::TextDisabled("Select Category");
        ImGui::Separator();

        EventManager& eventManager = EventManager::getInstance();
        if (ImGui::Selectable("Model")) { 
            
            // std::string path = Utils::fileDialog();
            // if (!path.empty()) {
                auto function = [&](AsyncEvent& event) mutable {
                    modelManager = &ServiceLocator::GetService<ModelManager>("ModelManager");
                    modelManager->loadModel("assets/models/reimu/reimu.obj");
                    // entity.addComponent<ModelComponent>(path);
                    // entity.addComponent<ModelComponent>("assets/models/reimu/reimu.obj");
                    // entity.onModelComponentAdded();
                };
                eventManager.queue(asyncE, function);
            // }
        }
        
        if (ImGui::BeginMenu("Mesh")) {
            auto loadModelData = [this] (
                Entity& entity,
                Mesh& mesh,
                std::string meshType
            ){
                std::string path = "assets/textures/mobi-padoru.png"; 
                MaterialDesc materialDesc;
                materialDesc.albedoIDs.push_back(textureManager->loadTexture(path, 1, false));
                mesh.materialID = materialManager->createMaterial(materialDesc);

                Model model {};
                model.meshIDs.push_back(meshManager->loadMesh(mesh));
                // ModelComponent modelComponent;
                // modelComponent.modelID = modelManager->addModel(model);
                // modelComponent.path = meshType;
                entity.addComponent<ModelComponent>(meshType, modelManager->addModel(model));
            };

            if (ImGui::Selectable("Quad")) {
                Mesh mesh = EngineUtils::drawQuad();  
                loadModelData(entity, mesh, "$prim$Quad");
            }
            if (ImGui::Selectable("Cube")) {
                Mesh mesh = EngineUtils::drawCube(2.0f);
                loadModelData(entity, mesh, "$prim$Cube");
            }
            if (ImGui::Selectable("Sphere")) { 
                Mesh mesh = EngineUtils::drawSphere(0.5f, 36, 36);
                loadModelData(entity, mesh, "$prim$Sphere");
            }
            
            ImGui::EndMenu();
        }

        if (ImGui::Selectable("Sprite")) {
            entity.addComponent<SpriteComponent>();
            Entity::_onSpriteComponentAdded()(entity);
        }

        if (ImGui::Selectable("Camera")) { 
            entity.addComponent<CameraComponent>();

            // CameraComponent& cameraComponent = entity.getComponent<CameraComponent>();
            // cameraComponent.viewWidth = AppWindow::getWidth();
            // cameraComponent.viewHeight = AppWindow::getHeight();
            // cameraComponent.projection = projection;
            // cameraComponent.view = entity.getComponent<TransformComponent>().translateVec;
            // cameraComponent.orientation = -cameraTransform.translateVec;
            // cameraEntity.addComponent<CameraComponent>(cameraComponent);
        }

        if (ImGui::BeginMenu("Scripts")) {
            if (ImGui::Selectable("LoadOneTimeScript")) { 
                ScriptManager* scriptManager = &ServiceLocator::GetService<ScriptManager>("ScriptManager");
                std::string path = "assets/scripts/sandbox.lua";
                scriptManager->loadScript(path);
                scriptManager->runScript(path);
            }
            if (ImGui::Selectable("PlayerController")) { 
                entity.addComponent<ScriptComponent>("assets/scripts/Player.lua");
                Entity::_onScriptComponentAdded()(entity);
            }
            if (ImGui::Selectable("CameraController")) { 
                entity.addComponent<ScriptComponent>("assets/scripts/Camera.lua");
                Entity::_onScriptComponentAdded()(entity);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Collider")) {
            auto modelManager = &ServiceLocator::GetService<ModelManager>("ModelManager");
            auto meshManager = &ServiceLocator::GetService<MeshManager>("MeshManager");
            auto physicsManager = &ServiceLocator::GetService<PhysicsManager>("PhysicsManager");
            
            TransformComponent& transform = entity.getComponent<TransformComponent>();
            ModelComponent& modelComponent = entity.getComponent<ModelComponent>();
            Model* model = modelManager->getModel(modelComponent.modelID);
            auto combinedMesh = [&]() { return meshManager->combineMeshes(model->meshIDs); }; 

            if (ImGui::BeginMenu("Static Body")) { 
                if (ImGui::Selectable("Mesh collider")) { 
                    uint32_t type = static_cast<uint32_t>(ColliderType::Static);
                    uint32_t bodyID = physicsManager->createMeshBody(entity, combinedMesh(), transform.translateVec, transform.scaleVec, type);
                    entity.addComponent<ColliderComponent>(bodyID, type);
                    // entity.onColliderComponentAdded();
                }
                ImGui::EndMenu();
            }
            
            if (ImGui::BeginMenu("Kinematic Body")) { 
                uint32_t type = static_cast<uint32_t>(ColliderType::Kinematic);
                // uint32_t bodyID = physicsManager->createKineticBody(entity, combinedMesh(), transform.translateVec, transform.scaleVec, type);
                // entity.addComponent<ColliderComponent>(bodyID, type);
                // entity.onColliderComponentAdded();
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Dynamic Body")) { 
                uint32_t type = static_cast<uint32_t>(ColliderType::Dynamic);
                if (ImGui::Selectable("Box collider")) {
                    uint32_t bodyID = physicsManager->createBoxBody(entity, combinedMesh(), transform.translateVec, transform.scaleVec, type);
                    entity.addComponent<ColliderComponent>(bodyID, type);
                    // entity.onColliderComponentAdded();
                }
                if (ImGui::Selectable("Sphere collider")) {
                    uint32_t bodyID = physicsManager->createSphereBody(entity, combinedMesh(), transform.translateVec, transform.scaleVec, 0.5f, type);
                    entity.addComponent<ColliderComponent>(bodyID, type);
                    // entity.onColliderComponentAdded();
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        ImGui::EndPopup();
    }
    ImGui::End();
    ImGui::PopStyleColor(4);
}

void ImGuiRightSidebarWidget::_nameControl(const Entity& entity)
{
    NameComponent& nameComponent = entity.getComponent<NameComponent>();
    if (ImGui::CollapsingHeader("Name", ImGuiTreeNodeFlags_DefaultOpen)) {
        textInput(&nameComponent.name, "Entity Name");
    }
}

void ImGuiRightSidebarWidget::_transformControl(const Entity& entity)
{
    TransformComponent& transform = entity.getComponent<TransformComponent>();

    auto DrawVec3Control = [](const std::string& label, glm::vec3& values, float resetValue = 0.0f)
    {
        bool changed = false;
        ImGui::PushID(label.c_str());

        ImGui::Columns(2);
        // Increased width to prevent "Translation" from cutting off
        ImGui::SetColumnWidth(0, 120.0f); 
        
        // Align text vertically with the input boxes
        ImGui::AlignTextToFramePadding();
        ImGui::Text("%s", label.c_str());
        ImGui::NextColumn();

        // Calculate available width for the 3 segments
        float totalWidth = ImGui::GetContentRegionAvail().x;
        float itemWidth = (totalWidth - (ImGui::GetStyle().ItemSpacing.x * 2.0f)) / 3.0f;
        
        // Control Spacing: Tighten the gap between the Colored Button and the Value Box
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0, 0 });
        
        auto RenderAxis = [&](const char* axisLabel, float& value, ImVec4 color, ImVec4 hoverColor, bool isLast) {
            ImGui::PushID(axisLabel);
            
            // Button style
            ImGui::PushStyleColor(ImGuiCol_Button, color);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoverColor);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, color);
            
            // The square colored button (X, Y, or Z)
            if (ImGui::Button(axisLabel, ImVec2(25, 0))) {
                value = resetValue;
                changed = true;
            }
            ImGui::PopStyleColor(3);

            ImGui::SameLine();
            
            // The DragFloat
            // Subtract button width from the segment width
            ImGui::SetNextItemWidth(itemWidth - 25.0f);
            if (ImGui::DragFloat("##val", &value, 0.01f, 0.0f, 0.0f, "%.2f")) {
                changed = true;
            }

            // Add a small gap between the X, Y, and Z groups, but NOT after the last one (Z)
            if (!isLast) {
                ImGui::SameLine();
                ImGui::Dummy(ImVec2(5, 0)); // Spacing between groups
                ImGui::SameLine();
            }
            
            ImGui::PopID();
        };

        RenderAxis("X", values.x, { 0.8f, 0.1f, 0.15f, 1.0f }, { 0.9f, 0.2f, 0.2f, 1.0f }, false);
        RenderAxis("Y", values.y, { 0.2f, 0.7f, 0.2f, 1.0f }, { 0.3f, 0.8f, 0.3f, 1.0f }, false);
        RenderAxis("Z", values.z, { 0.1f, 0.25f, 0.8f, 1.0f }, { 0.2f, 0.35f, 0.9f, 1.0f }, true);

        ImGui::PopStyleVar(); // Pop ItemSpacing
        ImGui::Columns(1);
        ImGui::PopID();

        return changed;
    };

    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) 
    {
        glm::vec3 translation = transform.translateVec;
        if (DrawVec3Control("Translation", translation)) 
            transform.translate(translation);

        glm::vec3 rotation = transform.rotateVec;
        if (DrawVec3Control("Rotation", rotation)) 
            transform.rotate(rotation);

        glm::vec3 scale = transform.scaleVec;
        if (DrawVec3Control("Scale", scale, 1.0f)) 
            transform.scale(scale);
    }

}

void ImGuiRightSidebarWidget::_modelControl(const Entity& entity)
{
    if(!entity.hasComponent<ModelComponent>()) {
        return;
    }

    ModelComponent& model = entity.getComponent<ModelComponent>();
    if (ImGui::CollapsingHeader("Model", ImGuiTreeNodeFlags_DefaultOpen)) {

    }
}

void ImGuiRightSidebarWidget::_meshControl(const Entity& entity)
{
    if(!entity.hasComponent<MeshComponent>()) {
        return;
    }
    
    MeshComponent& mesh = entity.getComponent<MeshComponent>();
    if (ImGui::CollapsingHeader("Mesh", ImGuiTreeNodeFlags_DefaultOpen)) {

    }
}

void ImGuiRightSidebarWidget::_spriteControl(const Entity& entity)
{
    if(!entity.hasComponent<SpriteComponent>()) {
        return;
    }
    auto& sprite = entity.getComponent<SpriteComponent>();

    // Create a 2-column table. ImGuiTableFlags_SizingFixedFit makes the left column fit the text.
    if (ImGui::CollapsingHeader("Sprite", ImGuiTreeNodeFlags_DefaultOpen)) {
        textInput(&sprite.path, "path");
        textInput(&sprite.targetRenderer, "renderer");
        
        if (ImGui::BeginTable("SpriteProperties", 2, ImGuiTableFlags_SizingFixedFit)) {
            
            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 120.0f);
            ImGui::TableSetupColumn("Widget", ImGuiTableColumnFlags_WidthStretch);

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("path:");
            ImGui::TableNextColumn(); ImGui::Text("%s", sprite.path.c_str());

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("targetRenderer:");
            ImGui::TableNextColumn(); ImGui::Text("%s", sprite.targetRenderer.c_str());

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("numRows:");
            ImGui::TableNextColumn(); 
            ImGui::SetNextItemWidth(-FLT_MIN); // Makes the widget fill the remaining column space
            ImGui::DragInt("##numRows", &sprite.numRows, 0.1f, 1.0f, 16.0f);

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("numCols:");
            ImGui::TableNextColumn(); 
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::DragInt("##numCols", &sprite.numCols, 0.1f, 1.0f, 16.0f);

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("frameIndex:");
            ImGui::TableNextColumn(); 
            ImGui::SetNextItemWidth(-FLT_MIN);
            int maxFrames = (sprite.numRows * sprite.numCols) - 1;
            if (ImGui::DragInt("##frameIndex", &sprite.frameIndex, 0.1f, 0, maxFrames)) {
                int currentRow = sprite.frameIndex / sprite.numCols;   // row represents y while col represents x 
                int currentCol = sprite.frameIndex % sprite.numCols;   // i.e pixel 1, 2 =  arr[2][1] NOT [arr1][2]
                int flippedRow = (sprite.numRows - 1) - currentRow;    // uv offset sampling need to be flipped also
                sprite.uvScale = {1.0 / sprite.numCols, 1.0 / sprite.numRows};
                sprite.uvOffset = {sprite.uvScale.x * currentCol, sprite.uvScale.y * flippedRow};


                ModelComponent& modelComponent = entity.getComponent<ModelComponent>();
                Model* model = modelManager->getModel(modelComponent.modelID);
                Mesh* mesh = meshManager->getMesh(model->meshIDs[0]);
                MaterialDesc material = materialManager->getMaterial(mesh->materialID);
                material.uv = sprite.uvOffset;
                material.uvScale = sprite.uvScale;
                materialManager->updateMaterial(mesh->materialID, material);
            }

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("color:");
            ImGui::TableNextColumn(); 
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::ColorEdit4("##color", &sprite.color[0]);

            ImGui::EndTable();
        }
    }
}

void ImGuiRightSidebarWidget::_animationControl(const Entity &entity)
{
    //TODO: only support sprite animation now, later add option for 3D skin animation and so on
    if(!entity.hasComponent<AnimationComponent>()) {
        return;
    }

    if (ImGui::CollapsingHeader("Animation", ImGuiTreeNodeFlags_DefaultOpen)) {
        int maxFrameCount = 0;
        if(entity.hasComponent<SpriteComponent>()) {
            auto& sprite = entity.getComponent<SpriteComponent>();
            maxFrameCount = sprite.numRows * sprite.numCols - 1;
        }

        auto& animation = entity.getComponent<AnimationComponent>();
        ImGui::SliderInt("Frame count", &animation.frameCount, 1, maxFrameCount);
        ImGui::DragFloat("Frame duration", &animation.frameDuration, 0.0001, 0.0001, 0.001);
        ImGui::DragFloat("Frame delay", &animation.frameDelay, 0.000, 0.01, 1.0);


        if(entity.hasComponent<RelationshipComponent>()) {
            auto& relationship = entity.getComponent<RelationshipComponent>();
            auto& children = relationship.children;
            // m_logger->error("error has relationship Component");
            // for(auto& child : children) {
            //     ImGui::TableNextRow();
            //     ImGui::TableNextColumn(); ImGui::Text("color:");
            //     ImGui::TableNextColumn(); 
            //     ImGui::SetNextItemWidth(-FLT_MIN);

            //     Entity entity(scene. child)
            //     if(child.has)
            //     ImGui::Text()
            // }
        }
    }
}

void ImGuiRightSidebarWidget::_scriptControl(const Entity &entity)
{
    if(!entity.hasComponent<ScriptComponent>()) {
        return;
    }
    
    ScriptComponent& script = entity.getComponent<ScriptComponent>();
    if (ImGui::CollapsingHeader("Script", ImGuiTreeNodeFlags_DefaultOpen)) {
        textInput(&script.path, "path");
        
        if(ImGui::Button("Reload Script", ImVec2(-1.0f, 0.0f))) {
            ScriptManager* scriptManager = &ServiceLocator::GetService<ScriptManager>("ScriptManager");
            scriptManager->reloadScript(script.path);
        }
    }
}

void ImGuiRightSidebarWidget::_colliderControl(const Entity &entity)
{
    if(!entity.hasComponent<ColliderComponent>()) {
        return;
    }

    auto modelManager = &ServiceLocator::GetService<ModelManager>("ModelManager");
    auto meshManager = &ServiceLocator::GetService<MeshManager>("MeshManager");
    auto physicsManager = &ServiceLocator::GetService<PhysicsManager>("PhysicsManager");
    
    ColliderComponent& collider = entity.getComponent<ColliderComponent>();
    
    bool changed = false;
    if (ImGui::CollapsingHeader("Collider", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text(std::string("collider id: " + std::to_string(collider.shapeID)).c_str());
        if(ImGui::Button("Add Force")) {
            physicsManager->addForce(collider.shapeID, {0.0, 100.0, 0.0});
        }
        ImGui::SameLine();
        if(ImGui::Button("Impulse")) {
            physicsManager->addImpulse(collider.shapeID, {0.0, 100.0, 0.0});
        }
        changed |= ImGui::DragFloat("mass: ", &collider.mass, 0.01f, -25.0f, 25.0f);
        changed |= ImGui::DragFloat3("Gravity Center", &collider.center[0], 0.0f, 0.0f, 10.0f);
        changed |= ImGui::DragFloat3("Inertia x", &collider.inertia[0][0], 0.0f, 0.0f, 10.0f);
        changed |= ImGui::DragFloat3("Inertia y", &collider.inertia[1][0], 0.0f, 0.0f, 10.0f);
        changed |= ImGui::DragFloat3("Inertia z", &collider.inertia[2][0], 0.0f, 0.0f, 10.0f);
    }

    if(changed) {
        MassData data{};
        data.mass = collider.mass;
        data.center = collider.center;
        data.inertia = collider.inertia;
        physicsManager->setMass(collider.shapeID, data);
    }

    // MassData data = physicsManager->getMass(collider.shapeID);
    // m_logger->info("updated mass: {}", data.mass);
    // m_logger->info("updated Gravity Center: x:{}, y:{}, z:{}", data.center.x, data.center.y, data.center.z);

}

void ImGuiRightSidebarWidget::_particleControl(const Entity &entity)
{
    if(!entity.hasComponent<ParticleEmitter>()) {
        return;
    }

    ParticleEmitter& emitter = entity.getComponent<ParticleEmitter>();
    if (!ImGui::CollapsingHeader("Particle", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    ImGui::PushID("ParticleEmitter");

    // every section is a framed header over a label | widget table so the controls line up
    auto BeginSection = [](const char* name, bool defaultOpen) {
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding;
        if (defaultOpen) {
            flags |= ImGuiTreeNodeFlags_DefaultOpen;
        }
        if (!ImGui::TreeNodeEx(name, flags)) {
            return false;
        }
        if (!ImGui::BeginTable(name, 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_PadOuterX)) {
            ImGui::TreePop();
            return false;
        }
        ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 7.5f);
        ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);
        return true;
    };
    auto EndSection = []() {
        ImGui::EndTable();
        ImGui::TreePop();
    };
    // label cell, then leaves the cursor in the value cell with a full width item
    auto Row = [](const char* label, const char* tooltip = nullptr) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        if (tooltip && ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", tooltip);
        }
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
    };
    auto Range = [](const char* id, float& minValue, float& maxValue, float speed, float lower, float upper, const char* format) {
        std::string minFormat = std::string("min ") + format;
        std::string maxFormat = std::string("max ") + format;
        return ImGui::DragFloatRange2(id, &minValue, &maxValue, speed, lower, upper, minFormat.c_str(), maxFormat.c_str(), ImGuiSliderFlags_AlwaysClamp);
    };

    // transport and status
    float buttonWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 2.0f) / 3.0f;
    if (ImGui::Button(emitter.isPlaying ? "Pause" : "Play", ImVec2(buttonWidth, 0.0f))) {
        emitter.isPlaying = !emitter.isPlaying;
    }
    ImGui::SameLine();
    if (ImGui::Button("Restart", ImVec2(buttonWidth, 0.0f))) {
        emitter.resetRequested = true;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("kill every particle and start emitting again");
    }
    ImGui::SameLine();
    if (ImGui::Button("Burst", ImVec2(buttonWidth, 0.0f))) {
        emitter.pendingBurst += std::max(emitter.burstCount, 1);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("spawn Burst count particles now");
    }
    ImGui::TextDisabled("container %u, up to %d particles", emitter.containerID, emitter.maxParticles);
    ImGui::Spacing();

    if (BeginSection("Emission", true)) {
        // resizing recreates the gpu buffers, so the typed value is held aside and applied once the edit is done
        Row("Max particles", "pool size, changing it restarts the emitter");
        ImGuiStorage* storage = ImGui::GetStateStorage();
        int* pendingMaxParticles = storage->GetIntRef(ImGui::GetID("MaxParticlesPending"), emitter.maxParticles);
        ImGui::InputInt("##maxParticles", pendingMaxParticles, 0, 0);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            emitter.maxParticles = std::clamp(*pendingMaxParticles, 1, 1 << 20);
        } else if (!ImGui::IsItemActive()) {
            *pendingMaxParticles = emitter.maxParticles;    // follow changes from scripts or other entities
        }

        Row("Rate", "particles spawned per second");
        ImGui::DragFloat("##rate", &emitter.emitRate, 1.0f, 0.0f, 100000.0f, "%.1f / s");

        Row("Burst", "particles spawned at once, repeated on the interval");
        ImGui::DragInt("##burstCount", &emitter.burstCount, 1.0f, 0, 100000, "%d particles");

        Row("Burst every", "seconds between bursts, 0 only bursts from the Burst button");
        ImGui::DragFloat("##burstInterval", &emitter.burstInterval, 0.01f, 0.0f, 60.0f, emitter.burstInterval > 0.0f ? "%.2f s" : "manual only");

        Row("Lifetime", "seconds each particle lives, picked per particle");
        Range("##lifetime", emitter.lifetimeMin, emitter.lifetimeMax, 0.01f, 0.01f, 60.0f, "%.2f s");

        Row("Space", "Local: particles move with the entity, forces act in its space");
        int space = emitter.localSpace ? 1 : 0;
        if (ImGui::Combo("##space", &space, "World\0Local\0")) {
            emitter.localSpace = space == 1;
        }
        EndSection();
    }

    if (BeginSection("Spawn shape", true)) {
        const char* shapes[] = { "Point", "Box", "Sphere", "Hemisphere", "Cone", "Circle" };
        static_assert(IM_ARRAYSIZE(shapes) == static_cast<int>(ParticleSpawnShape::Count));
        Row("Shape", "where new particles appear, placed and rotated by the entity transform");
        ImGui::Combo("##shape", &emitter.spawnShape, shapes, IM_ARRAYSIZE(shapes));

        ParticleSpawnShape shape = static_cast<ParticleSpawnShape>(emitter.spawnShape);
        if (shape == ParticleSpawnShape::Box) {
            Row("Half extents");
            ImGui::DragFloat3("##extents", &emitter.shapeExtents[0], 0.01f, 0.0f, 1000.0f, "%.2f");
        }
        if (shape == ParticleSpawnShape::Sphere || shape == ParticleSpawnShape::Hemisphere
            || shape == ParticleSpawnShape::Cone || shape == ParticleSpawnShape::Circle) {
            Row("Radius");
            ImGui::DragFloat("##radius", &emitter.shapeRadius, 0.01f, 0.0f, 1000.0f, "%.2f");
        }
        if (shape == ParticleSpawnShape::Cone) {
            Row("Cone angle", "how far the edge of the cone tilts outward");
            ImGui::SliderFloat("##coneAngle", &emitter.shapeConeAngle, 0.0f, 90.0f, "%.1f deg");
        }
        if (shape != ParticleSpawnShape::Point) {
            Row("Spawn on");
            int surface = emitter.emitFromShell ? 1 : 0;
            if (ImGui::Combo("##spawnOn", &surface, "Volume\0Surface\0")) {
                emitter.emitFromShell = surface == 1;
            }
        }
        EndSection();
    }

    if (BeginSection("Velocity", true)) {
        Row("Direction", "Outward pushes each particle away from the shape, Point gives an explosion");
        int outward = emitter.velocityFromShape ? 1 : 0;
        if (ImGui::Combo("##directionMode", &outward, "Fixed direction\0Outward from shape\0")) {
            emitter.velocityFromShape = outward == 1;
        }
        if (!emitter.velocityFromShape) {
            Row("Vector", "in entity space, normalized");
            ImGui::DragFloat3("##direction", &emitter.direction[0], 0.01f, -1.0f, 1.0f, "%.2f");
        }

        Row("Spread", "random cone around the direction");
        ImGui::SliderFloat("##spread", &emitter.spreadAngle, 0.0f, 180.0f, "%.1f deg");

        Row("Speed");
        Range("##speed", emitter.speedMin, emitter.speedMax, 0.05f, 0.0f, 1000.0f, "%.2f");
        EndSection();
    }

    if (BeginSection("Forces", false)) {
        Row("Gravity", "multiplier on world gravity");
        ImGui::DragFloat("##gravity", &emitter.gravityScale, 0.01f, -10.0f, 10.0f, "%.2f x");

        Row("Constant force");
        ImGui::DragFloat3("##force", &emitter.force[0], 0.05f, -100.0f, 100.0f, "%.2f");

        Row("Drag", "slows particles down over time");
        ImGui::DragFloat("##drag", &emitter.drag, 0.01f, 0.0f, 50.0f, "%.2f");

        Row("Turbulence", "strength | frequency of the noise field");
        float half = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
        ImGui::SetNextItemWidth(half);
        ImGui::DragFloat("##turbulence", &emitter.turbulenceStrength, 0.05f, 0.0f, 100.0f, "%.2f");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::DragFloat("##turbulenceFrequency", &emitter.turbulenceFrequency, 0.01f, 0.01f, 20.0f, "freq %.2f");

        Row("Vortex", "swirl around the entity's up axis");
        ImGui::DragFloat("##vortex", &emitter.vortexStrength, 0.05f, -100.0f, 100.0f, "%.2f");

        Row("Attract", "pull toward the entity, negative pushes away");
        ImGui::DragFloat("##attract", &emitter.attractStrength, 0.05f, -100.0f, 100.0f, "%.2f");
        EndSection();
    }

    if (BeginSection("Size & rotation", false)) {
        Row("Size", "picked per particle at spawn");
        Range("##size", emitter.sizeMin, emitter.sizeMax, 0.005f, 0.0f, 100.0f, "%.3f");

        Row("Size at death", "multiplier reached at the end of the lifetime");
        ImGui::DragFloat("##sizeEnd", &emitter.sizeEndScale, 0.01f, 0.0f, 20.0f, "%.2f x");

        Row("Start rotation");
        Range("##rotation", emitter.rotationMin, emitter.rotationMax, 1.0f, -360.0f, 360.0f, "%.0f deg");

        Row("Spin");
        Range("##spin", emitter.angularVelocityMin, emitter.angularVelocityMax, 1.0f, -3600.0f, 3600.0f, "%.0f deg/s");
        EndSection();
    }

    if (BeginSection("Color", false)) {
        ImGuiColorEditFlags colorFlags = ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float | ImGuiColorEditFlags_AlphaBar;
        Row("Start");
        ImGui::ColorEdit4("##colorStart", &emitter.colorStart[0], colorFlags);

        Row("End", "blended toward over the lifetime");
        ImGui::ColorEdit4("##colorEnd", &emitter.colorEnd[0], colorFlags);

        Row("Fade in", "part of the lifetime spent fading in");
        ImGui::SliderFloat("##fadeIn", &emitter.fadeIn, 0.0f, 1.0f, "%.2f of life");

        Row("Fade out", "part of the lifetime spent fading out");
        ImGui::SliderFloat("##fadeOut", &emitter.fadeOut, 0.0f, 1.0f, "%.2f of life");
        EndSection();
    }

    if (BeginSection("Rendering", true)) {
        const char* renderModes[] = { "Billboard", "Billboard (upright)", "Stretched by velocity", "Flat on ground", "Fixed to entity", "Mesh" };
        static_assert(IM_ARRAYSIZE(renderModes) == static_cast<int>(ParticleRenderMode::Count));
        Row("Draw as", "Billboard faces the camera, Fixed to entity keeps the entity's orientation");
        ImGui::Combo("##renderMode", &emitter.renderMode, renderModes, IM_ARRAYSIZE(renderModes));

        const char* blendModes[] = { "Additive (glow)", "Alpha (sorted)" };
        static_assert(IM_ARRAYSIZE(blendModes) == static_cast<int>(ParticleBlendMode::Count));
        Row("Blend", "Additive suits fire and sparks, Alpha suits smoke and debris");
        ImGui::Combo("##blend", &emitter.blendMode, blendModes, IM_ARRAYSIZE(blendModes));

        ParticleRenderMode renderMode = static_cast<ParticleRenderMode>(emitter.renderMode);
        if (renderMode == ParticleRenderMode::VelocityStretched) {
            Row("Stretch", "extra length per unit of speed");
            ImGui::DragFloat("##stretch", &emitter.stretchFactor, 0.005f, 0.0f, 10.0f, "%.3f");
        }

        if (renderMode == ParticleRenderMode::Mesh) {
            const std::string primitivePrefix = "$prim$";
            auto isPrimitive = [&](const std::string& path) { return path.rfind(primitivePrefix, 0) == 0; };
            auto displayName = [&](const std::string& path) {
                return isPrimitive(path) ? path.substr(primitivePrefix.size()) : std::filesystem::path(path).filename().string();
            };

            Row("Mesh", "primitives or any model the scene has loaded, Browse loads a new one");
            float browseWidth = ImGui::CalcTextSize("Browse").x + ImGui::GetStyle().FramePadding.x * 2.0f;
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - browseWidth - ImGui::GetStyle().ItemSpacing.x);
            if (ImGui::BeginCombo("##mesh", displayName(emitter.meshPath).c_str())) {
                auto MeshItem = [&](const std::string& path) {
                    bool selected = emitter.meshPath == path;
                    ImGui::PushID(path.c_str());
                    if (ImGui::Selectable(displayName(path).c_str(), selected)) {
                        emitter.meshPath = path;
                    }
                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                    if (!isPrimitive(path) && ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("%s", path.c_str());
                    }
                    ImGui::PopID();
                };

                ImGui::SeparatorText("Primitives");
                for (const std::string& name : ModelManager::listPrimitives()) {
                    MeshItem(name);
                }
                ImGui::SeparatorText("Loaded models");
                for (const std::string& path : modelManager->listModelPaths()) {
                    if (!isPrimitive(path)) {
                        MeshItem(path);
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::SameLine();
            if (ImGui::Button("Browse##mesh")) {
                std::string path = Utils::fileDialog();
                if (!path.empty()) {
                    emitter.meshPath = path;    // the renderer loads it on its next update
                }
            }
            if (emitter.meshModelID == 0 && emitter.loadedMeshPath == emitter.meshPath) {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.3f, 1.0f), "failed to load, drawing billboards");
            }

            Row("Shading", "0 flat color, 1 fully lit by a fixed light");
            ImGui::SliderFloat("##shading", &emitter.meshShading, 0.0f, 1.0f, "%.2f");

            Row("Double sided", "turn on if the mesh looks inside out or has holes");
            ImGui::Checkbox("##doubleSided", &emitter.meshDoubleSided);
        }
        EndSection();
    }

    if (BeginSection("Texture", true)) {
        ParticleTextureSource source = static_cast<ParticleTextureSource>(emitter.activeTextureSource);
        const char* sourceNames[] = { "none, flat color", "set here", "from the entity's sprite", "from the model's material" };

        Row("Texture", "Browse sets a texture for this emitter, it wins over sprite and model textures");
        float thumbSize = ImGui::GetFrameHeight();
        if (emitter.activeTextureID != 0) {
            ImGui::Image((ImTextureID)textureManager->inspectTexture(emitter.activeTextureID), ImVec2(thumbSize, thumbSize), ImVec2(0, 1), ImVec2(1, 0));
            ImGui::SameLine();
        }
        std::string textureName = emitter.texturePath != "None" ? std::filesystem::path(emitter.texturePath).filename().string() : "(none)";
        float buttonsWidth = ImGui::CalcTextSize("BrowseClear").x + ImGui::GetStyle().FramePadding.x * 4.0f + ImGui::GetStyle().ItemSpacing.x * 2.0f;
        ImGui::SetNextItemWidth(std::max(ImGui::GetContentRegionAvail().x - buttonsWidth, 1.0f));
        ImGui::BeginDisabled();
        ImGui::InputText("##textureName", &textureName, ImGuiInputTextFlags_ReadOnly);
        ImGui::EndDisabled();
        if (emitter.texturePath != "None" && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("%s", emitter.texturePath.c_str());
        }
        ImGui::SameLine();
        if (ImGui::Button("Browse##texture")) {
            std::string path = Utils::fileDialog();
            if (!path.empty()) {
                emitter.texturePath = path;     // the renderer loads it on its next update
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear##texture")) {
            emitter.texturePath = "None";
            emitter.loadedTexturePath = "None";
            emitter.textureID = 0;
        }

        Row("Drawing");
        ImGui::TextDisabled("%s", sourceNames[std::clamp(static_cast<int>(source), 0, IM_ARRAYSIZE(sourceNames) - 1)]);

        if (static_cast<ParticleRenderMode>(emitter.renderMode) == ParticleRenderMode::Mesh) {
            Row("Model texture", "use the model's albedo texture when none is set here");
            ImGui::Checkbox("##useModelTexture", &emitter.useModelTexture);
        }

        Row("Flipbook grid", "columns x rows of the sprite sheet, 1 x 1 for a single image");
        int grid[2] = { emitter.flipbookCols, emitter.flipbookRows };
        if (ImGui::DragInt2("##flipbookGrid", grid, 0.1f, 1, 64)) {
            emitter.flipbookCols = grid[0];
            emitter.flipbookRows = grid[1];
        }

        if (emitter.flipbookCols * emitter.flipbookRows > 1) {
            Row("Flipbook speed", "frames per second, 0 plays the sheet once over each lifetime");
            ImGui::DragFloat("##flipbookFPS", &emitter.flipbookFPS, 0.1f, 0.0f, 120.0f, emitter.flipbookFPS > 0.0f ? "%.1f fps" : "once per life");
        }
        EndSection();
    }

    ImGui::PopID();
}

void ImGuiRightSidebarWidget::_scenesControl()
{
    SceneManager& sceneManager = SceneManager::getInstance();
    ImGui::Begin("Scenes Control");
    if(ImGui::Button("Add Scene")) {
        // Scene* scene = SceneManager::getInstance().addScene("Level1");
        // scene->loadScene("assets/data/level2.json");
        // SceneManager::getInstance().setActiveScene(scene->getName());
    }

    for(uint32_t id : sceneManager.listIDs()) {
        Scene* scene = sceneManager.getScene(id);
        if(!scene){
            continue;
        }

        bool isSelected = sceneManager.getActiveScene()->getName() == scene->getName();
        if(ImGui::Selectable(scene->getName().c_str(), isSelected, 0, ImVec2(0, 0))) {
            sceneManager.setActiveScene(scene->getName());
            // scene->reloadScene();
        }
    }
    ImGui::End();

}
