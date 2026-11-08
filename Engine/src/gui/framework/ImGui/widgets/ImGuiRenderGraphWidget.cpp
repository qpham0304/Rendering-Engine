#include "ImGuiRenderGraphWidget.h"

#include <imgui.h>

ImGuiRenderGraphWidget::ImGuiRenderGraphWidget(std::string name)
    : ImGuiWidget(name)
{
    setFootprint("ImGuiRenderGraphWidget", sizeof(ImGuiRenderGraphWidget), MemorySystem::Gui);}

void ImGuiRenderGraphWidget::render()
{
    if (!m_isVisible) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(360.0f, 420.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin(getName(), &m_isVisible);

    RenderGraph& graph = rendererManager->getRenderGraph();
    ImGui::Text("Render %ux%u  Output %ux%u",
        graph.getRenderWidth(), graph.getRenderHeight(), graph.getOutputWidth(), graph.getOutputHeight());

    if (ImGui::Button("Enable All")) {
        graph.enableAllPasses();
    }
    ImGui::SameLine();
    m_filter.Draw("##filter", -FLT_MIN);

    ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY;
    if (ImGui::BeginTable("##passes", 4, flags)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("On", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Pass", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Reads", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Writes", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableHeadersRow();

        std::vector<RGPassInfo> passes = graph.getPasses();
        for (size_t i = 0; i < passes.size(); i++) {
            const RGPassInfo& pass = passes[i];
            if (!m_filter.PassFilter(pass.name.c_str())) {
                continue;
            }

            ImGui::TableNextRow();
            ImGui::PushID(static_cast<int>(i));

            ImGui::TableNextColumn();
            bool enabled = pass.enabled;
            if (ImGui::Checkbox("##enabled", &enabled)) {
                graph.setPassEnabled(pass.name, enabled);
            }

            ImGui::BeginDisabled(!pass.enabled);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(pass.name.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%u", pass.readCount);
            ImGui::TableNextColumn();
            ImGui::Text("%u", pass.writeCount);
            ImGui::EndDisabled();

            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    ImGui::End();
}
