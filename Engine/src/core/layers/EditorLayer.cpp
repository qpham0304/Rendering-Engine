#include "EditorLayer.h"
#include "window/appwindow.h"
#include "window/Input.h"
#include "gui/GuiManager.h"
#include "core/features/camera.h"
#include "core/features/OrbitCamera.h"
#include "core/features/EngineStates.h"
#include "core/events/EventManager.h"
#include "core/layers/layerManager.h"
#include "core/components/MComponent.h"

#include "gui/framework/ImGui/widgets/ImGuiConsoleLogWidget.h"
#include "gui/framework/ImGui/widgets/ImGuiLeftSidebarWidget.h"
#include "gui/framework/ImGui/widgets/ImGuiRightSidebarWidget.h"
#include "gui/framework/ImGui/widgets/ImGuiMenuWidget.h"
#include "gui/framework/ImGui/widgets/ImGuiMathWidget.h"
#include "gui/framework/ImGui/widgets/ImGuiResourcesInspectorWidget.h"
#include "gui/framework/ImGui/widgets/ImGuiAssetBrowserWidget.h"
#include "gui/framework/ImGui/widgets/ImGuiParticleEditorWidget.h"
#include "gui/framework/ImGui/widgets/ImGuiGraphicsSettingsWidget.h"

void EditorLayer::renderGuizmo()
{
	Scene* scene = SceneManager::getInstance().getActiveScene();
	if(!scene) {
		Log().warn("no active scene found");
		return;
	}
	std::vector<Entity> selectedEntities = scene->getSelectedEntities();

	if (!selectedEntities.empty()) {
		Entity entity = selectedEntities[0];
		auto& transformComponent = entity.getComponent<TransformComponent>();
		glm::mat4 parentWorld = entity.hasParent() ? entity.getParent().getComponent<TransformComponent>().getModelMatrix() : glm::mat4(1.0f);
		guiController.renderGuizmo(transformComponent, parentWorld);
	}
}

EditorLayer::EditorLayer(const std::string& name, GuiManager& controller)
	: Layer (name), guiController(controller)
{
	setFootprint("EditorLayer", sizeof(EditorLayer), MemorySystem::Core);

}

EditorLayer::~EditorLayer()
{
	
}

bool EditorLayer::init()
{
	guiController.useDarkTheme();
	// guiController.useLightTheme();
	
	guiController.addWidget<ImGuiLeftSidebarWidget>();
	guiController.addWidget<ImGuiRightSidebarWidget>();
	guiController.addWidget<ImGuiResourceInspectorWidget>();
	guiController.addWidget<ImGuiConsoleLogWidget>();
	guiController.addWidget<ImGuiAssetBrowserWidget>();
	guiController.addWidget<ImGuiParticleEditorWidget>();
	guiController.addWidget<ImGuiGraphicsSettingsWidget>();
	guiController.addWidget<ImGuiMenuWidget>(guiController.getWidgets());

	
	m_eventSubscriptions.push_back(eventManager.subscribe(EventType::MouseMoved, [&](Event& event) {
		MouseMoveEvent& mouseEvent = static_cast<MouseMoveEvent&>(event);

		if (guiController.isGuizmoFocus()) {
			mouseEvent.Handled = true;	// block mouse event from other layers
		}
	}));

	m_eventSubscriptions.push_back(eventManager.subscribe(EventType::MouseScrolled, [&](Event& event) {
		MouseScrollEvent& mouseEvent = static_cast<MouseScrollEvent&>(event);
		if (guiController.isGuizmoFocus()) {
			mouseEvent.Handled = true;	// block mouse event from other layers
		}
	}));

	keyEventSubscription = eventManager.subscribe(EventType::KeyPressed, [&](Event& event) {
		KeyPressedEvent& keyPressedEvent = static_cast<KeyPressedEvent&>(event);
		if (guiController.isGuizmoFocus() || guiController.isEditorFocus()) {
			handleKeyPressed(keyPressedEvent.keyCode);
			keyPressedEvent.Handled = true;	// block keyboard event from other layers
		}
	});

	if(SceneManager::getInstance().listIDs().empty()) {
		SceneManager::getInstance().addScene("default scene");
		Scene* scene = SceneManager::getInstance().getActiveScene();
		if(scene) {
			// scene->loadScene("assets/data/default-scene.json");
		}
	}

	if(!SceneManager::cameraController) {
		editorCamera = std::make_unique<OrbitCamera>();
		editorCamera->init(
			AppWindow::getWidth(),
			AppWindow::getHeight(),
			glm::vec3(3),
			glm::vec3(0)
		);

		SceneManager::cameraController = editorCamera.get();
	}

	return true;
}

void EditorLayer::onAttach(LayerManager* manager)
{
	Layer::onAttach(manager);
}

void EditorLayer::onDetach()
{
}

void EditorLayer::onUpdate(const FrameTime& frameTime)
{
    if(EngineState::isStopped()) {
        SceneManager::cameraController = editorCamera.get();
    }
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
		case KEY_G:	keyEventSubscription.reset(); break;
	}
}
