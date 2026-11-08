#include "ImGuiMenuWidget.h"

#include <algorithm>
#include "ImGuiScenePicker.h"
#include "core/scene/SceneManager.h"

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
		WindowMenu();
		HelpMenu();
		_activeSceneButton();

		ImGui::EndMainMenuBar();
	}
}

// the shortcut text is only a hint, the keys are not bound from the menu yet
void ImGuiMenuWidget::FileMenu()
{
	if (ImGui::BeginMenu("File"))
	{
		Scene* scene = SceneManager::getInstance().getActiveScene();

		if (ImGui::MenuItem("New Scene", "Ctrl+N")) {
			ImGuiScenePicker::newScene();
		}
		if (ImGui::MenuItem("Open Scene...", "Ctrl+O")) {
			ImGuiScenePicker::open();
		}

		ImGui::Separator();
		if (ImGui::MenuItem("Save Scene", "Ctrl+S", false, scene != nullptr)) {
			ImGuiScenePicker::saveActiveScene();
		}
		if (scene && ImGui::IsItemHovered()) {
			ImGui::SetTooltip("%s", scene->savePath().c_str());
		}
		if (ImGui::MenuItem("Reload Scene", "Ctrl+R", false, scene != nullptr)) {
			scene->requestReload();
		}

		ImGui::Separator();
		if (ImGui::MenuItem("Exit", "Alt+F4"))
		{

		}
		ImGui::EndMenu();
	}
}

void ImGuiMenuWidget::EditMenu()
{
	if (ImGui::BeginMenu("Edit"))
	{
		if (ImGui::MenuItem("Undo", "Ctrl+Z"))
		{

		}

		if (ImGui::MenuItem("Redo", "Ctrl+Y"))
		{

		}

		ImGui::Separator();
		if (ImGui::MenuItem("Cut", "Ctrl+X"))
		{

		}

		if (ImGui::MenuItem("Duplicate", "Ctrl+D"))
		{

		}

		if (ImGui::MenuItem("Select All", "Ctrl+A"))
		{

		}
		ImGui::EndMenu();
	}
}

void ImGuiMenuWidget::_activeSceneButton()
{
	Scene* scene = SceneManager::getInstance().getActiveScene();
	std::string label = (scene ? scene->getName() : std::string("No scene")) + "  v";
	float width = ImGui::CalcTextSize(label.c_str()).x + ImGui::GetStyle().FramePadding.x * 2.0f;
	float hintWidth = ImGui::CalcTextSize("Scene").x + ImGui::GetStyle().ItemSpacing.x;
	ImGui::SameLine(std::max(ImGui::GetWindowWidth() - width - hintWidth - ImGui::GetStyle().WindowPadding.x, ImGui::GetCursorPosX()));
	ImGui::TextDisabled("Scene");
	ImGui::SameLine();
	ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
	if (ImGui::Button(label.c_str())) {
		ImGuiScenePicker::open();
	}
	ImGui::PopStyleColor();
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Open Scene (Ctrl+O)");
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

