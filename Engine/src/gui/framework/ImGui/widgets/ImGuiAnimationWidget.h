#pragma once

#include "ImGuiWidget.h"
#include <functional>

class ImGuiAnimationWidget : public ImGuiWidget 
{
public:
    ImGuiAnimationWidget(std::string name = "ImGuiAnimationWidget");
    ~ImGuiAnimationWidget() override = default;

    virtual void render() override;
    
};