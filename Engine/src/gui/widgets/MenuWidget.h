#pragma once

#include "Widget.h"

class MenuWidget : public Widget
{
protected:
	MenuWidget() : Widget() {}

public:
	~MenuWidget() = default;

	virtual void render() override = 0;
	virtual void MainMenuBar() = 0;
	virtual void FileMenu() = 0;
	virtual void EditMenu() = 0;
	virtual void ToolMenu() = 0;
	virtual void WindowMenu() = 0;
	virtual void HelpMenu() = 0;
};

