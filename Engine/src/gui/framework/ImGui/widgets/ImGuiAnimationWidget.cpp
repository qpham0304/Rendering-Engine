#include "ImGuiAnimationWidget.h"
#include "core/features/Math.h"
#include "core/scene/SceneManager.h"
#include "core/features/camera.h"

ImGuiAnimationWidget::ImGuiAnimationWidget(std::string name)
    : ImGuiWidget(name)
{
    
}

void ImGuiAnimationWidget::render()
{
    if(!m_isVisible) {
        return;
    }
    
    ImGui::Begin("Animation Widget", &m_isVisible);

    std::vector<uint32_t> ids = skeletonManager->listIDs();
    auto func = std::function<void(Entity)>([&](Entity entity) -> void {
        auto& nameComponent = entity.getComponent<NameComponent>();
        auto& skinningComponent = entity.getComponent<SkinningComponent>();
        const AnimationData* data = animationManager->getAnimationData(skinningComponent.animationPath);
        if(!data) {
            return;
        }

        ImGui::PushID(entity);

        const bool isEntitySelected = (selectedEntityID == entity);
        if (ImGui::Selectable(nameComponent.name.c_str(), isEntitySelected)) {
            selectedEntityID = entity;
            
            if (skinningComponent.animatorID != 0) {
                auto animator = animationManager->getAnimator(skinningComponent.animatorID);
                if (animator) {
                    selectedAnimation = animator->animationName;
                }
            }
        }

        if (isEntitySelected) {
            auto animator = animationManager->getAnimator(skinningComponent.animatorID);
            
            if (animator && ImGui::BeginCombo("Animations", selectedAnimation.c_str())) {
                for (auto& [name, value] : *data) {
                    const bool isSelected = (name == selectedAnimation);
                    
                    if (ImGui::Selectable(name.c_str(), isSelected)) {
                        selectedAnimation = name;
                        animator->animationName = selectedAnimation;
                    }

                    if (isSelected) {
                        ImGui::SetItemDefaultFocus(); 
                    }
                }
                ImGui::EndCombo();
            }
        }

        ImGui::PopID();
        ImGui::Separator();
    });

    SceneManager& sceneManager = SceneManager::getInstance();
	Scene* scene = sceneManager.getActiveScene();
	if(!scene){
		m_logger->error("ImGuiAnimationWidget: No scene to render");
	}

    scene->forEnitiesWith<SkinningComponent>(func);

    ImGui::Separator();
    // drawBone(ids);
    drawTree(ids);
    
    // if (ImGui::Button("view simple popup")) {
    //     ImGui::OpenPopup("simple popup");
    // }

    // if (ImGui::BeginPopupModal("simple popup", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {

    //     ImGui::Separator();
    //     if (ImGui::Button("Close", ImVec2(120, 0))) {
    //         ImGui::CloseCurrentPopup();
    //     }
    //     ImGui::EndPopup();
    // }

    ImGui::End();
}

void ImGuiAnimationWidget::drawBone(const std::vector<uint32_t> ids) const
{
    auto WorldToScreen = [](const glm::vec3& worldPos, const glm::mat4& viewProj, const ImVec2& windowPos, const ImVec2& windowSize) {
        glm::vec4 clipSpace = viewProj * glm::vec4(worldPos, 1.0f);
        if (clipSpace.w <= 0.0f) return ImVec2(-1, -1);

        glm::vec3 ndc = glm::vec3(clipSpace) / clipSpace.w;
        float x = windowPos.x + (ndc.x + 1.0f) * 0.5f * windowSize.x;
        float y = windowPos.y + (1.0f - ndc.y) * 0.5f * windowSize.y;
        return ImVec2(x, y);
    };

    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    ImVec2 windowPos = ImGui::GetCursorScreenPos();
    ImVec2 windowSize = ImGui::GetContentRegionAvail();

    Camera& camera = *SceneManager::cameraController;
    glm::mat4 viewProj = camera.getProjectionMatrix() * camera.getViewMatrix(); 

    for (auto& id : ids) {
        auto skeleton = skeletonManager->getSkeleton(id);
        if (!skeleton) {
            continue;
        }

        std::function<void(uint32_t, const glm::mat4&)> ComputeNodeWorldPositions = [&](uint32_t nodeIndex, const glm::mat4& parentGlobalMatrix) {
            if (nodeIndex >= skeleton->nodes.size()) return;

            auto& node = skeleton->nodes[nodeIndex];
            glm::mat4 globalMatrix = parentGlobalMatrix * node.vqs.toMat4();
            node.worldPosition = glm::vec3(globalMatrix[3]); 

            for (uint32_t childIndex : node.childrenNodes) {
                ComputeNodeWorldPositions(childIndex, globalMatrix);
            }
        };

        std::function<void(uint32_t)> DrawSkeletonTree = [&](uint32_t nodeIndex) {
            if (nodeIndex >= skeleton->nodes.size()) return;

            auto& node = skeleton->nodes[nodeIndex];
            ImVec2 parentScreen = WorldToScreen(node.worldPosition, viewProj, windowPos, windowSize);

            if (parentScreen.x >= 0) {
                drawList->AddCircleFilled(parentScreen, 4.0f, IM_COL32(255, 0, 0, 255));
            }

            for (uint32_t childIndex : node.childrenNodes) {
                auto& childNode = skeleton->nodes[childIndex];
                ImVec2 childScreen = WorldToScreen(childNode.worldPosition, viewProj, windowPos, windowSize);

                if (parentScreen.x >= 0 && childScreen.x >= 0) {
                    drawList->AddLine(parentScreen, childScreen, IM_COL32(0, 255, 0, 255), 2.0f);
                }

                DrawSkeletonTree(childIndex);
            }
        };

        if (skeleton->rootIndex < skeleton->nodes.size()) {
            ComputeNodeWorldPositions(skeleton->rootIndex, glm::mat4(1.0f));    
            DrawSkeletonTree(skeleton->rootIndex);
        }
    }
}

void ImGuiAnimationWidget::drawTree(const std::vector<uint32_t> ids) const
{
    for (auto& id : ids) {
        auto skeleton = skeletonManager->getSkeleton(id);
        if (!skeleton || skeleton->nodes.empty()) {
            continue;
        }

        if (ImGui::CollapsingHeader(skeleton->name.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Separator();

            std::function<void(uint32_t)> renderNodeTree = [&](uint32_t nodeIndex) {
                if(nodeIndex >= skeleton->nodes.size()) {
                    return;
                }

                auto node = skeleton->nodes[nodeIndex];
                ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
                bool nodeOpen = ImGui::TreeNodeEx((void*)(uintptr_t)nodeIndex, flags, "%s", node.name.c_str());


                std::string isBone = node.isBone ? "bone" : "";
                float textWidth = ImGui::CalcTextSize(isBone.c_str()).x;
                float rightPos = ImGui::GetWindowContentRegionMax().x - textWidth - ImGui::GetStyle().WindowPadding.x;
                ImGui::SameLine(rightPos);
                ImGui::TextDisabled("%s", isBone.c_str());

                if(nodeOpen) {
                    for(auto& childrenIdx : node.childrenNodes) {
                        renderNodeTree(childrenIdx);
                    }

                    ImGui::TreePop();
                }
            };

            if (skeleton->rootIndex < skeleton->nodes.size()) {
                renderNodeTree(skeleton->rootIndex);
            }
        }
    }
}
