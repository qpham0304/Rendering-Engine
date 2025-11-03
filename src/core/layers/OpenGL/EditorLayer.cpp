#include "../EditorLayer.h"

#include "../../src/window/appwindow.h"
#include "../../src/core/features/Timer.h"
#include "../../src/core/events/EventManager.h"
#include "../../src/core/layers/AppLayer.h"
#include "../../src/core/layers/BloomLayer.h"
#include "../../src/core/components/MComponent.h"
#include "../../src/core/components/cameracomponent.h"
#include "../../src/window/Input.h"
#include "../../src/core/Application.h"
#include "../../src/core/layers/layerManager.h"
//#include "../../src/gui/GuiController.h"
#include "camera.h"


void EditorLayer::mockThreadTasks()
{
	AsyncEvent addComponentEvent;
	auto func = [](Event& event) -> void {
		//Component reimu("Models/reimu/reimu.obj");
		//SceneManager::addComponent(reimu);
	};
	eventManager.Queue(addComponentEvent, func);

	AsyncEvent addComponentEvent1;
	auto func1 = [](Event& event) -> void {
		//Component reimu("Models/sponza/sponza.obj");
		//SceneManager::addComponent(reimu);
	};
	eventManager.Queue(addComponentEvent1, func1);

	AsyncEvent addComponentEvent2;
	auto func2 = [](Event& event) -> void {
		//Component reimu("Models/aru/aru.gltf");
		//SceneManager::addComponent(reimu);
	};
	eventManager.Queue(addComponentEvent2, func2);
}

void EditorLayer::renderGuizmo()
{
	ImGuizmo::BeginFrame();
	glm::vec3 translateVector(0.0f, 0.0f, 0.0f);
	glm::vec3 scaleVector(1.0f, 1.0f, 1.0f);

	float viewManipulateRight = ImGui::GetIO().DisplaySize.x;
	float viewManipulateTop = 0;

	auto v = &SceneManager::cameraController->getViewMatrix()[0][0];
	auto p = glm::value_ptr(SceneManager::cameraController->getProjectionMatrix());
	Scene* scene = SceneManager::getInstance().getActiveScene();
	std::vector<Entity> selectedEntities = scene->getSelectedEntities();

	if (!selectedEntities.empty()) {
		auto& transformComponent = selectedEntities[0].getComponent<TransformComponent>();

		glm::mat4& transform = transformComponent.getModelMatrix();

		ImGuizmo::SetOrthographic(false);
		ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
		float wd = (float)ImGui::GetWindowWidth();
		float wh = (float)ImGui::GetWindowHeight();

		ImVec2 windowPos = ImGui::GetWindowPos();
		ImGuizmo::SetRect(windowPos.x, windowPos.y, wd, wh);
		//ImVec2 viewportPos = ImGui::GetMainViewport()->Pos;
		//ImGuizmo::SetRect(
		//	windowPos.x - viewportPos.x,
		//	windowPos.y - viewportPos.y,
		//	ImGui::GetWindowWidth(),
		//	ImGui::GetWindowHeight()
		//);

		glm::mat4 identity(1.0f);

		if (drawGrid) {
			ImGuizmo::DrawGrid(v, p, glm::value_ptr(identity), 100.f);
		}

		bool res = ImGuizmo::Manipulate(
			v, 
			p, 
			(ImGuizmo::OPERATION)GuizmoType, 
			ImGuizmo::LOCAL, 
			glm::value_ptr(transform)
		);
		viewManipulateRight = ImGui::GetWindowPos().x + wd;
		viewManipulateTop = ImGui::GetWindowPos().y;
		ImGuizmo::ViewManipulate(
			v, 
			5.0f, 
			ImVec2(viewManipulateRight - 128, viewManipulateTop), 
			ImVec2(128, 128), 0x10101010
		);

		if (ImGuizmo::IsUsing()) {
			GuizmoActive = true;
			glm::vec3 translation, rotation, scale;
			Utils::Math::DecomposeTransform(transform, translation, rotation, scale);
			glm::vec3 deltaRotation = rotation - transformComponent.rotateVec;

			transformComponent.translateVec = translation;
			transformComponent.rotateVec += deltaRotation;
			transformComponent.scaleVec = scale;
		}
		else {
			GuizmoActive = false;
		}
	}
}

EditorLayer::EditorLayer(const std::string& name) 
	: Layer (name)
{

}

void EditorLayer::init(GuiManager* controller)
{
	guiController = controller;
	guiController->useDarkTheme();
	sceneManager.addScene("default");
	modelShader.Init("Shaders/model.vert", "Shaders/model.frag");
}

