#pragma once

#include "layers/LayerManager.h"
#include "scene/SceneManager.h"
#include "../../src/core/events/EventManager.h"
#include "layers/EditorLayer.h"
#include "../gui/framework/ImGui/ImGuiController.h"
#include "../../src/core/layers/LayerManager.h"
#include "features/ServiceLocator.h"
#include "features/PlatformFactory.h"


class Application
{
private:
	bool isRunning;
	ServiceLocator serviceLocator;
	PlatformFactory factory{ serviceLocator };


public:
	Application();
	
	SceneManager& sceneManager = SceneManager::getInstance();
	EventManager& eventManager = EventManager::getInstance();
	std::unique_ptr<LayerManager> layerManager;
	std::unique_ptr<GuiManager> guiController;
	std::unique_ptr<AppWindow> appWindow;
	EditorLayer* editorLayer;

	~Application() = default;

	void pushLayer(Layer* layer);
	void init();
	void start();
	void run();
	void end();
	void onClose();
};

