#pragma once

#include "core/layers/LayerManager.h"
#include "core/scene/SceneManager.h"
#include "core/events/EventManager.h"
#include "core/features/ServiceLocator.h"
#include "core/features/PlatformFactory.h"
#include "core/layers/EditorLayer.h"

class Engine
{
public:
	std::unique_ptr<GuiManager> guiManager;

public:
	Engine(WindowConfig windowConfig);
	~Engine() = default;

	void pushLayer(Layer* layer);

	void init();
	void start();
	void run();
	void end();
	void onClose();

private:
	std::vector<Service*> services;
	bool isRunning;
	WindowConfig windowConfig;
	ServiceLocator serviceLocator;
	PlatformFactory platformFactory{ serviceLocator };

	SceneManager& sceneManager = SceneManager::getInstance();
	EventManager& eventManager = EventManager::getInstance();
	std::unique_ptr<LayerManager> layerManager;
	std::unique_ptr<AppWindow> appWindow;
	std::unique_ptr<RenderDevice> renderDevice;
	std::unique_ptr<Logger> engineLogger;
	std::unique_ptr<Logger> clientLogger;
	EditorLayer* editorLayer;
};

