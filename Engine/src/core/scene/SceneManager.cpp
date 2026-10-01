#include "SceneManager.h"
#include "graphics/utils/Utils.h"
#include "graphics/framework/OpenGL/core/ModelOpenGL.h"
#include "core/features/Camera.h"
#include "window/AppWindow.h"
#include "core/events/EventManager.h"
#include "core/features/ServiceLocator.h"
#include "gui/GuiManager.h"

Camera* SceneManager::cameraController = nullptr;
std::string SceneManager::selectedID = "";

SceneManager::SceneManager() 
	: Manager("SceneManager")
{
	
}

SceneManager::~SceneManager()
{

}

bool SceneManager::init(WindowConfig config)
{
	Service::init(config);
	
	EventManager& eventManager = EventManager::getInstance();
	eventManager.subscribe(EventType::WindowResize, [this](Event& event) {
		WindowResizeEvent& windowResizeEvent = static_cast<WindowResizeEvent&>(event);
		if(!cameraController) {
			return;
		}

		cameraController->updateViewResize(windowResizeEvent.m_width, windowResizeEvent.m_height);
	});

	eventManager.subscribe(EventType::MouseScrolled, [this](Event& event) {
		MouseScrollEvent& mouseEvent = static_cast<MouseScrollEvent&>(event);
		if(!cameraController) {
			return;
		}

		if(areaFocused) {
			cameraController->scroll_callback(mouseEvent.m_x, mouseEvent.m_y);
		}
	});

	eventManager.subscribe(EventType::MouseMoved, [this](Event& event) {
		MouseMoveEvent& mouseEvent = static_cast<MouseMoveEvent&>(event);
		if(!cameraController) {
			return;
		}

		GuiManager* guiManager = &ServiceLocator::GetService<GuiManager>("ImGuiManager");
		if(areaFocused) {
			cameraController->processMouse();
		}
	});

	eventManager.subscribe(EventType::GuiFocusedEvent, [this](Event& event) {
		GuiFocusEvent& focusEvent = static_cast<GuiFocusEvent&>(event);
		areaFocused = focusEvent.isFocused;
	});

	return true;
}

bool SceneManager::onClose()
{
	return true;
}

void SceneManager::onUpdate()
{
	for (auto& [name, scene] : scenes) {
		if (!scene->isEnabled) {
			continue;
		}
		scene->onUpdate(AppWindow::getTime());
	}

	if(!cameraController){
		return;
	}

	cameraController->onUpdate();

	if(areaFocused) {
		// cameraController->processMouse();
		cameraController->processKeyboard();
	}
}

SceneManager& SceneManager::getInstance()
{
	static SceneManager instance;
	return instance;
}

Scene* SceneManager::addScene(const std::string& name)
{
	if (scenes.find(name) == scenes.end()) {
		scenes[name] = std::make_unique<Scene>(name);
		scenes[name]->id = _assignID();
		activeScene = name;
	}
	return getScene(name);
}

Scene* SceneManager::getScene(const std::string& name)
{
	if (scenes.find(name) != scenes.end()) {
		return scenes[name].get();
	}
	return nullptr;
}

Scene* SceneManager::getScene(const uint32_t& id)
{
	for(auto& [name, scene] : scenes) {
		if(scene->id == id){
			return scene.get();
		}
	}
	return nullptr;
}


Scene* SceneManager::getActiveScene()
{
	if (scenes.find(activeScene) != scenes.end()) {
		return scenes[activeScene].get();
	}
	return nullptr;
}


//TODO: properly unlock non active scene or there'll be duplicate data like collider
bool SceneManager::setActiveScene(const std::string& name)
{
	if (scenes.find(name) != scenes.end()) {
		// getActiveScene()->unloadScene();
		activeScene = name;
		// getActiveScene()->reloadScene();
		return true;
	}
	return false;
}

bool SceneManager::setSceneName(Scene* scene, const std::string& newName)
{
	if (scenes.find(newName) != scenes.end()) {
		return false;
	}

	auto nodeHandler = scenes.extract(scene->getName());
	if(nodeHandler.empty()) {
		return false;
	}
	nodeHandler.key() = newName;
	scenes.insert(std::move(nodeHandler));

	return true;
}


bool SceneManager::removeScene(const std::string& name)
{
	if (scenes.find(name) != scenes.end()) {
		scenes[name].reset();
		scenes.erase(name);
		return true;
	}
	else {
		return false;
	}
}

bool SceneManager::empty()
{
    return scenes.empty();
}

std::vector<uint32_t> SceneManager::listIDs() const
{
	std::vector<uint32_t> list;
	for (const auto& [name, scene] : scenes) {
		list.emplace_back(scene->id);
	}
	return list;
}
