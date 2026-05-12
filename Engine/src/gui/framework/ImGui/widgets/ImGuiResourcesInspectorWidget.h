#pragma once

#include <imgui.h>
#include "ImGuiWidget.h"
#include <functional>
#include <imgui.h>

class ImGuiResourceInspectorWidget : public ImGuiWidget 
{
public:
    ImGuiResourceInspectorWidget(std::string name = "ImGuiResourceInspectorWidget");
    ~ImGuiResourceInspectorWidget() override = default;

    virtual void render() override;
    
};