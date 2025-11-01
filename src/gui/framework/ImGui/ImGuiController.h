#pragma once

#include "../../GuiController.h"
#include "widgets/ImGuiConsoleLogWidget.h"
#include "widgets/ImGuiLeftSidebarWidget.h"
#include "widgets/ImGuiRightSidebarWidget.h"
#include "widgets/ImGuiMenuWidget.h"

class ImGuiController : public GuiManager {
private:
	ImGuiLeftSidebarWidget leftSidebar;
	ImGuiRightSidebarWidget rightSidebar;
	ImGuiConsoleLogWidget console;
	ImGuiMenuWidget menu;

public:
	ImGuiController();
	ImGuiController(bool darkTheme);


	ImGuiController(const ImGuiController& other) = default;

	ImGuiController(ImGuiController&& other) noexcept = default;

	ImGuiController& operator=(const ImGuiController& other) {}

	ImGuiController& operator=(ImGuiController&& other) noexcept {}

	~ImGuiController();

	void init(GLFWwindow* window, int width, int height) override;
	void start() override;
	void debugWindow(ImTextureID texture);
	void applicationWindow();
	void render() override;
	void end() override;
	void onClose() override;

	void setTheme(bool darkTheme) override;
	void useLightTheme() override;
	void useDarkTheme() override;

	// TODO: add closable tab, ability to on/off open close tab
	// then reopen them in navigation bar;
};
