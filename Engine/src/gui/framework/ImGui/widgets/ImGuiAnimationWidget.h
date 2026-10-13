#pragma once

#include "ImGuiWidget.h"
#include <functional>

class ImGuiAnimationWidget : public ImGuiWidget 
{
public:
    ImGuiAnimationWidget(std::string name = "ImGuiAnimationWidget");
    ~ImGuiAnimationWidget() override = default;

    virtual void render() override;
    
    void drawBone(const std::vector<uint32_t> ids) const;
    void drawTree(const std::vector<uint32_t> ids) const;
private:
    std::string selectedAnimation{"select animation"};
    uint32_t selectedEntityID { 0 };
};