#include "SceneManager.h"
#include "graphics/utils/Utils.h"
#include "animation/Animation.h"
#include "core/features/Camera.h"
#include "graphics/framework/OpenGL/core/ModelOpenGL.h"
#include "window/AppWindow.h"

std::unordered_map<std::string, std::unique_ptr<ShaderOpenGL>> SceneManager::shaders = {};
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

void SceneManager::onGuiUpdate()
{
	for (auto& [name, scene] : scenes) {
		scene->onGuiUpdate(AppWindow::getTime());
	}
}

std::string SceneManager::addModel(const std::string& path)
{
	try {
		std::string uuid = Utils::uuid::get_uuid();
		std::scoped_lock<std::mutex> lock(modelsLock);
		if (models.find(uuid) == models.end()) {
			models[uuid] = std::make_shared<ModelOpenGL>(path.c_str());
		}
		//TODO: might want to manual increase reference counter for instanced drawing
		return uuid;
	}
	catch (std::runtime_error) {
		return "";
	}
}

std::string SceneManager::addModelFromMeshes(std::vector<MeshOpenGL>& meshes)
{
	try {
		std::string uuid = Utils::uuid::get_uuid();
		std::scoped_lock<std::mutex> lock(modelsLock);
		if (models.find(uuid) == models.end()) {
			models[uuid] = std::make_shared<ModelOpenGL>(meshes, uuid);
		}
		//TODO: might want to manual increase reference counter for instanced drawing
		return uuid;
	}
	catch (std::runtime_error) {
		return "";
	}
}

bool SceneManager::removeModel(const std::string& path)
{
	std::scoped_lock<std::mutex> lock(modelsLock);
	if (models.find(path) != models.end()) {
		models.erase(path);
		return true;
	}
	return false;
}

std::string SceneManager::addAnimation(const std::string& path, ModelOpenGL* model)
{
	std::string uuid = Utils::uuid::get_uuid();
	std::scoped_lock<std::mutex> lock(animationsLock);
	if (animations.find(uuid) == animations.end()) {
		animations[uuid] = std::make_shared<Animation>(path, model);
		return uuid;
	}
	else {
		throw::std::runtime_error ("failed to load model");
		return "";
	}
}

bool SceneManager::removeAnimation(const std::string& path)
{
	std::scoped_lock<std::mutex> lock(animationsLock);
	if (animations.find(path) != animations.end()) {
		animations.erase(path);
		return true;
	}
	return false;
}