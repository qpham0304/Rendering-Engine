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
	logger = platformFactory.createLogger(LoggerPlatform::SPDLOG, "Logger");

	//engine specific features
	layerManager = std::make_unique<LayerManager>(serviceLocator);

	//Binding platform widow to single ton app window
	//TODO: find a better solution if possible
	AppWindow::window = appWindow.get();

	editorLayer = new EditorLayer("EditorLayer");


	logger->info("Welcome to spdlog!");
	logger->error("Some error message with arg: {}", 1);

	logger->warn("Easy padding in numbers like {:08d}", 12);
	logger->critical("Support for int: {0:d};  hex: {0:x};  oct: {0:o}; bin: {0:b}", 42);
	logger->info("Support for floats {:03.2f}", 1.23456);
	logger->info("Positional args are {1} {0}..", "too", "supported");
	logger->info("{:<30}", "left aligned");

	logger->setLevel(LogLevel::Debug); // Set *global* log level to debug
	logger->debug("This message should be displayed..");

	// Compile time log levels
	// Note that this does not change the current log level, it will only
	// remove (depending on SPDLOG_ACTIVE_LEVEL) the call on the release code.

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