#pragma once

#include "../../src/core/layers/LayerManager.h"
#include "../../src/core/scene/SceneManager.h"
#include "../../src/core/events/EventManager.h"
#include "../../src/core/features/ServiceLocator.h"
#include "../../src/core/features/PlatformFactory.h"
#include "../../src/core/layers/EditorLayer.h"

class Application
{
public:
	SceneManager& sceneManager = SceneManager::getInstance();
	EventManager& eventManager = EventManager::getInstance();
	std::unique_ptr<LayerManager> layerManager;
	std::unique_ptr<GuiManager> guiManager;
	std::unique_ptr<AppWindow> appWindow;
	std::unique_ptr<Logger> engineLogger;
	std::unique_ptr<Logger> clientLogger;
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

