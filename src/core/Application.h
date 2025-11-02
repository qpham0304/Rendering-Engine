#pragma once

#include "layers/LayerManager.h"
#include "scene/SceneManager.h"
#include "../../src/core/events/EventManager.h"
#include "layers/EditorLayer.h"
#include "../gui/framework/ImGui/ImGuiController.h"
#include "../../src/core/layers/LayerManager.h"


class AppWindow;

class Application
{
private:
	bool isRunning;
	Application();

public:
	SceneManager& sceneManager = SceneManager::getInstance();
	EventManager& eventManager = EventManager::getInstance();
	LayerManager layerManager;
	std::unique_ptr<GuiManager> guiController;
	std::unique_ptr<AppWindow> appWindow;
	EditorLayer editorLayer;

	~Application() = default;

	static Application& getInstance();

	void run();
	void onClose();
};

