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
#include "imgui.h"	// TODO: remove dependency with imgui once gui interface is setup

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

bool EditorLayer::init()
{
	guiController.useDarkTheme();
	sceneManager.addScene("default");
	return true;
}

void EditorLayer::onAttach(LayerManager* manager)
{
	Layer::onAttach(manager);

	if (!SceneManager::cameraController) {
		editorCamera = new Camera();
		editorCamera->init(AppWindow::getWidth(), AppWindow::getHeight(), glm::vec3(1.0, 0.0, 0.0), glm::vec3(1.0));
		SceneManager::cameraController = editorCamera;
	}
	else {
		editorCamera = SceneManager::cameraController;
	}

	eventManager.subscribe(EventType::ModelLoadEvent, [](Event& event) {
		ModelLoadEvent& e = static_cast<ModelLoadEvent&>(event);
		if (!e.entity.hasComponent<ModelComponent>()) {
			e.entity.addComponent<ModelComponent>();
		}
		
		ModelComponent& component = e.entity.getComponent<ModelComponent>();
		component.path = "Loading...";
		std::string uuid = SceneManager::getInstance().addModel(e.path.c_str());
		
		if (component.path != e.path && !uuid.empty()) {
			component.model = SceneManager::getInstance().models[uuid];
			component.path = e.path;
		}
		
		else {
			ImGui::OpenPopup("Failed to load file, please check the format");
			component.reset();
		}
	});

	eventManager.subscribe(EventType::AnimationLoadEvent, [](Event& event) {
		AnimationLoadEvent& e = static_cast<AnimationLoadEvent&>(event);
		if (!e.entity.hasComponent<AnimationComponent>()) {
			e.entity.addComponent<AnimationComponent>();
		}

		AnimationComponent& animationComponent = e.entity.getComponent<AnimationComponent>();
		ModelComponent& modelComponent = e.entity.getComponent<ModelComponent>();
		animationComponent.path = "Loading...";
		std::string uuid = SceneManager::getInstance().addAnimation(e.path.c_str(), modelComponent.model.lock().get());

		if (animationComponent.path != e.path && !uuid.empty()) {
			animationComponent.animation = SceneManager::getInstance().animations[uuid];
			animationComponent.animator.Init(SceneManager::getInstance().animations[uuid].get());
			animationComponent.path = e.path;
		}

		else {
			ImGui::OpenPopup("Failed to load file, please check the format");
			animationComponent.reset();
		}
	});


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
	editorCamera->onUpdate();
	
	Scene& scene = *SceneManager::getInstance().getActiveScene();

	for (auto& [id, entity] : scene.entities) {
		if (entity.hasComponent<CameraComponent>()) {
			ModelComponent& modelComponent = entity.getComponent<ModelComponent>();
			TransformComponent& transform = entity.getComponent<TransformComponent>();
			glm::mat4 viewMatrix = SceneManager::cameraController->getViewMatrix();
			const glm::mat4& modelMatrix = transform.getModelMatrix();
			std::shared_ptr<ModelOpenGL> model = modelComponent.model.lock();

			if (model != nullptr) {
				
			}
		}
	}

}

void EditorLayer::onGuiUpdate()
{
	guiController.render();
	//if(ImGui::Button("addmock data")){
	//	mockThreadTasks();
	//}

	if (ImGui::Begin("Application window")) {
		ImGui::BeginChild("Child");
		ImGui::SetNextItemAllowOverlap();
		renderGuizmo();
		ImGui::EndChild();
		ImGui::End();
	}

	std::string id;
	ImGui::Begin("Layers");
	Scene* scene = sceneManager.getActiveScene();
	
	if (ImGui::Button("add demo layer")) {

	}
	
	if (ImGui::Button("add bloom layer")) {

	}

	if (ImGui::Button("remove layer")) {

	}
	ImGui::End();
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
