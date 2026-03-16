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
    bool showDebug { true };
    bool showInfo { true };
    bool showWarn { true };
    bool showError { true };
    char searchFilter[128] {""};

    void _renderConsole();
    void _renderProfiler();
};

