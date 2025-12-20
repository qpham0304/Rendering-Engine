#include "SceneManager.h"
#include "graphics/utils/Utils.h"
#include "animation/Animation.h"
#include "core/features/Camera.h"
#include "graphics/framework/OpenGL/core/ModelOpenGL.h"
#include "window/AppWindow.h"
#include "core/events/EventManager.h"

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
	EventManager& eventManager = EventManager::getInstance();
	eventManager.subscribe(EventType::WindowResize, [this](Event& event) {
		WindowResizeEvent& windowResizeEvent = static_cast<WindowResizeEvent&>(event);
		cameraController->updateViewResize(windowResizeEvent.m_width, windowResizeEvent.m_height);
	});

	eventManager.subscribe(EventType::MouseScrolled, [this](Event& event) {
		MouseScrollEvent& mouseEvent = static_cast<MouseScrollEvent&>(event);
		cameraController->scroll_callback(mouseEvent.m_x, mouseEvent.m_y);
	});

	eventManager.subscribe(EventType::MouseMoved, [this](Event& event) {
		MouseMoveEvent& mouseEvent = static_cast<MouseMoveEvent&>(event);
		cameraController->processInput();
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

	if(cameraController){
		cameraController->onUpdate();
		cameraController->processInput();
	}
}

SceneManager& SceneManager::getInstance()
{
	static SceneManager instance;
	return instance;
}

bool SceneManager::addScene(const std::string& name)
{
	if (scenes.find(name) == scenes.end()) {
		scenes[name].reset(new Scene(name));
		scenes[name]->id = _assignID();
		activeScene = name;
		return true;
	}
	else {
		return false;
	}
}

bool SceneManager::addScene(std::unique_ptr<Scene> scene)
{
	if (scenes.find(scene->getName()) == scenes.end()) {
		scenes[scene->getName()] = std::move(scene);
		scenes[scene->getName()]->id = _assignID();
		return true;
	}
	else {
		return false;
	}
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

void SceneManager::setActiveScene(const std::string& name)
{
	if (scenes.find(activeScene) != scenes.end()) {
		activeScene = name;
	}
	return;
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

void SceneManager::onGuiUpdate()
{
	for (auto& [name, scene] : scenes) {
		scene->onGuiUpdate(AppWindow::getTime());
	}
}

std::vector<uint32_t> SceneManager::listIDs() const
{
	std::vector<uint32_t> list;
	for (const auto& [name, scene] : scenes) {
		list.emplace_back(scene->id);
	}
	return list;
}
