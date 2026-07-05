#pragma once

#include <string>
#include <vector>
#include <stack>
#include <unordered_map>
#include <algorithm> 
#include <memory>
#include <future>
#include <mutex>
#include <thread>
#include "Scene.h"
#include "graphics/framework/OpenGL/core/TextureOpenGL.h"
#include "core/resources/managers/Manager.h"
#include "core/features/Mesh.h"

class Camera;

class SceneManager : public Manager
{
public:
	static Camera* cameraController;


public:
	~SceneManager();

	static SceneManager& getInstance();
	
	Scene* addScene(const std::string& name);
	Scene* getScene(const std::string& name);
	Scene* getScene(const uint32_t& id);
	Scene* getActiveScene();

	virtual std::vector<uint32_t> listIDs() const override;
	
	bool setActiveScene(const std::string& name);
	bool setSceneName(Scene* scene, const std::string& newName);
	bool removeScene(const std::string& name);
	bool empty();


private:
	SceneManager();

	bool init(WindowConfig config) override;
	bool onClose() override;
	void onUpdate() override;
	virtual void destroy(uint32_t id) override {};

private:
	std::unordered_map<std::string, std::unique_ptr<Scene>> scenes;
	bool areaFocused { false };

	static std::string selectedID;
	std::mutex animationsLock;
	std::mutex animatorsLock;
	std::mutex modelsLock;
	std::string activeScene;

};

