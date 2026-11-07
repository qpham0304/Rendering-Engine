#pragma once

#include "ImGuiWidget.h"

// the passes the render graph ran last frame, each can be switched off to debug the others alone
class ImGuiRenderGraphWidget : public ImGuiWidget
{
public:
    ImGuiRenderGraphWidget(std::string name = "Render Graph");
    ~ImGuiRenderGraphWidget() override = default;

    virtual void render() override;

private:
    ImGuiTextFilter m_filter;
};
