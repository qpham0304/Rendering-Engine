
#include "src/gui/framework/ImGui/widgets/ImGuiConsoleLogWidget.h"
#include "../../src/core/features/Profiler.h"
#include <imgui.h>

bool ButtonCenteredOnLine(const char* label, float alignment = 0.5f)
{
	ImGuiStyle& style = ImGui::GetStyle();

	float size = ImGui::CalcTextSize(label).x + style.FramePadding.x * 2.0f;
	float avail = ImGui::GetContentRegionAvail().x;

	float off = (avail - size) * alignment;
	if (off > 0.0f)
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + off);

	return ImGui::Button(label);
}

ImGuiConsoleLogWidget::ImGuiConsoleLogWidget() : ConsoleLogWidget()
{

}

void ImGuiConsoleLogWidget::render()
{
	ImGui::BeginGroup();
	//ImGui::SetNextItemAllowOverlap();
	//ImGui::SetCursorPos(ImGui::GetWindowContentRegionMin());
	Profiler::getInstance().display();

	ImGui::Begin("Assets");
	ImGui::Text("assets placeholder");
	ImGui::End();

	ImGui::Begin("console");
	ImGui::ShowDebugLogWindow();
	ImGui::End();

	//ImGui::ShowDemoWindow();
	ImGui::EndGroup();
}
