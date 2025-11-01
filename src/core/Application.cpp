#include "Application.h"
#include <FileWatch.hpp>

#include "../../src/window/AppWindow.h"
#include "../../src/core/features/Timer.h"
#include "../../src/core/layers/AppLayer.h"
#include "../../src/core/features/Profiler.h"
#include "../../src/window/platform/GLFW/AppWindowGLFW.h"
#include "features/PlatformFactory.h"

Application::Application() : isRunning(false)
{
	WindowConfig config{};
	config.title = "Application Untitled";
	config.renderPlatform = RenderPlatform::OPENGL;
	config.windowPlatform = WindowPlatform::GLFW;
	config.width = 1920;
	config.height = 1080;
	config.vsync = true;

	PlatformFactory& factory = PlatformFactory::getInstance();
	appWindow = factory.createWindow(config);
	AppWindow::window = appWindow.get();	//TODO: find a better solution if possible

	guiController = factory.createGuiManager(GuiPlatform::IMGUI);
}

Application& Application::getInstance()
{
	static Application application;
	return application;
}

void Application::run() {
	isRunning = true;
	appWindow->start();

	int width = appWindow->width;
	int height = appWindow->height;
	printf("width: %d, height: %d", width, height);
	GLFWwindow* window = appWindow->window->getWindow();
	guiController->init(window, appWindow->width, appWindow->height);
	editorLayer.init(guiController.get());

	//std::bind(&Application::onClose, this, std::placeholders::_1);
	eventManager.Subscribe(EventType::WindowClose, [this](Event& event) {
		onClose();
		});

	//TODO: experimenting with file watcher for now
	std::unique_ptr<filewatch::FileWatch<std::string>> fileWatcher;
	fileWatcher.reset(new filewatch::FileWatch<std::string>(
		"./Shaders",
		[](const std::string& path, const filewatch::Event change_type) {
			std::cout << path << "-" << (int)change_type << "\n";
		}
	));

	while (isRunning) {
		// Application
		eventManager.OnUpdate();
		sceneManager.onUpdate(glfwGetTime());
		layerManager.onUpdate();

		bool useEditor = true;
		if (useEditor) {
			editorLayer.onUpdate();		// render editor as overlay

			//GUI
			guiController->start();
			sceneManager.onGuiUpdate(glfwGetTime());
			layerManager.onGuiUpdate();
			editorLayer.onGuiUpdate();	// also render ui after to show overlay
			guiController->end();
		}

		appWindow->onUpdate();
	}
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