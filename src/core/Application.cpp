#include "Application.h"
#include <FileWatch.hpp>
#include "../../src/window/AppWindow.h"
#include "../../src/core/features/Timer.h"
#include "../../src/core/layers/AppLayer.h"
#include "../../src/core/features/Profiler.h"
#include "../../src/window/platform/GLFW/AppWindowGLFW.h"


Application::Application(WindowConfig windowConfig) 
	: isRunning(false), windowConfig(windowConfig), editorLayer(nullptr)
{
	ServiceLocator::setContext(&serviceLocator);
}

void Application::pushLayer(Layer* layer)
{
	layerManager->addLayer(layer);
}

void Application::init()
{
	//separate service modules
	engineLogger = platformFactory.createLogger(LoggerPlatform::SPDLOG, "Engine");
	clientLogger = platformFactory.createLogger(LoggerPlatform::SPDLOG, "Client");
	appWindow = platformFactory.createWindow(windowConfig.windowPlatform);
	renderDevice = platformFactory.createRenderDevice(windowConfig.renderPlatform);
	guiManager = platformFactory.createGuiManager(windowConfig.guiPlatform);

	//engine specific features
	layerManager = std::make_unique<LayerManager>(serviceLocator);

	editorLayer = new EditorLayer("EditorLayer");

	services.push_back(appWindow.get());
	services.push_back(guiManager.get());
	services.push_back(engineLogger.get());
	services.push_back(clientLogger.get());

	ServiceLocator::supportingServices();

	renderDevice->init(windowConfig);
	clientLogger->setLevel(LogLevel::Debug); // Set *global* log level to debug

	isRunning = true;

	eventManager.subscribe(EventType::WindowClose, [this](Event& event) {
		onClose();
	});
}

void Application::start()
{
	appWindow->init(windowConfig);
	guiManager->init(windowConfig);
	editorLayer->init(guiManager.get());

	//TODO: experimenting with file watcher for now
	std::unique_ptr<filewatch::FileWatch<std::string>> fileWatcher;
	fileWatcher.reset(new filewatch::FileWatch<std::string>(
		"./Shaders",
		[](const std::string& path, const filewatch::Event change_type) {
			std::cout << path << "-" << (int)change_type << "\n";
		}
	));
}

void Application::run() {
	pushLayer(editorLayer);	// must be the last to be added to layer stack

	while (isRunning) {
		// Application
		appWindow->onUpdate();
		float dt = appWindow->getTime();
		eventManager.onUpdate();
		sceneManager.onUpdate(dt);
		layerManager->onUpdate();

		bool useEditor = true;
		if (useEditor) {
			//TODO GUI should theese be only done by the guiController
			guiManager->start();
			sceneManager.onGuiUpdate(dt);
			layerManager->onGuiUpdate();
			guiManager->end();
		}
	}
}


void Application::end()
{
	guiManager->onClose();
	appWindow->onClose();
}

void Application::onClose()
{
	EventManager::getInstance().onClose();
	isRunning = false;
}