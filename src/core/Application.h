#pragma once

#include "layers/LayerManager.h"
#include "scene/SceneManager.h"
#include "../../src/core/events/EventManager.h"
#include "layers/EditorLayer.h"
#include "../gui/framework/ImGui/ImGuiController.h"

struct AppConfig {

};

class Application
{
private:
	bool isRunning;
	Application();

public:
	SceneManager& sceneManager = SceneManager::getInstance();
	EventManager& eventManager = EventManager::getInstance();
	ImGuiController guiController;
	EditorLayer editorLayer;

	~Application() = default;

	static Application& getInstance();

	void run();
	void onClose();
};

