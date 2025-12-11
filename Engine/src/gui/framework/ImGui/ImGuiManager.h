#pragma once

#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_vulkan.h>
#include <ImGuizmo.h>

#include "widgets/ImGuiConsoleLogWidget.h"
#include "widgets/ImGuiLeftSidebarWidget.h"
#include "widgets/ImGuiRightSidebarWidget.h"
#include "widgets/ImGuiMenuWidget.h"
#include "gui/GuiManager.h"

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
	void onUpdate() override;
	int onClose() override;
	void start(void* handle = nullptr) override;
	void render(void* handle = nullptr) override;
	void end(void* handle = nullptr) override;

	void setTheme(bool darkTheme) override;
	void useLightTheme() override;
	void useDarkTheme() override;

	void renderGuizmo(TransformComponent& transformComponent) override;
	void guizmoTranslate() override;
	void guizmoRotate() override;
	void guizmoScale() override;

	void debugWindow(ImTextureID texture);
	void applicationWindow();


	// TODO: add closable tab, ability to on/off open close tab
	// then reopen them in navigation bar;
private:
	int _initOpenGL();
	void _onCloseOpenGL();
	void _onUpdateOpenGL();

	int _initVulkan();
	void _onCloseVulkan();
	void _onUpdateVulkan();

private:
	VkDescriptorPool guiDescriptorPool;

	ImGuizmo::OPERATION GuizmoType = ImGuizmo::OPERATION::TRANSLATE;
	
	ImGuiLeftSidebarWidget leftSidebar;
	ImGuiRightSidebarWidget rightSidebar;
	ImGuiConsoleLogWidget console;
	ImGuiMenuWidget menu;
};
