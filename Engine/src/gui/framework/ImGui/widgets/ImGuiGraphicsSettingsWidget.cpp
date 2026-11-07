#include "ImGuiGraphicsSettingsWidget.h"

#include "graphics/utils/GraphicsSettings.h"
#include <imgui.h>

ImGuiGraphicsSettingsWidget::ImGuiGraphicsSettingsWidget(std::string name)
    : ImGuiWidget(name)
{
    setFootprint("ImGuiGraphicsSettingsWidget", sizeof(ImGuiGraphicsSettingsWidget), MemorySystem::Gui);
    // every settings type a renderer can register, only the ones registered at runtime are drawn
    m_drawers.add<BloomSettings>();
    m_drawers.add<ShadowMapSettings>();
    m_drawers.add<LightingSettings>();
    m_drawers.add<AmbientOcclusionSettings>();
    m_drawers.add<SSRSettings>();
    m_drawers.add<SkyAtmosphereSettings>();
    m_drawers.add<ImageBasedSettings>();
    m_drawers.add<DeferredSettings>();
    m_drawers.add<DDGISettings>();
    m_drawers.add<DLSSSettings>();
    m_drawers.add<TonemapSettings>();
    m_drawers.add<TemporalSettings>();
    m_drawers.add<AtrousSettings>();
}

void ImGuiGraphicsSettingsWidget::render()
{
    if (!m_isVisible) {
        return;
    }

    ImGui::Begin(getName(), &m_isVisible);

    GraphicsSettingsSerializer& graphicsSettings = rendererManager->getGraphicsSettings();
    float buttonWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
    if (ImGui::Button("Save", ImVec2(buttonWidth, 0.0f))) {
        graphicsSettings.saveSettings(GraphicsSettingsSerializer::DefaultPath);
    }
    ImGui::SameLine();
    if (ImGui::Button("Load", ImVec2(buttonWidth, 0.0f))) {
        graphicsSettings.loadSettings(GraphicsSettingsSerializer::DefaultPath);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", GraphicsSettingsSerializer::DefaultPath);
    }

    if (rendererManager->getViewTexture() != 0 && ImGui::Button("View Final Image", ImVec2(-FLT_MIN, 0.0f))) {
        rendererManager->setViewTexture(0);
    }
    ImGui::Separator();

    const auto& entries = graphicsSettings.entries();
    if (entries.empty()) {
        ImGui::TextDisabled("no renderer registered settings");
    }

    std::vector<RGPassInfo> passes = rendererManager->getRenderGraph().getPasses();
    for (const auto& entry : entries) {
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding;
        if (!ImGui::TreeNodeEx(entry.name.c_str(), flags)) {
            continue;
        }
        if (m_drawers.contains(entry.type)) {
            m_drawers.draw(entry.type, entry.data.get());
        } else {
            ImGui::TextDisabled("no drawer registered for this settings type");
        }
        _drawOutputs(passes, entry.data.get());
        ImGui::TreePop();
    }

    ImGui::End();
}

// the textures the passes behind these settings wrote last frame, any of them can replace the final image in the viewport
void ImGuiGraphicsSettingsWidget::_drawOutputs(const std::vector<RGPassInfo>& passes, const void* settings)
{
    bool header = false;
    for (const RGPassInfo& pass : passes) {
        if (pass.settings != settings) {
            continue;
        }
        if (!header) {
            ImGui::SeparatorText("Outputs");
            header = true;
        }

        for (const RGTextureInfo& texture : pass.outputs) {
            ImGui::PushID(static_cast<int>(texture.id));
            bool viewing = rendererManager->getViewTexture() == texture.id;

            std::string label = (viewing ? "Hide " : "View ") + texture.name;
            ImGui::BeginDisabled(!texture.viewable);
            if (ImGui::Button(label.c_str(), ImVec2(-FLT_MIN, 0.0f))) {
                rendererManager->setViewTexture(viewing ? 0 : texture.id);
            }
            ImGui::EndDisabled();
            ImGui::PopID();
        }
    }

    if (!header) {
        ImGui::TextDisabled("no pass in the graph this frame");
    }
}
