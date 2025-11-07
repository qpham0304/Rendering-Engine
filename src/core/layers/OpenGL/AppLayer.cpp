#include "../AppLayer.h"
#include "../../src/core/scene/SceneManager.h"
#include "../../src/window/AppWindow.h"
#include "../../src/core/events/EventManager.h"
#include "Imgui.h"  //TODO: remove when there's no ui dependency
#include "camera.h"
#include "../../features/ServiceLocator.h"
#include "../../src/core/layers/layerManager.h"

void AppLayer::renderControl()
{
	ImGui::Begin("control");

	ImGui::End();
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
			camera->processKeyboard();
			isActive = true;
		}
		else {
			isActive = false;
		}
		ImGui::EndChild();
		ImGui::End();
	}
}

AppLayer::AppLayer(const std::string& name) 
	: Layer(name), isActive(false), VAO(0), VBO(0)
{
}

AppLayer::~AppLayer()
{
	onDetach();
}

void AppLayer::onAttach(LayerManager* manager)
{
	Layer::onAttach(manager);

	int width = manager->Window().width;
	int height = manager->Window().width;

	applicationFBO.Init(
		width,
		height,
		GL_RGBA32F,
		GL_RGBA,
		GL_FLOAT,
		nullptr
	);
	camera = std::make_unique< Camera>();
	camera->init(
		width,
		height,
		glm::vec3(-6.5f, 3.5f, 8.5f),
		glm::vec3(0.5, -0.2, -1.0f)
	);
	skybox.reset(new SkyboxRenderer());
	
	EventManager::getInstance().subscribe(EventType::ModelLoadEvent, [](Event& event) {
		ModelLoadEvent& e = static_cast<ModelLoadEvent&>(event);
		if (!e.entity.hasComponent<ModelComponent>()) {
			e.entity.addComponent<ModelComponent>();
		}

		ModelComponent& component = e.entity.getComponent<ModelComponent>();
		component.path = "Loading...";
		std::string uuid = SceneManager::getInstance().addModel(e.path.c_str());

		if (component.path != e.path && !uuid.empty()) {
			component.model = SceneManager::getInstance().models[uuid];
			component.path = e.path;
		}

		else {
			ImGui::OpenPopup("Failed to load file, please check the format");
			component.reset();
		}
		});

	EventManager::getInstance().subscribe(EventType::AnimationLoadEvent, [](Event& event) {
		AnimationLoadEvent& e = static_cast<AnimationLoadEvent&>(event);
		if (!e.entity.hasComponent<AnimationComponent>()) {
			e.entity.addComponent<AnimationComponent>();
		}

		AnimationComponent& animationComponent = e.entity.getComponent<AnimationComponent>();
		ModelComponent& modelComponent = e.entity.getComponent<ModelComponent>();
		animationComponent.path = "Loading...";
		std::string uuid = SceneManager::getInstance().addAnimation(e.path.c_str(), modelComponent.model.lock().get());

		if (animationComponent.path != e.path && !uuid.empty()) {
			animationComponent.animation = SceneManager::getInstance().animations[uuid];
			animationComponent.animator.Init(SceneManager::getInstance().animations[uuid].get());
			animationComponent.path = e.path;
		}

		else {
			ImGui::OpenPopup("Failed to load file, please check the format");
			animationComponent.reset();
		}
		});


	EventManager& eventManager = EventManager::getInstance();
	eventManager.subscribe(EventType::MouseScrolled, [this](Event& event) {
		MouseScrollEvent& mouseEvent = static_cast<MouseScrollEvent&>(event);
		if (isActive) {
			camera->scroll_callback(mouseEvent.m_x, mouseEvent.m_y);
		}
	});

	eventManager.subscribe(EventType::MouseMoved, [this](Event& event) {
		MouseMoveEvent& mouseEvent = static_cast<MouseMoveEvent&>(event);
		if (isActive) {
			camera->processMouse();
		}
	});

	eventManager.subscribe(EventType::WindowResize, [this](Event& event) {
		WindowResizeEvent& windowResizeEvent = static_cast<WindowResizeEvent&>(event);
		camera->updateViewResize(windowResizeEvent.m_width, windowResizeEvent.m_height);
	});
}

void AppLayer::onDetach()
{
	SceneManager::cameraController = nullptr;
}

void AppLayer::onUpdate()
{
	camera->onUpdate();
}

void AppLayer::onGuiUpdate()
{
	//ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	renderControl();
	renderApplication();
	//ImGui::PopStyleVar();
}

void AppLayer::onEvent(Event& event)
{

}

const int& AppLayer::GetTextureID()
{
	return applicationFBO.texture;
}
