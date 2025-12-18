#include "Engine.h"
#include <FileWatch.hpp>
#include "window/AppWindow.h"
#include "core/features/Timer.h"
#include "core/layers/AppLayer.h"
#include "core/features/Profiler.h"
#include "window/platform/GLFW/AppWindowGLFW.h"


Engine::Engine(WindowConfig windowConfig) 
	: isRunning(false), windowConfig(windowConfig)
{
	ServiceLocator::setContext(&serviceLocator);

	//separate service modules
	engineLogger = platformFactory.createLogger(LoggerPlatform::SPDLOG, "Engine");
	clientLogger = platformFactory.createLogger(LoggerPlatform::SPDLOG, "Client");
	appWindow = platformFactory.createWindow(windowConfig.windowPlatform);
	renderDevice = platformFactory.createRenderDevice(windowConfig.renderPlatform);
	guiManager = platformFactory.createGuiManager(windowConfig.guiPlatform);

	//engine specific features
	layerManager = std::make_unique<LayerManager>();


	services.push_back(appWindow.get());
	services.push_back(guiManager.get());
	services.push_back(engineLogger.get());
	services.push_back(clientLogger.get());

	ServiceLocator::supportingServices();

	renderDevice->init(windowConfig);
	clientLogger->setLevel(LogLevel::Debug); // Set *global* log level to debug

	editorLayer = new EditorLayer("EditorLayer", *guiManager);
}

void Engine::pushLayer(Layer* layer) {
	layerManager->addLayer(layer);
}

void Engine::init()
{
	appWindow->init(windowConfig);
	guiManager->init(windowConfig);
	layerManager->init(windowConfig);
	editorLayer->init();
}

void Engine::start()
{
	pushLayer(editorLayer);
	isRunning = true;

	eventManager.subscribe(EventType::WindowClose, [this](Event& event) {
		onClose();
	});

	//TODO: experimenting with file watcher for now
	std::unique_ptr<filewatch::FileWatch<std::string>> fileWatcher;
	fileWatcher.reset(new filewatch::FileWatch<std::string>(
		"./Shaders",
		[](const std::string& m_path, const filewatch::Event change_type) {
			std::cout << m_path << "-" << (int)change_type << "\n";
		}
	));
}

void Engine::run() {	// must be the last to be added to layer stack
	while (isRunning) {
		// Application
		appWindow->onUpdate();
		eventManager.onUpdate();
		sceneManager.onUpdate();
		layerManager->onUpdate();

		bool useEditor = true;
		if (useEditor) {
			//TODO GUI should be done by the guiController
			guiManager->start();
			sceneManager.onGuiUpdate();
			layerManager->onGuiUpdate();
			guiManager->end();
		}
	}
}


void Engine::end()
{
	guiManager->onClose();
	appWindow->onClose();
}

void Engine::onClose()
{
	EventManager::getInstance().onClose();
	isRunning = false;
}