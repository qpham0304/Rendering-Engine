#pragma once

#include <memory>
#include "core/scene/SceneManager.h"
#include "core/events/EventManager.h"
#include "core/layers/Layer.h"

class Camera;
class GuiManager;

class EditorLayer : public Layer
{
private:
	SceneManager& sceneManager = SceneManager::getInstance();
	EventManager& eventManager = EventManager::getInstance();
	GuiManager& guiController;
	std::unique_ptr<Camera> editorCamera;
	bool GuizmoActive = false;
	bool drawGrid = false;
	bool editorActive = false;
	bool flipUV = true;
	bool faceCamera = false;
	EventSubscription keyEventSubscription;

	void mockThreadTasks();
	void renderGuizmo();

public:
	EditorLayer(const std::string& name, GuiManager& controller);
	~EditorLayer();

	bool init() override;
	void onAttach(LayerManager* manager) override;
	void onDetach() override;
	void onUpdate(const FrameTime& frameTime) override;
	void onGuiUpdate() override;
	void onEvent(Event& event) override;
	void handleKeyPressed(int keycode);
};

