#include "Application.h"
#include <FileWatch.hpp>
#include "../../src/window/AppWindow.h"
#include "../../src/core/features/Timer.h"
#include "../../src/core/layers/AppLayer.h"
#include "../../src/core/features/Profiler.h"
#include "../../src/window/platform/GLFW/AppWindowGLFW.h"

Application::Application(WindowConfig windowConfig) 
	: isRunning(false), windowConfig(windowConfig)
{

}

void Application::pushLayer(Layer* layer)
{
	layerManager->addLayer(layer);
}

void Application::init()
{
	//separate service modules
	appWindow = platformFactory.createWindow(windowConfig.windowPlatform);
	guiManager = platformFactory.createGuiManager(windowConfig.guiPlatform);
	engineLogger = platformFactory.createLogger(LoggerPlatform::SPDLOG, "Engine");
	clientLogger = platformFactory.createLogger(LoggerPlatform::SPDLOG, "Client");
	//engine specific features
	layerManager = std::make_unique<LayerManager>(serviceLocator);

	editorLayer = new EditorLayer("EditorLayer");

	services.push_back(appWindow.get());
	services.push_back(guiManager.get());
	services.push_back(engineLogger.get());
	services.push_back(clientLogger.get());


	engineLogger->warn(appWindow->getServiceName());
	engineLogger->warn(guiManager->getServiceName());
	engineLogger->warn(engineLogger->getServiceName());
	engineLogger->warn(clientLogger->getServiceName());

	engineLogger->info("Welcome to spdlog!");
	engineLogger->error("Some error message with arg: {}", 1);

	engineLogger->warn("Easy padding in numbers like {:08d}", 12);
	engineLogger->critical("Support for int: {0:d};  hex: {0:x};  oct: {0:o}; bin: {0:b}", 42);
	engineLogger->info("Support for floats {:03.2f}", 1.23456);
	clientLogger->info("Positional args are {1} {0}..", "too", "supported");
	clientLogger->info("{:<30}", "left aligned");

	clientLogger->setLevel(LogLevel::Debug); // Set *global* log level to debug
	clientLogger->debug("This message should be displayed..");

	isRunning = true;

	//std::bind(&Application::onClose, this, std::placeholders::_1);
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
		float dt = appWindow->getTime();
		// Application
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
		appWindow->onUpdate();
	}
}


void Application::end()
{
	guiManager->onClose();
	appWindow->onClose();
}

void Application::onClose()
{
	for (auto& [thread, status] : EventManager::getInstance().threads) {
		thread.join();
	}
	isRunning = false;
}