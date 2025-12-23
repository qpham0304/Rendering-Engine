#include "Sandbox.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <ranges>
#include "window/AppWindow.h"
#include "core/events/EventManager.h"


#include <core/layers/EditorLayer.h>
#include <core/layers/LayerManager.h>
#include <core/scene/SceneManager.h>
#include <core/resources/managers/TextureManager.h>
#include <core/resources/managers/BufferManager.h>
#include <core/resources/managers/MeshManager.h>
#include <core/resources/managers/ModelManager.h>
#include <core/resources/managers/DescriptorManager.h>
#include <core/resources/managers/MaterialManager.h>
#include <graphics/renderers/Renderer.h>

Sandbox::Sandbox(WindowConfig config)
	:	windowConfig(config),
		sceneManager(SceneManager::getInstance()),
		eventManager(EventManager::getInstance())
{
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

	guiManager = platformFactory.createGuiManager(windowConfig.guiPlatform);
	renderer = platformFactory.createRenderer(windowConfig.renderPlatform);

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
	
	pushLayer(new EditorLayer("EditorLayer", *guiManager));

	for (Service*& service : services) {
		if(!service->init(windowConfig)) {	// assuming logger is always success
			engineLogger->error("Service Initilize failed: {}", service->getServiceName());
		} else {
			engineLogger->info("Initilize Service: {}", service->getServiceName());
		}
	}
}

void Sandbox::start()
{
	eventManager.subscribe(EventType::KeyPressed, [this](Event& event) {
		KeyPressedEvent& keyPressedEvent = static_cast<KeyPressedEvent&>(event);
		if(keyPressedEvent.keyCode == KEY_ESCAPE){
			isRunning = false;
		}
	});
}

void Sandbox::run() {
	while (isRunning) {
		for (Service*& service : services) {
			service->onUpdate();
		}
	}
}

void Sandbox::close()
{
	for (Service*& service : std::views::reverse(services)) {
		if(!service->onClose()) {	// assuming logger is always success
			engineLogger->error("Service Close failed: {}", service->getServiceName());
		} else {
			engineLogger->info("Closing Service: {}", service->getServiceName());
		}
	}
}
