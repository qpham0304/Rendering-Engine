
#include "ImGuiConsoleLogWidget.h"
#include "core/features/Profiler.h"
#include <imgui.h>
#include <imgui_stdlib.h>

#include "logging/Logger.h"
#include "window/AppWindow.h"
#include "core/features/Timer.h"
#include "core/features/Mesh.h"
#include "core/features/Material.h"
#include "Core/features/ServiceLocator.h"
#include "core/scene/SceneManager.h"

// #include "graphics/framework/Vulkan/renderers/RendererManagerVulkan.h"	//TODO: remove vulkan specific setup
// #include "graphics/framework/Vulkan/renderers/RenderDeviceVulkan.h"
// #include "graphics/framework/Vulkan/renderers/renderpiplines/ShadowMapPassVulkan.h"
// #include "graphics/framework/vulkan/renderers/renderpiplines/DeferredRendererVulkan.h"
// #include "graphics/framework/vulkan/renderers/renderpiplines/features/ImageBasedVulkan.h"
// #include "graphics/framework/vulkan/renderers/RendererVulkan.h"

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
    if(!m_isVisible) {
        return;
    }

	_renderConsole();
	_renderProfiler();
}

void ImGuiConsoleLogWidget::_renderConsole()
{
	ImGuiIO& io = ImGui::GetIO();

	ImGui::Begin("Console");

    ImGui::SetNextItemWidth(200);
    ImGui::InputTextWithHint("Search", ICON_FA_SEARCH " Search...", searchFilter, IM_ARRAYSIZE(searchFilter));
    ImGui::SameLine();
    
    ImGui::Checkbox("Debug", &showDebug); ImGui::SameLine();
    ImGui::Checkbox("Info", &showInfo);   ImGui::SameLine();
    ImGui::Checkbox("Warn", &showWarn);   ImGui::SameLine();
    ImGui::Checkbox("Error", &showError); ImGui::SameLine();
    
    if (ImGui::Button("Clear")) {
		m_imguiLogger.clear();
	}

    ImGui::Separator();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.05f, 0.05f, 0.05f, 0.2f));
    // ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1)); 
    
    ImGui::BeginChild("LogRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

    std::string searchStr = searchFilter;
    std::transform(searchStr.begin(), searchStr.end(), searchStr.begin(), ::tolower);

	auto getColorForLevel = [](LogLevel level) -> ImVec4 {
		switch (level) {
			case LogLevel::Debug: return ImVec4(0.4f, 0.8f, 1.0f, 1.0f);
			case LogLevel::Info:  return ImVec4(0.2f, 0.8f, 0.3f, 1.0f);
			case LogLevel::Warn:  return ImVec4(0.9f, 0.8f, 0.0f, 1.0f);
			case LogLevel::Error: return ImVec4(1.0f, 0.2f, 0.2f, 1.0f);
			default:              return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
		}
	};

	// draw the background rectangle on the window's draw list
	auto TextBackgroundFilled = [](const char* text, ImU32 fill_col) {
		ImVec2 pos = ImGui::GetCursorScreenPos();
		ImVec2 size = ImGui::CalcTextSize(text);
		ImGui::GetWindowDrawList()->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), fill_col);
		ImGui::TextUnformatted(text);
	};
	
    auto entries = m_imguiLogger.logEntries();
    for (int i = 0; i < (int)entries.size(); i++) {
        const auto& entry = entries[i];
        
        if (entry.logLevel == LogLevel::Debug && !showDebug) continue;
        if (entry.logLevel == LogLevel::Info  && !showInfo)  continue;
        if (entry.logLevel == LogLevel::Warn  && !showWarn)  continue;
        if (entry.logLevel == LogLevel::Error && !showError) continue;

        if (searchFilter[0] != '\0') {
            std::string msgLower = entry.message;
            std::transform(msgLower.begin(), msgLower.end(), msgLower.begin(), ::tolower);
            if (msgLower.find(searchStr) == std::string::npos) continue;
        }

        ImVec4 color = getColorForLevel(entry.logLevel);
        
        ImGui::BeginGroup();
		{
			ImGui::TextColored(color, "[%s]", m_imguiLogger.getLevelString(entry.logLevel));
			ImGui::SameLine();

			std::string originalMsg = entry.message;
			size_t matchPos = std::string::npos;

			if (searchFilter[0] != '\0') {
				std::string msgLower = originalMsg;
				std::transform(msgLower.begin(), msgLower.end(), msgLower.begin(), ::tolower);
				matchPos = msgLower.find(searchStr);
			}

			if (matchPos != std::string::npos) {
				if (matchPos > 0) {
					ImGui::TextUnformatted(originalMsg.substr(0, matchPos).c_str());
					ImGui::SameLine(0, 0);
				}

				// highlight matched cases
				std::string matchPart = originalMsg.substr(matchPos, searchStr.length());
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f)); 		// white text for match
				TextBackgroundFilled(matchPart.c_str(), ImColor(255, 150, 0, 100));     // orange highlight
				ImGui::PopStyleColor();
				ImGui::SameLine(0, 0);

				ImGui::TextUnformatted(originalMsg.substr(matchPos + searchStr.length()).c_str());
			} else {
				ImGui::TextUnformatted(originalMsg.c_str());
			}

			ImGui::SameLine(0, 0); 
			ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMin().x);
			std::string id = "##row_" + std::to_string(i);

			ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.05f));
			ImGui::PushStyleColor(ImGuiCol_HeaderActive,  ImVec4(1.0f, 1.0f, 1.0f, 0.1f));

			if (ImGui::Selectable(id.c_str(), false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick)) {
				if (ImGui::IsMouseDoubleClicked(0)) {
					ImGui::SetClipboardText(entry.message.c_str());
				}
			}

			ImGui::PopStyleColor(2);
		}
		ImGui::EndGroup();
    }

    if (m_scrollToBottom || (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())) {
        ImGui::SetScrollHereY(1.0f);
    }
    m_scrollToBottom = false;

    ImGui::EndChild();
    // ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    m_imguiLogger.pollMessage();
    ImGui::End();

}

void ImGuiConsoleLogWidget::_renderProfiler()
{
	Profiler::display();
}