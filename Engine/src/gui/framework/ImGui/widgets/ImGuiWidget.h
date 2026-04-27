#pragma once

#include "imgui.h"
#include "gui/widgets/Widget.h"

class ImGuiWidget : public Widget 
{
public:
    virtual ~ImGuiWidget() = default;
	virtual void render() = 0;

protected:
    ImGuiWidget(std::string name = "widget") : Widget(name) {}

};