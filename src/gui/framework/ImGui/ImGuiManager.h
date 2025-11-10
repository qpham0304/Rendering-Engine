#pragma once

#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <ImGuizmo.h>

#include "../../GuiManager.h"
#include "widgets/ImGuiConsoleLogWidget.h"
#include "widgets/ImGuiLeftSidebarWidget.h"
#include "widgets/ImGuiRightSidebarWidget.h"
#include "widgets/ImGuiMenuWidget.h"

class ImGuiManager : public GuiManager 
{
public:

public:
	ImGuiManager();
	ImGuiManager(bool darkTheme);

	ImGuiManager(const ImGuiManager& other) = default;
	ImGuiManager(ImGuiManager&& other) noexcept = default;
	ImGuiManager& operator=(const ImGuiManager& other) = default;
	ImGuiManager& operator=(ImGuiManager&& other) noexcept = default;

	~ImGuiManager();

	int init(WindowConfig config) override;
	void start() override;
	void debugWindow(ImTextureID texture);
	void applicationWindow();
	void render() override;
	void end() override;
	int onClose() override;

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
