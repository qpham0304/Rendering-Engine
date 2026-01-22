#pragma once

#include "gui/widgets/Widget.h"
#include <imgui.h>

class ImGuiMathWidget : public Widget
{
public:
    ImGuiMathWidget() = default;
    ~ImGuiMathWidget() override = default;

	virtual void render() override;

private:
    int degree = 1;
    std::vector<float> coeffs = { 1.0f, 1.0f }; // Default degree 1, all coeffs = 1


};