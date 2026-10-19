#include "ImGuiLeftSidebarWidget.h"
#include <windows.h>
#include <shobjidl.h> 
#include <cfloat>
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

// subtree flags for the hierarchy filter
static constexpr uint8_t SUBTREE_HAS_MESH = 1 << 0;
static constexpr uint8_t SUBTREE_HAS_BONE = 1 << 1;

static const ImVec4 COLOR_BONE  = ImVec4(0.95f, 0.78f, 0.35f, 1.0f);
static const ImVec4 COLOR_MESH  = ImVec4(0.45f, 0.70f, 1.00f, 1.0f);
static const ImVec4 COLOR_NODE  = ImVec4(0.55f, 0.55f, 0.55f, 1.0f);
static const ImVec4 COLOR_MUTED = ImVec4(0.50f, 0.50f, 0.50f, 1.0f);

static uint8_t computeSubtreeFlags(const SkeletonManager::SkeletonAsset& skeleton, uint32_t nodeIndex, std::vector<uint8_t>& flags)
{
    if (nodeIndex >= skeleton.nodes.size()) {
        return 0;
    }

    const auto& node = skeleton.nodes[nodeIndex];
    uint8_t result = 0;
    if (!node.meshIDs.empty()) result |= SUBTREE_HAS_MESH;
    if (node.isBone)           result |= SUBTREE_HAS_BONE;

    for (uint32_t child : node.childrenNodes) {
        result |= computeSubtreeFlags(skeleton, child, flags);
    }

    flags[nodeIndex] = result;
    return result;
}

// shorten text from the front so the file name stays visible
static std::string truncateFront(const std::string& text, float maxWidth)
{
    if (ImGui::CalcTextSize(text.c_str()).x <= maxWidth) {
        return text;
    }

    std::string result = text;
    while (result.size() > 1 && ImGui::CalcTextSize(("..." + result).c_str()).x > maxWidth) {
        result.erase(result.begin());
    }
    return "..." + result;
}

