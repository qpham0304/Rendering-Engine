#pragma once

#include "ImGuiWidget.h"
#include "gui/framework/ImGui/ImGuiReflection.h"

// every settings struct the running renderers registered in GraphicsSettingsSerializer, drawn from its DEFINE_TYPE fields
class ImGuiGraphicsSettingsWidget : public ImGuiWidget
{
public:
    ImGuiGraphicsSettingsWidget(std::string name = "Graphics Settings");
    ~ImGuiGraphicsSettingsWidget() override = default;

    virtual void render() override;

private:
    ImGuiReflectionRegistry m_drawers;
};
