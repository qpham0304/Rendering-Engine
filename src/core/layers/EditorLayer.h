#pragma once

#include <memory>
#include "../../core/scene/SceneManager.h"
#include "../../src/core/events/EventManager.h"
#include "../../core/layers/Layer.h"
class Camera;
class GuiManager;

class EditorLayer : public Layer
{
private:

	Camera* editorCamera;
	SceneManager& sceneManager = SceneManager::getInstance();
	EventManager& eventManager = EventManager::getInstance();
	GuiManager* guiController = nullptr;
	bool GuizmoActive = false;
	bool drawGrid = false;
	bool editorActive = true;
	bool flipUV = true;
	bool faceCamera = false;
	Shader modelShader;
	uint32_t keyEventID;

	void mockThreadTasks();
	void renderGuizmo();

public:
	EditorLayer(const std::string& name = "EditorLayer");
	~EditorLayer() = default;

	void init(GuiManager* controller);
	void onAttach(LayerManager* manager) override;
	void onDetach() override;
	void onUpdate() override;
	void onGuiUpdate() override;
	void onEvent(Event& event) override;
	void handleKeyPressed(int keycode);
};

