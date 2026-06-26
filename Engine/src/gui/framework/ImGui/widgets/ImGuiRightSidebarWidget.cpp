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


ImGuiRightSidebarWidget::ImGuiRightSidebarWidget() 
    :   RightSidebarWidget(),
        popupOpen(false),
        errorPopupOpen(false),
        selectedTexture(0)
{

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

void ImGuiRightSidebarWidget::textInput(std::string *text, std::string message)
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

    ImGui::Separator();
    if (ImGui::Button("+ Add Component", ImVec2(-1.0f, 0.0f))) {
        ImGui::OpenPopup("AddComponentPopup"); 
    }

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
            /*
            	EventManager& eventManager = EventManager::getInstance();
                AsyncEvent asyncEvent;
                eventManager.queue(asyncEvent, [this] (AsyncEvent event) {
                    hdrImageID = textureManagerVulkan->loadTexture(
                        "assets/textures/hdr/farm_field_puresky_2k.hdr", 
                        // "assets/textures/hdr/newport_loft.hdr", 
                        1, 
                        false
                    );
                    EventManager& eventManager = EventManager::getInstance();

                    eventManager.publish(event);
                });

                eventManager.subscribe(EventType::AsyncEvent, [](Event& event)) {
                    AsyncEvent& e =
                }
            */

            auto loadModelData = [] (
                Entity& entity,
                Mesh& mesh,
                TextureManager* textureManager,
                MaterialManager* materialManager,
                MeshManager* meshManager,
                ModelManager* modelManager,
                std::string meshType
            ){
                MaterialDesc materialDesc;
                materialDesc.albedoIDs.push_back(
                    textureManager->loadTexture(
                    "assets/textures/mobi-padoru.png",
                    1, 
                    false
                ));
                mesh.materialID = materialManager->createMaterial(materialDesc);

                Model model {};
                model.meshIDs.push_back(meshManager->loadMesh(mesh));
                ModelComponent modelComponent;
                modelComponent.modelID = modelManager->addModel(model);
                modelComponent.path = meshType;
                entity.addComponent<ModelComponent>(modelComponent);
            };

            if (ImGui::Selectable("Quad")) {
                AsyncEvent asyncEvent;
                eventManager.queue(asyncEvent, [&] (AsyncEvent event) {
                    Mesh mesh = EngineUtils::drawQuad();  
                    loadModelData(entity, mesh, textureManager, materialManager, meshManager, modelManager, "$prim$Quad");
                });
            }
            if (ImGui::Selectable("Cube")) {
                Mesh mesh = EngineUtils::drawCube(2.0f);
                loadModelData(entity, mesh, textureManager, materialManager, meshManager, modelManager, "$prim$Quad");
            }
            if (ImGui::Selectable("Sphere")) { 
                Mesh mesh = EngineUtils::drawSphere(0.5f, 36, 36);
                loadModelData(entity, mesh, textureManager, materialManager, meshManager, modelManager, "$prim$Cube");
            }
            
            ImGui::EndMenu();
        }

        if (ImGui::Selectable("Sprite")) {
            entity.addComponent<SpriteComponent>();
            entity.onSpriteComponentAdded();
        }

        if (ImGui::Selectable("Camera")) { 

        }

        if (ImGui::Selectable("Physics")) { 

        }

        if (ImGui::BeginMenu("Scripts")) {
            if (ImGui::Selectable("PlayerController")) { 

            }
            if (ImGui::Selectable("CameraController")) { 

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
    
    SpriteComponent& sprite = entity.getComponent<SpriteComponent>();

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
            if(ImGui::DragInt("##frameIndex", &sprite.frameIndex, 0.1f, 1.0f, 24.0f)) {
                glm::vec2 uvScale = {1.0 / sprite.numRows, 1.0 / sprite.numCols};
                int row = sprite.frameIndex % sprite.numRows;
                int col = sprite.frameIndex % sprite.numCols;
                glm::vec2 uvOffset = {uvScale.x * row, uvScale.y * col};

                ModelComponent& modelComponent = entity.getComponent<ModelComponent>();
                Model* model = modelManager->getModel(modelComponent.modelID);
                Mesh* mesh = meshManager->getMesh(model->meshIDs[0]);
                MaterialDesc material = materialManager->getMaterial(mesh->materialID);
                material.uv = uvOffset;
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

void ImGuiRightSidebarWidget::_scenesControl()
{
    SceneManager& sceneManager = SceneManager::getInstance();
    ImGui::Begin("Scenes Control");
    if(ImGui::Button("Add Scene")) {
        Scene* scene = SceneManager::getInstance().addScene("Level1");
        scene->loadScene("assets/data/level2.json");
        SceneManager::getInstance().setActiveScene(scene->getName());
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