// right align a muted label on the current line, skipped if it would overlap the item
static void rightAlignedHint(const char* text)
{
    float width = ImGui::CalcTextSize(text).x;
    float x = ImGui::GetWindowContentRegionMax().x - width;
    ImGui::SameLine();
    if (ImGui::GetCursorPosX() + ImGui::GetStyle().ItemSpacing.x < x) {
        ImGui::SameLine(x);
        ImGui::TextDisabled("%s", text);
    } else {
        ImGui::NewLine();
    }
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
        ImGui::TextUnformatted(message);
        ImGui::Separator();

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

    std::string filterStr = filterBuffer;
    auto entities = scene->getEntitiesWith<TransformComponent>();

    for (auto& entity : entities) {
        uint32_t currentID = entity.getID();
        NameComponent& nameComponent = entity.getComponent<NameComponent>();

        if (entity.hasComponent<ModelComponent>() && nameComponent.name == "Entity") {
            Model* model = modelManager->getModel(entity.getComponent<ModelComponent>().modelID);
            if (model) {
                nameComponent.name = model->path;
            }
        }

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

        bool isSelected = _IsEntitySelected(scene, entity);
        ImGuiTreeNodeFlags nodeFlags = base_flags | ImGuiTreeNodeFlags_FramePadding;
        if (isSelected) {
            nodeFlags |= ImGuiTreeNodeFlags_Selected;
        }

        const char* icon = entity.hasComponent<LightComponent>() ? ICON_FA_LIGHTBULB
                         : entity.hasComponent<ModelComponent>() ? ICON_FA_CUBES
                         : ICON_FA_DOT_CIRCLE;

        bool treeNodeOpen = ImGui::TreeNodeEx("##entity", nodeFlags, "%s  %s", icon, nameComponent.name.c_str());

        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen() && !isSelected) {
            scene->selectEntities({ entity });
            scene->selectMesh(0);   // the selected mesh belongs to the previous entity
        }

        if (ImGui::BeginPopupContextItem("Add Component")) {
            _RenameMenuItem(entity);
            _DuplicateMenuItem(entity);
            _AddModelMenuItem(entity, "Add Model Async");
            _AddLightMenuItem(entity);
            _AddCameraMenuItem(entity);
            _LoadAnimationMenuItem(entity);
            ImGui::Separator();
            _DeleteEntityMenuItem(entity, scene);
            ImGui::EndPopup();
        }

        // after the context popup, it binds to the last item
        if (entity.hasComponent<SkinningComponent>()) {
            rightAlignedHint(ICON_FA_RUNNING);
        }

        if (treeNodeOpen) {
            if (entity.hasComponent<ModelComponent>()) {
                _ModelDetails(entity, scene);
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }

    ImGui::End();
}

void ImGuiLeftSidebarWidget::_ModelDetails(Entity& entity, Scene* scene)
{
    auto& modelComp = entity.getComponent<ModelComponent>();
    Model* model = modelManager->getModel(modelComp.modelID);
    if (!model) {
        ImGui::TextDisabled("Model not loaded");
        return;
    }

    // path line
    const std::string& fullPath = modelComp.path;
    float availWidth = ImGui::GetContentRegionAvail().x;
    std::string idLabel = "#" + std::to_string(entity.getID());
    float pathWidth = availWidth - ImGui::CalcTextSize(idLabel.c_str()).x - ImGui::GetStyle().ItemSpacing.x * 2;
    std::string displayPath = truncateFront(fullPath, pathWidth);

    ImGui::PushStyleColor(ImGuiCol_Text, COLOR_MUTED);
    ImGui::TextUnformatted(displayPath.c_str());
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", fullPath.c_str());
    }
    rightAlignedHint(idLabel.c_str());

    if (entity.hasComponent<SkinningComponent>()) {
        _AnimationControls(entity);
    }

    auto skeleton = skeletonManager->getSkeleton(model->skeletonID);
    if (!skeleton || skeleton->rootIndex >= skeleton->nodes.size()) {
        // no hierarchy, fall back to the flat mesh list
        for (uint32_t meshID : model->meshIDs) {
            _MeshRow(entity, scene, meshID);
        }
        return;
    }

    _HierarchyFilterBar();

    std::vector<uint8_t> subtreeFlags(skeleton->nodes.size(), 0);
    computeSubtreeFlags(*skeleton, skeleton->rootIndex, subtreeFlags);

    ImGui::PushID(skeleton.get());
    _HierarchyNode(entity, scene, *skeleton, skeleton->rootIndex, subtreeFlags);
    ImGui::PopID();
}

