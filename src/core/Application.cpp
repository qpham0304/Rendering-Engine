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
	layerManager->AddLayer(layer);
}

void Application::init()
{
	//separate service modules
	appWindow = platformFactory.createWindow(windowConfig.windowPlatform);
	guiManager = platformFactory.createGuiManager(windowConfig.guiPlatform);

	//engine specific features
	layerManager = std::make_unique<LayerManager>(serviceLocator);

	//Binding platform widow to single ton app window
	//TODO: find a better solution if possible
	AppWindow::window = appWindow.get();

	editorLayer = new EditorLayer("EditorLayer");

	isRunning = true;

	//std::bind(&Application::onClose, this, std::placeholders::_1);
	eventManager.Subscribe(EventType::WindowClose, [this](Event& event) {
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
		eventManager.OnUpdate();
		sceneManager.onUpdate(glfwGetTime());
		layerManager->onUpdate();

		bool useEditor = true;
		if (useEditor) {
			//TODO GUI should theese be only done by the guiController
			guiManager->start();
			sceneManager.onGuiUpdate(glfwGetTime());
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