void EditorLayer::OnAttach(LayerManager* manager)
{
	Layer::OnAttach(manager);

	if (!SceneManager::cameraController) {
		editorCamera = new Camera();
		editorCamera->init(manager->Window().width, manager->Window().height, glm::vec3(1.0, 0.0, 0.0), glm::vec3(1.0));
		SceneManager::cameraController = editorCamera;
	}
	else {
		editorCamera = SceneManager::cameraController;
	}

	EventManager::getInstance().Subscribe(EventType::ModelLoadEvent, [](Event& event) {
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

	EventManager::getInstance().Subscribe(EventType::AnimationLoadEvent, [](Event& event) {
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


	EventManager::getInstance().Subscribe(EventType::MouseMoved, [&](Event& event) {
		MouseMoveEvent& mouseEvent = static_cast<MouseMoveEvent&>(event);

		if (GuizmoActive && editorActive) {
			mouseEvent.Handled = true;	// block mouse event from other layers
		}
	});

	keyEventID = EventManager::getInstance().Subscribe(EventType::KeyPressed, [&](Event& event) {
		KeyPressedEvent& keyPressedEvent = static_cast<KeyPressedEvent&>(event);
		if (GuizmoActive || editorActive) {
			handleKeyPressed(keyPressedEvent.keyCode);
			keyPressedEvent.Handled = true;	// block keyboard event from other layers
		}
	});


}

void EditorLayer::OnDetach()
{

}

void EditorLayer::OnUpdate()
{
	editorCamera->onUpdate();
	auto framebuffer = LayerManager::getFrameBuffer("DeferredIBLDemo");
	
	Scene& scene = *SceneManager::getInstance().getActiveScene();

	modelShader.Activate();
	modelShader.setInt("diffuse", 0);


	if (framebuffer) {
		framebuffer->Bind();

		for (auto& [id, entity] : scene.entities) {
			if (entity.hasComponent<CameraComponent>()) {

				ModelComponent& modelComponent = entity.getComponent<ModelComponent>();
				TransformComponent& transform = entity.getComponent<TransformComponent>();
				glm::mat4 viewMatrix = SceneManager::cameraController->getViewMatrix();
				const glm::mat4& modelMatrix = transform.getModelMatrix();
				std::shared_ptr<Model> model = modelComponent.model.lock();

				if (model != nullptr) {
					if (SceneManager::cameraController) {
						modelShader.setMat4("mvp", SceneManager::cameraController->getMVP());
					}

					if (faceCamera) {
						modelShader.setMat4(
							"matrix", 
							Utils::ViewTransform::faceCameraBillboard(modelMatrix, viewMatrix)
						);
					}
					else {
						modelShader.setMat4("matrix", modelMatrix);
					}
					modelShader.setBool("flipUV", flipUV);
					model->Draw(modelShader);
				}
				else {
					modelComponent.reset();
				}
			}
		}

		framebuffer->Unbind();
	}
}

void EditorLayer::OnGuiUpdate()
{
	guiController->render();
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
	//LayerManager& layerManager = *Application::getInstance().layerManager;
	if (ImGui::Button("add demo layer")) {
		//id = "demo " + std::to_string(layerManager.size());
		//scene->addLayer(new ParticleDemo(id.c_str()));
		//layerManager.AddLayer(new ParticleDemo(id.c_str()));

	}
	if (ImGui::Button("add bloom layer")) {
		//id = "bloom " + std::to_string(layerManager.size());
		//scene->addLayer(new BloomLayer(id.c_str()));
		//layerManager.AddLayer(new BloomLayer(id.c_str()));

	}
	if (ImGui::Button("remove layer")) {	//TODO: should be able to delete selected layers
		//scene->removeLayer(1);
		//layerManager.RemoveLayer(1);

	}
	ImGui::End();
}

void EditorLayer::OnEvent(Event& event)
{

}

void EditorLayer::handleKeyPressed(int keycode)
{
	if (keycode == KEY_T) {
		GuizmoType = ImGuizmo::OPERATION::TRANSLATE;
	}
	if (keycode == KEY_R) {
		GuizmoType = ImGuizmo::OPERATION::ROTATE;
	}
	if (keycode == KEY_Z) {
		GuizmoType = ImGuizmo::OPERATION::SCALE;
	}
	if (keycode == KEY_DELETE) {
		Scene* scene = SceneManager::getInstance().getActiveScene();
		std::vector<Entity> selectedEntities = scene->getSelectedEntities();
		if (!selectedEntities.empty()) {
			scene->removeEntity(selectedEntities[0].getID());
		}
	}
	if (keycode == KEY_G) {
		Console::println("receiving key event");
		EventManager::getInstance().Unsubscribe(EventType::KeyPressed, keyEventID);
	}
}
