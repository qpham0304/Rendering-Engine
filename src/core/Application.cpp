#include "Application.h"

#include "../core/features/AppWindow.h"
#include "../../src/apps/particle-demo/ParticleDemo.h"
#include "../../src/apps/deferred-IBL-demo/deferredIBL_demo.h"
#include "../../src/apps/volumetric-light/VolumetricLightDemo.h"
#include "features/Timer.h"
#include "layers/AppLayer.h"
#include "FileWatch.hpp"
#include "features/Profiler.h"

Application::Application() : isRunning(false)
{
	AppWindow::init(PLATFORM_OPENGL);
}

Application& Application::getInstance()
{
	static Application application;
	return application;
}

void Application::run() {
	isRunning = true;
	AppWindow::start("Application");

	int width = AppWindow::width;
	int height = AppWindow::height;
	guiController.init(AppWindow::window, width, height);
	editorLayer.init(guiController);

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
		editorLayer.onUpdate();		// render editor as overlay

		//GUI
		guiController.start();
		sceneManager.onGuiUpdate(glfwGetTime());
		editorLayer.onGuiUpdate();	// also render ui after to show overlay
		guiController.end();

		AppWindow::onUpdate();
	}
	AppWindow::end();
}

void Application::onClose()
{
	for (auto& [thread, status] : EventManager::getInstance().threads) {
		thread.join();
	}
	isRunning = false;
}