void ImGuiLeftSidebarWidget::_AnimationControls(Entity& entity)
{
    auto& skinningComponent = entity.getComponent<SkinningComponent>();
    const AnimationData* data = animationManager->getAnimationData(skinningComponent.animationPath);
    auto animator = animationManager->getAnimator(skinningComponent.animatorID);
    if (!data || data->empty() || !animator) {
        ImGui::TextDisabled(ICON_FA_RUNNING "  No animation");
        return;
    }

    ImGui::Spacing();
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(COLOR_BONE, ICON_FA_RUNNING);
    ImGui::SameLine();

    const char* preview = animator->animationName.empty() ? "Select animation" : animator->animationName.c_str();
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("##Animation", preview)) {
        for (auto& [name, animation] : *data) {
            const bool isCurrent = (name == animator->animationName);
            if (ImGui::Selectable(name.c_str(), isCurrent)) {
                animator->animationName = name;
                animator->currentTime = 0.0f;
            }
            if (isCurrent) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    auto it = data->find(animator->animationName);
    if (it != data->end() && it->second && it->second->duration > 0.0f) {
        const Animation& animation = *it->second;
        float tps = animation.ticksPerSecond > 0.0f ? animation.ticksPerSecond : 1.0f;
        float progress = animator->currentTime / animation.duration;

        char overlay[64];
        snprintf(overlay, sizeof(overlay), "%.2fs / %.2fs", animator->currentTime / tps, animation.duration / tps);
        // empty overlay, imgui's own overlay text follows the fill edge
        ImGui::ProgressBar(progress, ImVec2(-FLT_MIN, ImGui::GetTextLineHeight()), "");

        // draw the time centered over the whole bar instead
        ImVec2 barMin = ImGui::GetItemRectMin();
        ImVec2 barMax = ImGui::GetItemRectMax();
        ImVec2 textSize = ImGui::CalcTextSize(overlay);
        ImVec2 textPos((barMin.x + barMax.x - textSize.x) * 0.5f, (barMin.y + barMax.y - textSize.y) * 0.5f);
        ImGui::GetWindowDrawList()->AddText(textPos, ImGui::GetColorU32(ImGuiCol_Text), overlay);
    }
    ImGui::Spacing();
}

void ImGuiLeftSidebarWidget::_HierarchyFilterBar()
{
    struct Option { const char* label; HierarchyFilter value; };
    static const Option options[] = {
        { "All",                   HierarchyFilter::All    },
        { ICON_FA_CUBE " Meshes",  HierarchyFilter::Meshes },
        { ICON_FA_BONE " Bones",   HierarchyFilter::Bones  },
    };

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2.0f, ImGui::GetStyle().ItemSpacing.y));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 2.0f));
    float buttonWidth = (ImGui::GetContentRegionAvail().x - 4.0f) / 3.0f;

    for (int i = 0; i < 3; ++i) {
        bool active = (m_hierarchyFilter == options[i].value);
        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        }
        if (ImGui::Button(options[i].label, ImVec2(buttonWidth, 0))) {
            m_hierarchyFilter = options[i].value;
        }
        if (active) {
            ImGui::PopStyleColor();
        }
        if (i < 2) {
            ImGui::SameLine();
        }
    }

    ImGui::PopStyleVar(2);
}

void ImGuiLeftSidebarWidget::_HierarchyNode(Entity& entity, Scene* scene, const SkeletonManager::SkeletonAsset& skeleton, uint32_t nodeIndex, const std::vector<uint8_t>& subtreeFlags)
{
    if (nodeIndex >= skeleton.nodes.size()) {
        return;
    }

    auto isVisible = [&](uint32_t index) {
        switch (m_hierarchyFilter) {
            case HierarchyFilter::Meshes: return (subtreeFlags[index] & SUBTREE_HAS_MESH) != 0;
            case HierarchyFilter::Bones:  return (subtreeFlags[index] & SUBTREE_HAS_BONE) != 0;
            default:                      return true;
        }
    };

    if (!isVisible(nodeIndex)) {
        return;
    }

    const auto& node = skeleton.nodes[nodeIndex];
    const bool showMeshes = m_hierarchyFilter != HierarchyFilter::Bones;

    bool hasVisibleChildren = showMeshes && !node.meshIDs.empty();
    for (uint32_t child : node.childrenNodes) {
        if (hasVisibleChildren) break;
        hasVisibleChildren = isVisible(child);
    }

    ImGuiTreeNodeFlags flags = base_flags;
    if (!hasVisibleChildren) {
        flags |= ImGuiTreeNodeFlags_Leaf;
    }
    if (nodeIndex == skeleton.rootIndex) {
        flags |= ImGuiTreeNodeFlags_DefaultOpen;
    }

    const char* icon = node.isBone ? ICON_FA_BONE : (node.meshIDs.empty() ? ICON_FA_DOT_CIRCLE : ICON_FA_CUBES);
    const ImVec4& iconColor = node.isBone ? COLOR_BONE : (node.meshIDs.empty() ? COLOR_NODE : COLOR_MESH);

    // empty label so the icon can be colored separately, the row still owns the click
    bool open = ImGui::TreeNodeEx((void*)(uintptr_t)nodeIndex, flags, "%s", "");
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::TextUnformatted(node.name.c_str());
        ImGui::TextDisabled("Node %u  |  Parent %d", nodeIndex, node.parentIndex);
        if (node.isBone) {
            ImGui::TextColored(COLOR_BONE, ICON_FA_BONE " Bone");
        }
        if (!node.meshIDs.empty()) {
            ImGui::TextColored(COLOR_MESH, ICON_FA_CUBE " %zu mesh(es)", node.meshIDs.size());
        }
        ImGui::EndTooltip();
    }

    ImGui::SameLine(0.0f, 0.0f);
    ImGui::TextColored(iconColor, "%s", icon);
    ImGui::SameLine();
    ImGui::TextUnformatted(node.name.empty() ? "(unnamed)" : node.name.c_str());

    if (!node.meshIDs.empty() && !open) {
        std::string count = std::to_string(node.meshIDs.size());
        rightAlignedHint(count.c_str());
    }

    if (open) {
        if (showMeshes) {
            for (uint32_t meshID : node.meshIDs) {
                _MeshRow(entity, scene, meshID);
            }
        }
        for (uint32_t child : node.childrenNodes) {
            _HierarchyNode(entity, scene, skeleton, child, subtreeFlags);
        }
        ImGui::TreePop();
    }
}

