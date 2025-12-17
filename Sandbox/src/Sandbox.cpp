#include "Sandbox.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>
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
	bufferManager = platformFactory.createBufferManager(windowConfig.renderPlatform);
	descriptorManager = platformFactory.createDescriptorManager(windowConfig.renderPlatform);
	materialManager = platformFactory.createMaterialManager(windowConfig.renderPlatform);

	meshManager = std::make_unique<MeshManager>();
	modelManager = std::make_unique<ModelManager>();
	layerManager = std::make_unique<LayerManager>(serviceLocator);

	serviceLocator.Register<MeshManager>("MeshManager", *meshManager);
	serviceLocator.Register<ModelManager>("ModelManager", *modelManager);
	serviceLocator.Register<LayerManager>("LayerManager", *layerManager);


	engineLogger->setLevel(LogLevel::Debug);
	ServiceLocator::supportingServices();


	editorLayer = new EditorLayer("EditorLayer", *guiManager);


	//NOTE: setup order of adding is important!
	services.push_back(engineLogger.get());
	services.push_back(clientLogger.get());
	services.push_back(appWindow.get());
	services.push_back(renderDevice.get());
	services.push_back(bufferManager.get());
	services.push_back(descriptorManager.get());
	services.push_back(textureManager.get());
	services.push_back(materialManager.get());
	services.push_back(renderer.get());
	services.push_back(meshManager.get());
	services.push_back(modelManager.get());
	services.push_back(guiManager.get());
}

void Sandbox::start()
{
	for (Service*& service : services) {
		service->init(windowConfig);
	}
	renderer->addModel("assets/models/aru/aru.gltf");
	//renderer->addModel("assets/models/DamagedHelmet/gltf/DamagedHelmet.gltf");
	//renderer->addModel("assets/models/sponza/sponza.obj");
	//renderer->addModel("assets/models/cube/cube-notex.gltf");


	camera.init(
		AppWindow::getWidth(), 
		AppWindow::getHeight(),
		glm::vec3(5.0, 0.0, 0.0),
		glm::normalize(glm::vec3(-5.0, -0.0, -0.0))
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
		Timer("Render loop time", true);
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
	renderDevice->onClose();
	bufferManager->onClose();
	renderer->onClose();
	guiManager->onClose();
	textureManager->onClose();
	descriptorManager->onClose();
	appWindow->onClose();
}

void Sandbox::render()
{
	Scene* scene = sceneManager.getActiveScene();
	renderer->render(camera, scene);
}