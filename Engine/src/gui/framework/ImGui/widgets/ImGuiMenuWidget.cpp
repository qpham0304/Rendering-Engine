#include "ImGuiMenuWidget.h"

ImGuiMenuWidget::ImGuiMenuWidget(std::span<std::shared_ptr<Widget>> widgets) 
	: MenuWidget("ImGuiMenuWidget")
{
	m_widgetRefs.assign(widgets.begin(), widgets.end());

}


ImGuiMenuWidget::~ImGuiMenuWidget()
{

}

void ImGuiMenuWidget::render()
{
	MainMenuBar();
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

		ImGui::EndMainMenuBar();
	}
}

void ImGuiMenuWidget::FileMenu()
{
	if (ImGui::BeginMenu("File"))
	{
		if (ImGui::MenuItem("New"))
		{

		}

		if (ImGui::MenuItem("Open"))
		{

		}

		if (ImGui::MenuItem("Save"))
		{

		}

		if (ImGui::MenuItem("Exit"))
		{

		}
		ImGui::EndMenu();
	}
}

void ImGuiMenuWidget::EditMenu()
{
	if (ImGui::BeginMenu("Edit"))
	{
		if (ImGui::MenuItem("Undo"))
		{

		}

		if (ImGui::MenuItem("Redo"))
		{

		}

		if (ImGui::MenuItem("Cut"))
		{

		}

		if (ImGui::MenuItem("Select"))
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

