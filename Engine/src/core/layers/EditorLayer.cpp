#include "EditorLayer.h"
#include "window/appwindow.h"
#include "core/features/Timer.h"
#include "core/events/EventManager.h"
#include "core/layers/AppLayer.h"
#include "core/components/MComponent.h"
#include "core/components/cameracomponent.h"
#include "window/Input.h"
#include "core/layers/layerManager.h"
#include "gui/GuiManager.h"
#include "core/features/Camera.h"
#include "imgui.h"

void EditorLayer::mockThreadTasks()
{
	AsyncEvent addComponentEvent;
	auto func = [](Event& event) -> void {
		//Component reimu("assets/Models/reimu/reimu.obj");
		//SceneManager::addComponent(reimu);
	};
	eventManager.queue(addComponentEvent, func);

	AsyncEvent addComponentEvent1;
	auto func1 = [](Event& event) -> void {
		//Component reimu("assets/Models/sponza/sponza.obj");
		//SceneManager::addComponent(reimu);
	};
	eventManager.queue(addComponentEvent1, func1);

	AsyncEvent addComponentEvent2;
	auto func2 = [](Event& event) -> void {
		//Component reimu("assets/Models/aru/aru.gltf");
		//SceneManager::addComponent(reimu);
	};
	eventManager.queue(addComponentEvent2, func2);
}

void EditorLayer::renderGuizmo()
{
	Scene* scene = SceneManager::getInstance().getActiveScene();
	std::vector<Entity> selectedEntities = scene->getSelectedEntities();

	if (!selectedEntities.empty()) {
		auto& transformComponent = selectedEntities[0].getComponent<TransformComponent>();
		guiController.renderGuizmo(transformComponent);
	}
}

EditorLayer::EditorLayer(const std::string& name, GuiManager& controller)
	: Layer (name), guiController(controller)
{

}

EditorLayer::~EditorLayer()
{
	
}

bool EditorLayer::init()
{
	guiController.useDarkTheme();
	return true;
}

void EditorLayer::onAttach(LayerManager* manager)
{
	Layer::onAttach(manager);

	if (!SceneManager::cameraController) {
		return;
	}

	editorCamera = SceneManager::cameraController;

	//eventManager.subscribe(EventType::AnimationLoadEvent, [](Event& event) {
	//	AnimationLoadEvent& e = static_cast<AnimationLoadEvent&>(event);
	//	if (!e.entity.hasComponent<AnimationComponent>()) {
	//		e.entity.addComponent<AnimationComponent>();
	//	}

	//	AnimationComponent& animationComponent = e.entity.getComponent<AnimationComponent>();
	//	ModelComponent& modelComponent = e.entity.getComponent<ModelComponent>();
	//	animationComponent.path = "Loading...";
		//std::string uuid = SceneManager::getInstance().addAnimation(e.path.c_str(), modelComponent.model.lock().get());

		//if (animationComponent.path != e.path && !uuid.empty()) {
		//	animationComponent.animation = SceneManager::getInstance().animations[uuid];
		//	animationComponent.animator.Init(SceneManager::getInstance().animations[uuid].get());
		//	animationComponent.path = e.path;
		//}

		//else {
		//	ImGui::OpenPopup("Failed to load file, please check the format");
		//	animationComponent.reset();
		//}
	//});


	eventManager.subscribe(EventType::MouseMoved, [&](Event& event) {
		MouseMoveEvent& mouseEvent = static_cast<MouseMoveEvent&>(event);

		if (GuizmoActive && editorActive) {
			mouseEvent.Handled = true;	// block mouse event from other layers
		}
	});

	keyEventID = eventManager.subscribe(EventType::KeyPressed, [&](Event& event) {
		KeyPressedEvent& keyPressedEvent = static_cast<KeyPressedEvent&>(event);
		if (GuizmoActive || editorActive) {
			handleKeyPressed(keyPressedEvent.keyCode);
			keyPressedEvent.Handled = true;	// block keyboard event from other layers
		}
	});
}

void EditorLayer::onDetach()
{

}

void EditorLayer::onUpdate()
{
	
}

void EditorLayer::onGuiUpdate()
{
	
}

void EditorLayer::onEvent(Event& event)
{

}

void EditorLayer::handleKeyPressed(int keycode)
{
	switch (keycode) {
		case KEY_T:	guiController.guizmoTranslate(); break;
		case KEY_R:	guiController.guizmoRotate(); break;
		case KEY_Z:	guiController.guizmoScale(); break;
		case KEY_DELETE: {
			Scene* scene = SceneManager::getInstance().getActiveScene();
			std::vector<Entity> selectedEntities = scene->getSelectedEntities();
			if (!selectedEntities.empty()) {
				scene->removeEntity(selectedEntities[0].getID());
			}
			break;
		}
		case KEY_G:	eventManager.unsubscribe(EventType::KeyPressed, keyEventID); break;
	}
}
