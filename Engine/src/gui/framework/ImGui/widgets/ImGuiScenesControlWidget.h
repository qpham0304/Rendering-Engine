#pragma once

#include "ImGuiWidget.h"

class ImGuiScenesControlWidget : public ImGuiWidget
{
public:
    ImGuiScenesControlWidget(std::string name = "Scenes Control");
    ~ImGuiScenesControlWidget() override = default;

    virtual void render() override;
};
