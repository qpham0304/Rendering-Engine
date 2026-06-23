#include "ImGuiResourcesInspectorWidget.h"
#include <format>

ImGuiResourceInspectorWidget::ImGuiResourceInspectorWidget(std::string name)
    : ImGuiWidget(name)
{

}

void ImGuiResourceInspectorWidget::render()
{
    if(!m_isVisible) {
        return;
    }
    
    ImGui::Begin("Resource Inspector");

    // texture inspector
    if (ImGui::CollapsingHeader("Texture Data", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::BeginTable("##Textures", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 40.0f);
            ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthFixed, 40.0f);
            ImGui::TableHeadersRow();
            
            for (auto id : textureManager->listIDs()) {
                Texture* tex = textureManager->getTexture(id);   
                if (!tex) {
                    continue;
                }

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%d", id);
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%p", (void*)tex);
                // ImGui::TableSetColumnIndex(2);  // TODO: not all texture are inspectable yet
                // ImGui::Image((ImTextureID)textureManager->inspectTexture(id), ImVec2(32, 32));
            }
            ImGui::EndTable(); // Only called if BeginTable returned true
        }
    }

    // buffer inspector
    if (ImGui::CollapsingHeader("Buffer Data", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::BeginTable("##Buffers", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            
            ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 40.0f);
            ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            for (auto id : bufferManager->listIDs()) {
                Buffer* buffer = bufferManager->getBuffer(id);
                if (!buffer) continue;

                ImGui::TableNextRow();
                
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%d", id);

                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%p", (void*)buffer);
            }
            ImGui::EndTable();
        }
    }

    ImGui::End();
}