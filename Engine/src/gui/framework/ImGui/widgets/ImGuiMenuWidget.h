#pragma once

#include "../../src/gui/widgets/MenuWidget.h"
#include <imgui.h>

class ImGuiMenuWidget : public MenuWidget
{
protected:


public:
	ImGuiMenuWidget();
	~ImGuiMenuWidget();


	virtual void render() override;
	virtual void MainMenuBar()override;
	virtual void FileMenu()override;
	virtual void EditMenu()override;
	virtual void ToolMenu()override;
	virtual void WindowMenu()override;
	virtual void HelpMenu()override;
};

