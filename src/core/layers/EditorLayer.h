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
	void OnAttach(LayerManager* manager) override;
	void OnDetach() override;
	void OnUpdate() override;
	void OnGuiUpdate() override;
	void OnEvent(Event& event) override;
	void handleKeyPressed(int keycode);
};

