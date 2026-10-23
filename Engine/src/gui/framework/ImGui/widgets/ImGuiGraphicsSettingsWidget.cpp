#include "ImGuiGraphicsSettingsWidget.h"

#include "graphics/utils/GraphicsSettings.h"
#include <imgui.h>

ImGuiGraphicsSettingsWidget::ImGuiGraphicsSettingsWidget(std::string name)
    : ImGuiWidget(name)
{
    // every settings type a renderer can register, only the ones registered at runtime are drawn
    m_drawers.add<BloomSettings>();
    m_drawers.add<ShadowMapSettings>();
    m_drawers.add<LightingSettings>();
    m_drawers.add<AmbientOcclusionSettings>();
    m_drawers.add<ImageBasedSettings>();
    m_drawers.add<DeferredSettings>();
    m_drawers.add<DDGISettings>();
}

void ImGuiGraphicsSettingsWidget::render()
{
    if (!m_isVisible) {
        return;
    }

    ImGui::Begin(getName(), &m_isVisible);

    const auto& entries = rendererManager->getGraphicsSettings().entries();
    if (entries.empty()) {
        ImGui::TextDisabled("no renderer registered settings");
    }

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
        ImGui::TreePop();
    }

    ImGui::End();
}
