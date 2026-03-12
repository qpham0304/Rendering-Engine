#pragma once

#include "gui/widgets/ConsoleLogWidget.h"
#include "gui/framework/ImGui/ImGuiLogger.h"
#include <imgui.h>

class ImGuiConsoleLogWidget : public ConsoleLogWidget
{
public:
	ImGuiConsoleLogWidget();
	virtual ~ImGuiConsoleLogWidget() override;

	void render() override;
	

private:
	bool m_scrollToBottom{ true };
	ImGuiLogger m_imguiLogger;

};

