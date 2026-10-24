#include "ImGuiResourcesInspectorWidget.h"
#include <format>
#include <typeindex>
#include "core/features/Texture.h"
#include "src/graphics/renderers/Buffer.h"

ImGuiResourceInspectorWidget::ImGuiResourceInspectorWidget(std::string name)
    : ImGuiWidget(name)
{
    setFootprint("ImGuiResourceInspectorWidget", sizeof(ImGuiResourceInspectorWidget), MemorySystem::Gui);

}

void ImGuiResourceInspectorWidget::render()
{
    if(!m_isVisible) {
        return;
    }
    
    ImGui::Begin("Resource Inspector", &m_isVisible);

    // texture inspector
    if (ImGui::CollapsingHeader("Texture Data", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::BeginTable("##Textures", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 40.0f);
            ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthFixed, 60.0f);
            ImGui::TableHeadersRow();
            
            for (auto id : textureManager->listIDs()) {
                Texture* tex = textureManager->getTexture(id);   
                if (!tex) {
                    continue;
                }

                ImGui::PushID(id);
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                bool selected = selectedTextureID == id;
                if(ImGui::Selectable(std::to_string(id).c_str(), selected, ImGuiSelectableFlags_SpanAllColumns)) {
                    selectedTextureID = id;
                }
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%p", (void*)tex);

                ImGui::TableSetColumnIndex(2);
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text(tex->path().c_str());
                    ImGui::Image((ImTextureID)textureManager->inspectTexture(id), ImVec2(128, 128));
                    ImGui::EndTooltip();
                }
                ImGui::PopID();

            }
            ImGui::EndTable();
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
                if (!buffer) {
                    continue;
                }

                ImGui::PushID(id);
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                bool selected = selectedBufferID == id;
                if(ImGui::Selectable(std::to_string(id).c_str(), selected, ImGuiSelectableFlags_SpanAllColumns)) {
                    selectedBufferID = id;
                }

                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%p", (void*)buffer);

                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text(typeid(*buffer).name());
                    ImGui::EndTooltip();
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }

#if MEMORY_TRACKING_ENABLED
    if (ImGui::CollapsingHeader("Memory Data", ImGuiTreeNodeFlags_DefaultOpen)) {
        MemoryTracker& tracker = MemoryTracker::getInstance();
        if (ImGui::Button("Report Memory")) {
            tracker.reportUsage();
        }

        if (ImGui::BeginTable("##Memory", 7, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("System");
            ImGui::TableSetupColumn("Allocated");
            ImGui::TableSetupColumn("Peak");
            ImGui::TableSetupColumn("Allocs");
            ImGui::TableSetupColumn("Objects");
            ImGui::TableSetupColumn("Peak");
            ImGui::TableSetupColumn("Count");
            ImGui::TableHeadersRow();

            for (size_t i = 0; i < static_cast<size_t>(MemorySystem::Count); i++) {
                MemorySystem system = static_cast<MemorySystem>(i);
                MemoryTracker::SystemStats stats = tracker.getStats(system);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(MemoryTracker::systemName(system));
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%zu", stats.allocatedBytes);
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%zu", stats.peakAllocatedBytes);
                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%zu", stats.allocationCount);
                ImGui::TableSetColumnIndex(4);
                ImGui::Text("%zu", stats.objectBytes);
                ImGui::TableSetColumnIndex(5);
                ImGui::Text("%zu", stats.peakObjectBytes);
                ImGui::TableSetColumnIndex(6);
                ImGui::Text("%zu", stats.objectCount);
            }
            ImGui::EndTable();
        }
    }
#endif

    ImGui::End();
}