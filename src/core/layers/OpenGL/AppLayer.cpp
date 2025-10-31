#include "../AppLayer.h"
#include "../../src/core/scene/SceneManager.h"
#include "../../src/window/AppWindow.h"
#include "../../src/core/events/EventManager.h"
#include "camera.h"

void AppLayer::renderControl()
{
	if (ImGui::Begin("control")) {

		ImGui::End();
	}
}

void AppLayer::renderApplication(const int& fboTexture)
{
	if (ImGui::Begin("Application window")) {
		ImGui::BeginChild("Child");	// spand fullscreen so it won't scroll on resize
		ImVec2 wsize = ImGui::GetWindowSize();
		int wWidth = static_cast<int>(ImGui::GetWindowWidth());
		int wHeight = static_cast<int>(ImGui::GetWindowHeight());
		if (fboTexture == -1) {
			ImGui::Image((ImTextureID)applicationFBO.texture, wsize, ImVec2(0, 1), ImVec2(1, 0));
		}
		else {
			ImGui::Image((ImTextureID)fboTexture, wsize, ImVec2(0, 1), ImVec2(1, 0));
		}
		//(ImGui::IsItemHovered() && ImGui::IsWindowFocused()) ? isActive = true : false;
		if (camera->getViewWidth() != wWidth || camera->getViewHeight() != wHeight) {
			camera->updateViewResize(wWidth, wHeight);
		}

		if (ImGui::IsItemHovered() && ImGui::IsWindowFocused()) {
			camera->processKeyboard(AppWindow::window);
			isActive = true;
		}
		else {
			isActive = false;
		}
		ImGui::EndChild();
		ImGui::End();
	}
}

AppLayer::AppLayer(const std::string& name) : Layer(name), isActive(false), VAO(0), VBO(0)
{
	applicationFBO.Init(
		AppWindow::width,
		AppWindow::height,
		GL_RGBA32F,
		GL_RGBA,
		GL_FLOAT,
		nullptr
	);
	camera = new Camera();
	camera->init(
		AppWindow::width, 
		AppWindow::height, 
		glm::vec3(-6.5f, 3.5f, 8.5f), 
		glm::vec3(0.5, -0.2, -1.0f)
	);
	skybox.reset(new SkyboxRenderer());
}

AppLayer::~AppLayer()
{
	OnDetach();
}

void AppLayer::OnAttach()
{
	EventManager& eventManager = EventManager::getInstance();
	eventManager.Subscribe(EventType::MouseScrolled, [this](Event& event) {
		MouseScrollEvent& mouseEvent = static_cast<MouseScrollEvent&>(event);
		if (isActive) {
			camera->scroll_callback(mouseEvent.m_x, mouseEvent.m_y);
		}
	});

	eventManager.Subscribe(EventType::MouseMoved, [this](Event& event) {
		MouseMoveEvent& mouseEvent = static_cast<MouseMoveEvent&>(event);
		if (isActive) {
			camera->processMouse(mouseEvent.window);
		}
	});

	eventManager.Subscribe(EventType::WindowResize, [this](Event& event) {
		WindowResizeEvent& windowResizeEvent = static_cast<WindowResizeEvent&>(event);
		camera->updateViewResize(windowResizeEvent.m_width, windowResizeEvent.m_height);
	});
}

void AppLayer::OnDetach()
{
	SceneManager::cameraController = nullptr;
}

void AppLayer::OnUpdate()
{
	camera->onUpdate();
}

void AppLayer::OnGuiUpdate()
{
	//ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	renderControl();
	renderApplication();
	//ImGui::PopStyleVar();
}

void AppLayer::OnEvent(Event& event)
{

}

const int& AppLayer::GetTextureID()
{
	return applicationFBO.texture;
}
