#pragma once

//#include "GuiController.h"
#include "../../GuiController.h"
#include "widgets/ImGuiConsoleLogWidget.h"
#include "widgets/ImGuiLeftSidebarWidget.h"
#include "widgets/ImGuiRightSidebarWidget.h"
#include "widgets/ImGuiMenuWidget.h"

class ImGuiController : public GuiController {
private:
	ImGuiLeftSidebarWidget leftSidebar;
	ImGuiRightSidebarWidget rightSidebar;
	ImGuiConsoleLogWidget console;
	ImGuiMenuWidget menu;

public:
	ImGuiController();
	ImGuiController(bool darkTheme);


	// Copy constructor
	ImGuiController(const ImGuiController& other);

	// Move constructor
	ImGuiController(ImGuiController&& other) noexcept;

	// Copy assignment operator
	ImGuiController& operator=(const ImGuiController& other) {}

	// Move assignment operator
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
