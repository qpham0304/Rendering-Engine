#include "Application.h"
#include <FileWatch.hpp>
#include "../../src/window/AppWindow.h"
#include "../../src/core/features/Timer.h"
#include "../../src/core/layers/AppLayer.h"
#include "../../src/core/features/Profiler.h"
#include "../../src/window/platform/GLFW/AppWindowGLFW.h"

Application::Application() : isRunning(false)
{
	WindowConfig config{};
	config.title = "Application Untitled";
	config.renderPlatform = RenderPlatform::OPENGL;
	config.windowPlatform = WindowPlatform::GLFW;
	config.width = 1920;
	config.height = 1080;
	config.vsync = true;

	//separate service modules
	appWindow = factory.createWindow(config);
	guiController = factory.createGuiManager(GuiPlatform::IMGUI);

	//engine specific features
	layerManager = std::make_unique<LayerManager>(serviceLocator);

	AppWindow::window = appWindow.get();	//TODO: find a better solution if possible

	editorLayer = new EditorLayer("EditorLayer");
}

void Application::pushLayer(Layer* layer)
{
	layerManager->AddLayer(layer);
}

void Application::init()
{
	isRunning = true;

	//std::bind(&Application::onClose, this, std::placeholders::_1);
	eventManager.Subscribe(EventType::WindowClose, [this](Event& event) {
		onClose();
	});
}

void Application::start()
{
	appWindow->start();

	int width = appWindow->width;
	int height = appWindow->height;
	GLFWwindow* window = appWindow->window->getWindow();
	guiController->init(window, appWindow->width, appWindow->height);
	editorLayer->init(guiController.get());

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
			guiController->start();
			sceneManager.onGuiUpdate(glfwGetTime());
			layerManager->onGuiUpdate();
			guiController->end();
		}

		appWindow->onUpdate();
	}
}


void Application::end()
{
	guiController->onClose();
	appWindow->end();
}

void Application::onClose()
{
	for (auto& [thread, status] : EventManager::getInstance().threads) {
		thread.join();
	}
	isRunning = false;
}