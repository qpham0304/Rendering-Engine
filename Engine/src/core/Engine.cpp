#include "Engine.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <ranges>
#include "window/AppWindow.h"
#include "core/events/EventManager.h"
#include <chrono>
#include <thread>

#include <core/layers/EditorLayer.h>
#include <core/scene/SceneManager.h>
#include <core/resources/managers/TextureManager.h>
#include <core/resources/managers/BufferManager.h>
#include <core/resources/managers/MeshManager.h>
#include <core/resources/managers/ModelManager.h>
#include <core/resources/managers/DescriptorManager.h>
#include <core/resources/managers/MaterialManager.h>
#include <graphics/renderers/Renderer.h>

Engine::Engine(WindowConfig config)
	: windowConfig(config),
	sceneManager(SceneManager::getInstance()),
	eventManager(EventManager::getInstance())
{
	ServiceLocator::setContext(&serviceLocator);
	engineLogger = platformFactory.Create(LoggerPlatform::SPDLOG, "Engine");
	clientLogger = platformFactory.Create(LoggerPlatform::SPDLOG, "Client");
	appWindow = platformFactory.Create<AppWindow>(windowConfig.windowPlatform);
	renderDevice = platformFactory.Create<RenderDevice>(windowConfig.renderPlatform);
	bufferManager = platformFactory.Create<BufferManager>(windowConfig.renderPlatform);
	descriptorManager = platformFactory.Create<DescriptorManager>(windowConfig.renderPlatform);
	textureManager = platformFactory.Create<TextureManager>(windowConfig.renderPlatform);
	materialManager = platformFactory.Create<MaterialManager>(windowConfig.renderPlatform);

	meshManager = std::make_unique<MeshManager>();
	modelManager = std::make_unique<ModelManager>();
	layerManager = std::make_unique<LayerManager>();
	serviceLocator.Register<MeshManager>("MeshManager", *meshManager);
	serviceLocator.Register<ModelManager>("ModelManager", *modelManager);
	serviceLocator.Register<LayerManager>("LayerManager", *layerManager);

	guiManager = platformFactory.Create<GuiManager>(windowConfig.guiPlatform);
	rendererManager = platformFactory.Create<RendererManager>(windowConfig.renderPlatform);

	//NOTE: setup order is important!
	services.push_back(&eventManager);
	services.push_back(&sceneManager);
	services.push_back(appWindow.get());
	services.push_back(renderDevice.get());
	services.push_back(bufferManager.get());
	services.push_back(descriptorManager.get());
	services.push_back(textureManager.get());
	services.push_back(materialManager.get());
	services.push_back(meshManager.get());
	services.push_back(modelManager.get());
	services.push_back(guiManager.get());
	services.push_back(layerManager.get());
	services.push_back(rendererManager.get());
}

void Engine::pushLayer(Layer* layer)
{
	layerManager->addLayer(layer);
}

void Engine::init()
{
	engineLogger->setLevel(LogLevel::Debug);
	
	for (Service*& service : services) {
		if (!service->init(windowConfig)) {
			engineLogger->error("Service Initilize failed: {}", service->getServiceName());
		}
		else {
			engineLogger->debug("Initilize Service: {}", service->getServiceName());
		}
	}
	
}

void Engine::start()
{
	pushLayer(new EditorLayer("EditorLayer", *guiManager));
	
	eventManager.subscribe(EventType::WindowClose, [this](Event& event) {
		isRunning = false;
	});

	eventManager.subscribe(EventType::KeyPressed, [this](Event& event) {
		KeyPressedEvent& keyPressedEvent = static_cast<KeyPressedEvent&>(event);
		if (keyPressedEvent.keyCode == KEY_ESCAPE) {
			isRunning = false;
		}
	});
}

void Engine::run() {
    const int maxUpdates = 5;
    const double targetUpdateTime = 1.0 / windowConfig.targetUpdateFPS;
    const double targetRenderTime = 1.0 / windowConfig.targetRenderFPS;

    double accumulator = 0.0;
    double lastTime = AppWindow::getTime();
    double lastRenderTime = lastTime;
	int updatesThisFrame = 0;

	// fixed timestep update and variable rendering
    while (isRunning) {
        double currentTime = AppWindow::getTime();
        double deltaTime = currentTime - lastTime;
        lastTime = currentTime;
		deltaTime = deltaTime > 0.25 ? 0.25 : deltaTime;
        accumulator += deltaTime;
		updatesThisFrame = 0;
		
        while(accumulator >= targetUpdateTime && updatesThisFrame < maxUpdates) {
            for (Service* service : services) {
                service->onUpdate();
            }
            accumulator -= targetUpdateTime;
            updatesThisFrame++;
        }

        double timeSinceLastRender = currentTime - lastRenderTime;
        if(timeSinceLastRender >= targetRenderTime) {
            rendererManager->render();
            lastRenderTime = currentTime;
        }
    }
}

void Engine::close()
{
	for (Service*& service : std::views::reverse(services)) {
		if (!service->onClose()) {
			engineLogger->error("Service Close failed: {}", service->getServiceName());
		}
		else {
			engineLogger->debug("Closing Service: {}", service->getServiceName());
		}
	}
}
