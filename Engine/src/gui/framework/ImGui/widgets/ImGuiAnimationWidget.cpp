#include "ImGuiAnimationWidget.h"

ImGuiAnimationWidget::ImGuiAnimationWidget(std::string name)
    : ImGuiWidget(name)
{

}

void ImGuiAnimationWidget::render()
{
    ImGui::Begin("Animation Widget");

    ImGui::End();
}
