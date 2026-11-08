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
#include "ImGuiParticleInspector.h"
#include "ImGuiScenePicker.h"


ImGuiRightSidebarWidget::ImGuiRightSidebarWidget() 
    :   RightSidebarWidget(),
        popupOpen(false),
        errorPopupOpen(false),
        selectedTexture(0)
{
    setFootprint("ImGuiRightSidebarWidget", sizeof(ImGuiRightSidebarWidget), MemorySystem::Gui);
    m_logger = &ServiceLocator::GetService<Logger>("Engine_LoggerSPD");
    _registerComponents();
}

void ImGuiRightSidebarWidget::_registerComponents()
{
    m_components.add<NameComponent>("Name", { .removable = false });

    m_components.add<TransformComponent>("Transform", {
        .removable = false,
        .styles = {
            { "translateVec", ImGuiReflection::AxisStyle(0.0f, "Translation") },
            { "rotateVec", ImGuiReflection::AxisStyle(0.0f, "Rotation") },
            { "scaleVec", ImGuiReflection::AxisStyle(1.0f, "Scale") },
        },
        .onChanged = [](Entity, TransformComponent& transform) { transform.updateTransform(); },
    });

    m_components.add<PrefabComponent>("Prefab", { .removable = false });

    m_components.add<ModelComponent>("Model", {
        .onFieldChanged = {
            { "path", [](Entity entity, ModelComponent& model) {
                if (model.path == "None") {
                    model.reset();
                    return;
                }
                Entity::_onModelComponentAdded()(entity);
                if (entity.hasComponent<SpriteComponent>()) {
                    Entity::_onSpriteComponentAdded()(entity);
                }
            } },
        },
    });

    m_components.add<MeshComponent>("Mesh", {
        .draw = [](Entity, MeshComponent& mesh) {
            ImGui::Text("%zu meshes", mesh.meshIDs.size());
            return false;
        },
    });

    m_components.add<SpriteComponent>("Sprite", {
        .onFieldChanged = {
            { "path", [](Entity entity, SpriteComponent&) { Entity::_onSpriteComponentAdded()(entity); } },
        },
        .onChanged = [this](Entity entity, SpriteComponent& sprite) {
            sprite.numRows = std::max(sprite.numRows, 1);
            sprite.numCols = std::max(sprite.numCols, 1);
            int numRows = sprite.numRows;
            int numCols = sprite.numCols;
            sprite.frameIndex = std::clamp(sprite.frameIndex, 0, numRows * numCols - 1);
            if (!entity.hasComponent<ModelComponent>()) {
                return;
            }

            // row is y and col is x, the sampled row is flipped
            int currentRow = sprite.frameIndex / numCols;
            int currentCol = sprite.frameIndex % numCols;
            int flippedRow = (numRows - 1) - currentRow;
            sprite.uvScale = { 1.0f / numCols, 1.0f / numRows };
            sprite.uvOffset = { sprite.uvScale.x * currentCol, sprite.uvScale.y * flippedRow };

            Model* model = modelManager->getModel(entity.getComponent<ModelComponent>().modelID);
            Mesh* mesh = (model && !model->meshIDs.empty()) ? meshManager->getMesh(model->meshIDs[0]) : nullptr;
            if (mesh) {
                MaterialDesc material = materialManager->getMaterial(mesh->materialID);
                material.uv = sprite.uvOffset;
                material.uvScale = sprite.uvScale;
                materialManager->updateMaterial(mesh->materialID, material);
            }
        },
    });

    m_components.add<AnimationComponent>("Animation", { .addable = true });
    m_components.add<AnimationStateComponent>("Animation State", { .addable = true });
    m_components.add<SkinningComponent>("Skinning");

    m_components.add<ScriptComponent>("Script", {
        .extras = [](Entity, ScriptComponent& script) {
            if (ImGui::Button("Reload Script", ImVec2(-FLT_MIN, 0.0f))) {
                ServiceLocator::GetService<ScriptManager>("ScriptManager").reloadScript(script.path);
            }
        },
        .onFieldChanged = {
            { "path", [](Entity entity, ScriptComponent& script) {
                Entity::_onScriptComponentRemoved()(entity);
                script.onInit = []() {};
                script.onUpdate = [](double) {};
                script.onFixedUpdate = [](double) {};
                script.onDestroy = []() {};
                script.initilized = false;
                Entity::_onScriptComponentAdded()(entity);
            } },
        },
    });

    m_components.add<ColliderComponent>("Collider", {
        .draw = [](Entity, ColliderComponent& collider) {
            if (!ImGuiReflection::BeginFields("Collider")) {
                return false;
            }
            FieldMeta id { .readOnly = true };
            FieldMeta drag {};
            bool changed = false;
            ImGuiReflection::DrawField("shapeID", collider.shapeID, id);
            changed |= ImGuiReflection::DrawField("mass", collider.mass, drag);
            changed |= ImGuiReflection::DrawField("gravityCenter", collider.center, drag);
            changed |= ImGuiReflection::DrawField("inertiaX", collider.inertia[0], drag);
            changed |= ImGuiReflection::DrawField("inertiaY", collider.inertia[1], drag);
            changed |= ImGuiReflection::DrawField("inertiaZ", collider.inertia[2], drag);
            ImGuiReflection::EndFields();
            return changed;
        },
        .extras = [](Entity, ColliderComponent& collider) {
            PhysicsManager& physicsManager = ServiceLocator::GetService<PhysicsManager>("PhysicsManager");
            float half = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
            if (ImGui::Button("Add Force", ImVec2(half, 0.0f))) {
                physicsManager.addForce(collider.shapeID, { 0.0, 100.0, 0.0 });
            }
            ImGui::SameLine();
            if (ImGui::Button("Impulse", ImVec2(half, 0.0f))) {
                physicsManager.addImpulse(collider.shapeID, { 0.0, 100.0, 0.0 });
            }
        },
        .onChanged = [](Entity, ColliderComponent& collider) {
            MassData data{};
            data.mass = collider.mass;
            data.center = collider.center;
            data.inertia = collider.inertia;
            ServiceLocator::GetService<PhysicsManager>("PhysicsManager").setMass(collider.shapeID, data);
        },
    });

    m_components.add<LightComponent>("Light", { .addable = true });
    m_components.add<CameraComponent>("Camera", { .addable = true });

    m_components.add<LightProbeComponent>("Light Probes", {
        .addable = true,
        .onChanged = [](Entity entity, LightProbeComponent&) { Entity::_onLightProbeComponentAdded()(entity); },
    });

    m_components.add<ParticleEmitter>("Particle", {
        .addable = true,
        .draw = [this](Entity, ParticleEmitter& emitter) {
            ImGuiParticleInspector::draw(emitter, textureManager, modelManager);
            return false;
        },
    });

    m_components.add<RibbonComponent>("Ribbon", {
        .draw = [this](Entity, RibbonComponent& ribbon) {
            ImGuiParticleInspector::drawRibbon(ribbon, textureManager);
            return false;
        },
    });
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

void ImGuiRightSidebarWidget::_errorModal(const char* message) {
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
                            case 0: materialDesc.albedoIDs[0] = textureManager->loadTexture(path, TextureManager::FULL_MIP_CHAIN, false); break;
                            case 1: materialDesc.normalIDs[0] = textureManager->loadTexture(path, TextureManager::FULL_MIP_CHAIN, true); break;
                            case 2: materialDesc.metallicIDs[0] = textureManager->loadTexture(path, TextureManager::FULL_MIP_CHAIN, true); break;
                            case 3: materialDesc.roughnessIDs[0] = textureManager->loadTexture(path, TextureManager::FULL_MIP_CHAIN, true); break;
                            case 4: materialDesc.aoIDs[0] = textureManager->loadTexture(path, TextureManager::FULL_MIP_CHAIN, true); break;
                            case 5: materialDesc.emissiveIDs[0] = textureManager->loadTexture(path, TextureManager::FULL_MIP_CHAIN, false); break;
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
    m_components.draw(entity);

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

        if (!entity.hasComponent<RibbonComponent>() && ImGui::BeginMenu("Ribbon")) {
            const char* modes[] = { "Trail", "Beam", "Lightning", "Blade" };
            for (int mode = 0; mode < IM_ARRAYSIZE(modes); mode++) {
                if (ImGui::Selectable(modes[mode])) {
                    entity.addComponent<RibbonComponent>();
                    RibbonComponent& ribbon = entity.getComponent<RibbonComponent>();
                    ribbon.mode = mode;
                    if (mode == static_cast<int>(RibbonMode::Lightning)) {      // a usable bolt straight away
                        ribbon.widthStart = 0.08f;
                        ribbon.widthEnd = 0.04f;
                        ribbon.colorStart = glm::vec4(2.0f, 2.4f, 4.0f, 1.0f);
                        ribbon.colorEnd = glm::vec4(1.2f, 1.5f, 3.0f, 0.8f);
                    }
                    if (mode == static_cast<int>(RibbonMode::Blade)) {          // a short bright slash, narrowing toward the edge
                        ribbon.widthStart = 1.0f;
                        ribbon.widthEnd = 0.4f;
                        ribbon.colorStart = glm::vec4(2.5f, 2.5f, 3.0f, 1.0f);
                        ribbon.colorEnd = glm::vec4(0.6f, 0.8f, 2.0f, 0.0f);
                        ribbon.trailLifetime = 0.2f;
                        ribbon.minSegmentLength = 0.01f;
                        ribbon.uvMode = static_cast<int>(RibbonUVMode::Stretch);
                    }
                }
            }
            ImGui::EndMenu();
        }

        m_components.drawAddMenu(entity);

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

        // a collider is built from the model's mesh, opening the menu on an entity without one used to crash on getComponent
        bool hasModel = entity.hasComponent<ModelComponent>();
        bool colliderMenuOpen = ImGui::BeginMenu("Collider", hasModel);
        if (!hasModel && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("needs a Model or Mesh component first");
        }
        if (colliderMenuOpen) {
            auto modelManager = &ServiceLocator::GetService<ModelManager>("ModelManager");
            auto meshManager = &ServiceLocator::GetService<MeshManager>("MeshManager");
            auto physicsManager = &ServiceLocator::GetService<PhysicsManager>("PhysicsManager");
            
            TransformComponent& transform = entity.getComponent<TransformComponent>();
            ModelComponent& modelComponent = entity.getComponent<ModelComponent>();
            Model* model = modelManager->getModel(modelComponent.modelID);
            auto combinedMesh = [&]() { return meshManager->combineMeshes(model->meshIDs); };
            if (!model) {
                ImGui::TextDisabled("the model has not loaded yet");
            }

            if (model && ImGui::BeginMenu("Static Body")) {
                if (ImGui::Selectable("Mesh collider")) { 
                    uint32_t type = static_cast<uint32_t>(ColliderType::Static);
                    uint32_t bodyID = physicsManager->createMeshBody(entity, combinedMesh(), transform.translateVec, transform.scaleVec, type);
                    entity.addComponent<ColliderComponent>(bodyID, type);
                    // entity.onColliderComponentAdded();
                }
                ImGui::EndMenu();
            }
            
            if (model && ImGui::BeginMenu("Kinematic Body")) {
                uint32_t type = static_cast<uint32_t>(ColliderType::Kinematic);
                // uint32_t bodyID = physicsManager->createKineticBody(entity, combinedMesh(), transform.translateVec, transform.scaleVec, type);
                // entity.addComponent<ColliderComponent>(bodyID, type);
                // entity.onColliderComponentAdded();
                ImGui::EndMenu();
            }

            if (model && ImGui::BeginMenu("Dynamic Body")) {
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

void ImGuiRightSidebarWidget::_scenesControl()
{
    SceneManager& sceneManager = SceneManager::getInstance();
    Scene* active = sceneManager.getActiveScene();

    auto hint = [](const char* text) {
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", text);
        }
    };

    ImGui::Begin("Scenes Control");

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
