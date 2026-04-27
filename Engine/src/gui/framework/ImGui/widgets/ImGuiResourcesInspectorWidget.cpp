#include "ImGuiResourcesInspectorWidget.h"

ImGuiResourceInspectorWidget::ImGuiResourceInspectorWidget(std::string name)
    : ImGuiWidget(name)
{

}

void ImGuiResourceInspectorWidget::render()
{
    ImGui::Begin("Resource Inspector");

    ImGui::BeginGroup();

    // for(auto id : textureManager->listIDs()) {
    //     ImGui::Text(std::to_string(id).c_str());
	// 	ImGui::Image((ImTextureID)textureManager->inspectTexture(id), ImVec2(256, 144));
    // }

    ImGui::EndGroup();

    ImGui::End();
}