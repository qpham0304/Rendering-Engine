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
public:
	SceneManager& sceneManager = SceneManager::getInstance();
	EventManager& eventManager = EventManager::getInstance();
	std::unique_ptr<LayerManager> layerManager;
	std::unique_ptr<GuiManager> guiManager;
	std::unique_ptr<AppWindow> appWindow;
	EditorLayer* editorLayer;

public:
	Application(WindowConfig windowConfig);
	~Application() = default;

	void pushLayer(Layer* layer);
	void init();
	void start();
	void run();
	void end();
	void onClose();

private:
	bool isRunning;
	WindowConfig windowConfig;
	ServiceLocator serviceLocator;
	PlatformFactory platformFactory{ serviceLocator };
};

