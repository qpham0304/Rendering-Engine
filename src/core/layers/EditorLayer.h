#pragma once

#include <memory>
#include "../../core/scene/SceneManager.h"
#include "../../events/EventManager.h"
#include "../../gui/GuiController.h"

class EditorLayer
{
private:
	//class GuiController;

	Camera editorCamera;
	SceneManager& sceneManager = SceneManager::getInstance();
	EventManager& eventManager = EventManager::getInstance();
	GuiController* guiController = nullptr;
	bool GuizmoActive = false;
	bool drawGrid = false;
	bool editorActive = true;
	bool flipUV = true;
	bool faceCamera = false;
	ImGuizmo::OPERATION GuizmoType = ImGuizmo::OPERATION::TRANSLATE;
	Shader modelShader;
	uint32_t keyEventID;

	void mockThreadTasks();
	void renderGuizmo();

public:
	EditorLayer();
	~EditorLayer() = default;

	void init(GuiController* controller);
	void onAttach();
	void onDetach();
	void onUpdate();
	void onGuiUpdate();
	void onEvent(Event& event);
	void handleKeyPressed(int keycode);
};

