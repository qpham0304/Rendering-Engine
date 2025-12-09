#pragma once

#include "../../src/gui/widgets/ConsoleLogWidget.h"


class ImGuiConsoleLogWidget : public ConsoleLogWidget
{
protected:
	bool scrollToBottom = false;

public:
	ImGuiConsoleLogWidget();

	void render() override;
};

