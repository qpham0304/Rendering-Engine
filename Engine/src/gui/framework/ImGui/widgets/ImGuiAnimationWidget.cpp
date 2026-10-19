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
    // the node hierarchy and animation selection moved to the left sidebar entity tree
    // this widget only keeps the debug bone overlay
    // drawBone(skeletonManager->listIDs());
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
