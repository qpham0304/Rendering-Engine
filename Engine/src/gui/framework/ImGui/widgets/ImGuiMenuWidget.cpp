#include "ImGuiMenuWidget.h"

#include "ImGuiScenePicker.h"
#include "core/scene/SceneManager.h"
#include "core/features/ServiceLocator.h"
#include "gui/GuiManager.h"
#include <imgui_internal.h>

namespace {
	// spacing the shortcut from the button text
	bool ShortcutItem(const char* label, const char* shortcut, bool selected = false, bool enabled = true)
	{
		ImVec2 pos = ImGui::GetCursorScreenPos();
		float right = pos.x + ImGui::GetContentRegionAvail().x;
		bool pressed = ImGui::MenuItem(label, nullptr, selected, enabled);

		float width = ImGui::CalcTextSize(shortcut).x;
		ImGui::GetCurrentWindow()->DC.MenuColumns.DeclColumns(0.0f, 0.0f, width, 0.0f);

		float alpha = enabled ? 1.0f : ImGui::GetStyle().DisabledAlpha;
		ImGui::GetWindowDrawList()->AddText(ImVec2(right - width, pos.y), ImGui::GetColorU32(ImGuiCol_TextDisabled, alpha), shortcut);
		return pressed;
	}
}

ImGuiMenuWidget::ImGuiMenuWidget(std::span<std::shared_ptr<Widget>> widgets) 
	: MenuWidget("ImGuiMenuWidget")
{
	setFootprint("ImGuiMenuWidget", sizeof(ImGuiMenuWidget), MemorySystem::Gui);
	m_widgetRefs.assign(widgets.begin(), widgets.end());

}


ImGuiMenuWidget::~ImGuiMenuWidget()
{

}

void ImGuiMenuWidget::render()
{
	MainMenuBar();
	ImGuiScenePicker::draw();
}

void ImGuiMenuWidget::MainMenuBar()
{
	if (ImGui::BeginMainMenuBar())
	{
		FileMenu();
		EditMenu();
		ToolMenu();
		ViewMenu();
		WindowMenu();
		HelpMenu();

		ImGui::EndMainMenuBar();
	}
}

// the shortcut text is only a hint, the keys are not bound from the menu yet
void ImGuiMenuWidget::FileMenu()
{
	if (ImGui::BeginMenu("File"))
	{
		Scene* scene = SceneManager::getInstance().getActiveScene();

		if (ShortcutItem("New Scene", "Ctrl+N")) {
			ImGuiScenePicker::newScene();
		}
		if (ShortcutItem("Open Scene...", "Ctrl+O")) {
			ImGuiScenePicker::open();
		}

		ImGui::Separator();
		if (ShortcutItem("Save Scene", "Ctrl+S", false, scene != nullptr)) {
			ImGuiScenePicker::saveActiveScene();
		}
		if (scene && ImGui::IsItemHovered()) {
			ImGui::SetTooltip("%s", scene->savePath().c_str());
		}
		if (ShortcutItem("Reload Scene", "Ctrl+R", false, scene != nullptr)) {
			scene->requestReload();
		}

		ImGui::Separator();
		if (ShortcutItem("Exit", "Alt+F4"))
		{

		}
		ImGui::EndMenu();
	}
}

void ImGuiMenuWidget::EditMenu()
{
	if (ImGui::BeginMenu("Edit"))
	{
		if (ShortcutItem("Undo", "Ctrl+Z"))
		{

		}

		if (ShortcutItem("Redo", "Ctrl+Y"))
		{

		}

		ImGui::Separator();
		if (ShortcutItem("Cut", "Ctrl+X"))
		{

		}

		if (ShortcutItem("Duplicate", "Ctrl+D"))
		{

		}

		if (ShortcutItem("Select All", "Ctrl+A"))
		{

		}
		ImGui::EndMenu();
	}
}

void ImGuiMenuWidget::ToolMenu()
{
	if (ImGui::BeginMenu("Tool"))
	{
		if (ImGui::MenuItem("Effects"))
		{

		}

		if (ImGui::MenuItem("Post processing"))
		{

		}

		if (ImGui::MenuItem("Color adjust"))
		{

		}

		if (ImGui::MenuItem("Blur"))
		{

		}
		ImGui::EndMenu();
	}
}

void ImGuiMenuWidget::ViewMenu()
{
	if (ImGui::BeginMenu("View"))
	{
		if (ImGui::BeginMenu("Appearance")) {
			GuiManager& guiManager = ServiceLocator::GetService<GuiManager>("ImGuiManager");
			for (const std::string& name : guiManager.listThemes()) {
				bool current = guiManager.getTheme() == name;
				if (ImGui::MenuItem(name.c_str(), nullptr, current) && !current) {
					guiManager.setTheme(name);
				}
			}
			ImGui::EndMenu();
		}
		ImGui::EndMenu();
	}
}

void ImGuiMenuWidget::WindowMenu()
{
	if (ImGui::BeginMenu("Window"))
	{
		if (ImGui::MenuItem("Show all Debugs", nullptr, &m_showDebug)) {
			for (auto& widget : m_widgetRefs) {
				widget->setVisible(m_showDebug);
			}
		}
		for(auto& widget : m_widgetRefs) {
			bool visible = widget->isVisible();
			if (ImGui::MenuItem(widget->getName(), nullptr, &visible)) {
				widget->setVisible(visible);
			}
		}
		ImGui::EndMenu();
	}
}

void ImGuiMenuWidget::HelpMenu()
{
	if (ImGui::BeginMenu("Help"))
	{
		if (ImGui::MenuItem("Tutorial"))
		{

		}

		if (ImGui::MenuItem("Support"))
		{

		}

		if (ImGui::MenuItem("Contact"))
		{

		}

		if (ImGui::MenuItem("Send Feedback"))
		{

		}

		if (ImGui::MenuItem("Privacy"))
		{

		}

		if (ImGui::MenuItem("Version"))
		{

		}
		ImGui::EndMenu();
	}
}