void ImGuiLeftSidebarWidget::_MeshRow(Entity& entity, Scene* scene, uint32_t meshID)
{
    ImGui::PushID((int)meshID);

    bool selected = scene->getSelectedMeshID() == meshID && _IsEntitySelected(scene, entity);
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf
        | ImGuiTreeNodeFlags_NoTreePushOnOpen
        | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (selected) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    ImGui::TreeNodeEx("##mesh", flags, "%s", "");
    if (ImGui::IsItemClicked()) {
        _SelectMesh(entity, scene, meshID);
    }

    if (ImGui::IsItemHovered()) {
        const Mesh* mesh = meshManager->getMesh(meshID);
        ImGui::BeginTooltip();
        ImGui::Text("Mesh %u", meshID);
        if (mesh) {
            ImGui::TextDisabled("Material %u", mesh->materialID);
            if (!mesh->indices.empty()) {
                ImGui::TextDisabled("%zu triangles", mesh->indices.size() / 3);
            }
        }
        ImGui::EndTooltip();
    }

    ImGui::SameLine(0.0f, 0.0f);
    ImGui::TextColored(COLOR_MESH, ICON_FA_CUBE);
    ImGui::SameLine();
    ImGui::Text("Mesh %u", meshID);

    ImGui::PopID();
}

void ImGuiLeftSidebarWidget::_SelectMesh(Entity& entity, Scene* scene, uint32_t meshID)
{
    if (!_IsEntitySelected(scene, entity)) {
        scene->selectEntities({ entity });
    }
    scene->selectMesh(meshID);
}

bool ImGuiLeftSidebarWidget::_IsEntitySelected(Scene* scene, Entity& entity)
{
    for (const auto& selected : scene->getSelectedEntities()) {
        if (selected.getID() == entity.getID()) {
            return true;
        }
    }
    return false;
}

void ImGuiLeftSidebarWidget::ModelsTab()
{
    SceneManager& sceneManager = sceneManager.getInstance();
    ImGui::Begin("Models Browser");
    for (uint32_t& id : modelManager->listIDs()) {
        Model* model = modelManager->getModel(id);
        if (!model) {
            continue;
        }

        std::string uuid = std::to_string(id);
        ImGui::PushID(uuid.c_str());
        ImGuiTreeNodeFlags node_flags = base_flags | ImGuiTreeNodeFlags_Leaf;

        if (selectedModel == uuid) {
            node_flags |= ImGuiTreeNodeFlags_Selected;
        }

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
    if (ImGui::MenuItem(ICON_FA_TRASH " Delete Entity")) {
        // remove selection first, the right sidebar would otherwise inspect a destroyed mesh
        if (_IsEntitySelected(scene, entity)) {
            scene->selectEntities({});
            scene->selectMesh(0);
        }
        scene->removeEntity(entity.getID());
    }
}
#pragma endregion menu