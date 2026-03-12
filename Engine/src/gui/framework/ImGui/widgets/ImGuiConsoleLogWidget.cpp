
#include "ImGuiConsoleLogWidget.h"
#include "core/features/Profiler.h"
#include <imgui.h>

#include "window/AppWindow.h"
#include "logging/Logger.h"
#include "core/features/Timer.h"
#include "core/features/Mesh.h"
#include "core/features/Material.h"
#include "vulkan/vulkan.h" //TODO: remove dependency

bool ButtonCenteredOnLine(const char* label, float alignment = 0.5f)
{
	ImGuiStyle& style = ImGui::GetStyle();

	float size = ImGui::CalcTextSize(label).x + style.FramePadding.x * 2.0f;
	float avail = ImGui::GetContentRegionAvail().x;
	float off = (avail - size) * alignment;

	if (off > 0.0f) {
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + off);
	}

	return ImGui::Button(label);
}

ImGuiConsoleLogWidget::ImGuiConsoleLogWidget() : ConsoleLogWidget()
{

}

ImGuiConsoleLogWidget::~ImGuiConsoleLogWidget()
{
	
}

void ImGuiConsoleLogWidget::render()
{
	ImGuiIO& io = ImGui::GetIO();

	ImGui::Begin("Console");

	if (ImGui::Button("Clear")) s_UiBuffer.clear();
	ImGui::SameLine();
	ImGui::Text("Size: %d bytes", s_UiBuffer.size());

	ImGui::Separator();

	// Scrolling Region: Reserve height for the input field below
	// -ImGui::GetFrameHeightWithSpacing() leaves exact room for one line of input
	// const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
	const float footer_height_to_reserve = 0.0f;
	ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height_to_reserve), false, ImGuiWindowFlags_HorizontalScrollbar);

	ImGui::TextUnformatted(s_UiBuffer.begin());

	if (m_scrollToBottom || (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()))
		ImGui::SetScrollHereY(1.0f);
	m_scrollToBottom = false;

	ImGui::EndChild();

	m_imguiLogger.pollMessage();

	// ImGui::Separator();

	// char inputBuffer[256] = "";
	// bool reclaim_focus = false;

	// ImGuiInputTextFlags_EnterReturnsTrue allows process the command only when Enter is hit
	// if (ImGui::InputText("Command", inputBuffer, IM_ARRAYSIZE(inputBuffer), ImGuiInputTextFlags_EnterReturnsTrue))
	// {
	// 	std::string command(inputBuffer);
	// 	if (!command.empty()) {
	// 		//HandleCommand(command);   //logic placeholder
	// 	}
	// 	inputBuffer[0] = '\0';
	// 	reclaim_focus = true;
	// 	m_scrollToBottom = true;
	// }
	
	// ImGui::SetItemDefaultFocus();	// auto-focus on the input box if we just sent a command
	// if (reclaim_focus) {
	// 	ImGui::SetKeyboardFocusHere(-1); // Focus previous widget
	// }

	ImGui::End();

	// ImGui::BeginGroup();
	//ImGui::SetNextItemAllowOverlap();
	//ImGui::SetCursorPos(ImGui::GetWindowContentRegionMin());
	Profiler::display();
	// ImGui::ShowDebugLogWindow();
	// ImGui::EndGroup();
}
