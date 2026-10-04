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
    
private:
    uint32_t selectedTextureID { 0 };
    uint32_t selectedBufferID { 0 };
};