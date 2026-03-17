#pragma once

#include "gui/widgets/MenuWidget.h"
#include <imgui.h>

class ImGuiMenuWidget : public MenuWidget
{
public:
	ImGuiMenuWidget(std::span<std::shared_ptr<Widget>> widgetIDs);
	~ImGuiMenuWidget();

	virtual void render() override;
	virtual void MainMenuBar()override;
	virtual void FileMenu()override;
	virtual void EditMenu()override;
	virtual void ToolMenu()override;
	virtual void WindowMenu()override;
	virtual void HelpMenu()override;

protected:
	std::vector<std::shared_ptr<Widget>> m_widgetRefs;
	bool m_showDebug;

};

