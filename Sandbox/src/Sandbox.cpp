#include "Sandbox.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <ranges>
#include "window/AppWindow.h"
#include "core/events/EventManager.h"

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

Sandbox::Sandbox(WindowConfig config)
{
	windowConfig = config;

	ServiceLocator::setContext(&serviceLocator);
	engineLogger = platformFactory.createLogger(LoggerPlatform::SPDLOG, "Engine");
	clientLogger = platformFactory.createLogger(LoggerPlatform::SPDLOG, "Client");
	appWindow = platformFactory.createWindow(windowConfig.windowPlatform);
	renderDevice = platformFactory.createRenderDevice(windowConfig.renderPlatform);
	bufferManager = platformFactory.createBufferManager(windowConfig.renderPlatform);
	descriptorManager = platformFactory.createDescriptorManager(windowConfig.renderPlatform);
	textureManager = platformFactory.createTextureManager(windowConfig.renderPlatform);
	materialManager = platformFactory.createMaterialManager(windowConfig.renderPlatform);
	
	meshManager = std::make_unique<MeshManager>();
	modelManager = std::make_unique<ModelManager>();
	layerManager = std::make_unique<LayerManager>();
	serviceLocator.Register<MeshManager>("MeshManager", *meshManager);
	serviceLocator.Register<ModelManager>("ModelManager", *modelManager);
	serviceLocator.Register<LayerManager>("LayerManager", *layerManager);

	renderer = platformFactory.createRenderer(windowConfig.renderPlatform);
	guiManager = platformFactory.createGuiManager(windowConfig.guiPlatform);

	//NOTE: setup order is important!
	services.push_back(&eventManager);
	services.push_back(&sceneManager);
	services.push_back(engineLogger.get());
	services.push_back(clientLogger.get());
	services.push_back(appWindow.get());
	services.push_back(renderDevice.get());
	services.push_back(bufferManager.get());
	services.push_back(descriptorManager.get());
	services.push_back(textureManager.get());
	services.push_back(materialManager.get());
	services.push_back(meshManager.get());
	services.push_back(modelManager.get());
	services.push_back(layerManager.get());
	services.push_back(guiManager.get());
	services.push_back(renderer.get());
}

void Sandbox::pushLayer(Layer *layer)
{
	layerManager->addLayer(layer);
}

void Sandbox::init()
{
	engineLogger->setLevel(LogLevel::Debug);
	
	// pushLayer(new EditorLayer("EditorLayer", *guiManager));

	for (Service*& service : services) {
		if(!service->init(windowConfig)) {	// assuming logger is always success
			engineLogger->critical("Service Initilize failed: {}", service->getServiceName());
		} else {
			engineLogger->info("Initilize Service: {}", service->getServiceName());
		}
	}
}

void Sandbox::start()
{
	renderer->addModel("assets/models/aru/aru.gltf");
	//renderer->addModel("assets/models/DamagedHelmet/gltf/DamagedHelmet.gltf");
	//renderer->addModel("assets/models/sponza/sponza.obj");
	//renderer->addModel("assets/models/cube/cube-notex.gltf");

	camera.init(
		AppWindow::getWidth(), 
		AppWindow::getHeight(),
		glm::vec3(3.0f),
		glm::normalize(glm::vec3(-3.0f))
	);


	eventManager.subscribe(EventType::WindowResize, [this](Event& event) {
		WindowResizeEvent& windowResizeEvent = static_cast<WindowResizeEvent&>(event);
		camera.updateViewResize(windowResizeEvent.m_width, windowResizeEvent.m_height);
	});

	eventManager.subscribe(EventType::MouseScrolled, [this](Event& event) {
		MouseScrollEvent& mouseEvent = static_cast<MouseScrollEvent&>(event);
		camera.scroll_callback(mouseEvent.m_x, mouseEvent.m_y);
	});

	eventManager.subscribe(EventType::MouseMoved, [this](Event& event) {
		MouseMoveEvent& mouseEvent = static_cast<MouseMoveEvent&>(event);
		camera.processInput();
	});

	eventManager.subscribe(EventType::WindowClose, [this](Event& event) {
		isRunning = false;
	});

}


void Sandbox::run() {
	while (isRunning) {
		for (Service*& service : services) {
			service->onUpdate();
		}

		camera.onUpdate();
		camera.processInput();

		Scene* scene = sceneManager.getActiveScene();
		renderer->render(camera, scene);
	}
}

void Sandbox::close()
{
	for (Service*& service : std::views::reverse(services)) {
		if(!service->onClose()) {	// assuming logger is always success
			engineLogger->critical("Service Close failed: {}", service->getServiceName());
		} else {
			engineLogger->info("Closing Service: {}", service->getServiceName());
		}
	}
}
