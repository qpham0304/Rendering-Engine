#include "Sandbox.h"
#include <cstring>
#include <cassert>
#include <chrono>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>
#include "window/AppWindow.h"
#include "core/events/EventManager.h"

//TODO: remove from application, only renderer can see these concrete implementation
#include "graphics/framework/Vulkan/Renderers/RendererVulkan.h"

//const std::vector<Vertex> vertices = {
//    {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
//    {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
//    {{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
//    {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},
//
//    {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
//    {{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
//    {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
//    {{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}
//};
//
//std::vector<uint16_t> indices = {
//	0, 1, 2, 2, 3, 0,
//	4, 5, 6, 6, 7, 4
//};

void Sandbox::pushLayer(Layer* layer)
{
	layerManager->addLayer(layer);
}

void Sandbox::init(WindowConfig config)
{
	windowConfig = config;

	ServiceLocator::setContext(&serviceLocator);
	engineLogger = platformFactory.createLogger(LoggerPlatform::SPDLOG, "Engine");
	clientLogger = platformFactory.createLogger(LoggerPlatform::SPDLOG, "Client");
	appWindow = platformFactory.createWindow(windowConfig.windowPlatform);
	renderer = platformFactory.createRenderer(windowConfig.renderPlatform);
	renderDevice = platformFactory.createRenderDevice(windowConfig.renderPlatform);
	guiManager = platformFactory.createGuiManager(windowConfig.guiPlatform);
	textureManager = platformFactory.createTextureManager(windowConfig.renderPlatform);
	//bufferManager = platformFactory.createBufferManager(windowConfig.renderPlatform);

	// should be registered via factory
	// bufferManager = std::make_unique<VulkanBufferManager>();

	meshManager = std::make_unique<MeshManager>();
	modelManager = std::make_unique<ModelManager>();
	layerManager = std::make_unique<LayerManager>(serviceLocator);

	serviceLocator.Register<MeshManager>("MeshManager", *meshManager);
	serviceLocator.Register<ModelManager>("ModelManager", *modelManager);



	engineLogger->setLevel(LogLevel::Debug);
	ServiceLocator::supportingServices();



	editorLayer = new EditorLayer("EditorLayer", *guiManager);


	services.push_back(engineLogger.get());
	services.push_back(clientLogger.get());
	services.push_back(appWindow.get());
	services.push_back(renderer.get());
	services.push_back(renderDevice.get());
	services.push_back(guiManager.get());
}

void Sandbox::start()
{
	//NOTE: setup order is important!
	//appWindow->init(windowConfig);
	//renderer->init();
	//renderDevice->init(windowConfig);
	//guiManager->init(windowConfig);
	for (Service*& service : services) {
		service->init(windowConfig);
	}


	//TODO: this should be registered through platform factory
	RendererVulkan* rendererVulkan = dynamic_cast<RendererVulkan*>(renderer.get());
	serviceLocator.Register<BufferManager>("BufferManager", *rendererVulkan->bufferManager);

	textureManager->init();
	meshManager->init();
	modelManager->init();
	// renderer->addModel("assets/models/aru/aru.gltf");
	renderer->addModel("assets/models/cube/cube-notex.gltf");

	camera.init(AppWindow::getWidth(), AppWindow::getHeight(),
		glm::vec3(2.0f, 2.0f, 2.0f),
		glm::normalize(glm::vec3(0.0f, 0.0f, 0.0f) - glm::vec3(2.0f, 2.0f, 2.0f))
	);


	EventManager::getInstance().subscribe(EventType::WindowResize, [this](Event& event) {
		WindowResizeEvent& windowResizeEvent = static_cast<WindowResizeEvent&>(event);
		camera.updateViewResize(windowResizeEvent.m_width, windowResizeEvent.m_height);
		});

	EventManager& eventManager = EventManager::getInstance();
	eventManager.subscribe(EventType::MouseScrolled, [this](Event& event) {
		MouseScrollEvent& mouseEvent = static_cast<MouseScrollEvent&>(event);
		camera.scroll_callback(mouseEvent.m_x, mouseEvent.m_y);
		});

	eventManager.subscribe(EventType::MouseMoved, [this](Event& event) {
		MouseMoveEvent& mouseEvent = static_cast<MouseMoveEvent&>(event);
		camera.processInput();
		});


	EventManager::getInstance().subscribe(EventType::WindowClose, [this](Event& event) {
		isRunning = false;
		});

	//editorLayer->init(guiManager.get());
}


void Sandbox::run() {
	//pushLayer(editorLayer);

	while (isRunning) {
		appWindow->onUpdate();
		float dt = appWindow->getTime();
		eventManager.onUpdate();
		sceneManager.onUpdate(dt);
		layerManager->onUpdate();

		camera.onUpdate();
		camera.processInput();

		render();
	}
}

void Sandbox::end()
{
	renderer->onClose();
	renderDevice->onClose();
	guiManager->onClose();
	textureManager->onClose();
	appWindow->onClose();
}

void Sandbox::render()
{
	Scene* scene = sceneManager.getActiveScene();
	renderer->render(camera, scene);
}