#pragma once

#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <ImGuizmo.h>

#include "../../GuiManager.h"
#include "widgets/ImGuiConsoleLogWidget.h"
#include "widgets/ImGuiLeftSidebarWidget.h"
#include "widgets/ImGuiRightSidebarWidget.h"
#include "widgets/ImGuiMenuWidget.h"

class ImGuiController : public GuiManager 
{
public:

public:
	ImGuiController();
	ImGuiController(bool darkTheme);


	ImGuiController(const ImGuiController& other) = default;

	ImGuiController(ImGuiController&& other) noexcept = default;

	ImGuiController& operator=(const ImGuiController& other) {}

	ImGuiController& operator=(ImGuiController&& other) noexcept {}

	~ImGuiController();

	void init(WindowConfig config) override;
	void start() override;
	void debugWindow(ImTextureID texture);
	void applicationWindow();
	void render() override;
	void end() override;
	void onClose() override;

	void setTheme(bool darkTheme) override;
	void useLightTheme() override;
	void useDarkTheme() override;

	void renderGuizmo(TransformComponent& transformComponent) override;
	void guizmoTranslate() override;
	void guizmoRotate() override;
	void guizmoScale() override;


	// TODO: add closable tab, ability to on/off open close tab
	// then reopen them in navigation bar;
private:

private:
	ImGuizmo::OPERATION GuizmoType = ImGuizmo::OPERATION::TRANSLATE;
	
	ImGuiLeftSidebarWidget leftSidebar;
	ImGuiRightSidebarWidget rightSidebar;
	ImGuiConsoleLogWidget console;
	ImGuiMenuWidget menu;
};
