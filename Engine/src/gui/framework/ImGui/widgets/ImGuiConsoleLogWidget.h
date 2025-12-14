#pragma once

#include "gui/widgets/ConsoleLogWidget.h"
#include <imgui.h>

class ImGuiConsoleLogWidget : public ConsoleLogWidget
{
protected:
	bool scrollToBottom = false;

public:
	ImGuiConsoleLogWidget();

	void render() override;
};

