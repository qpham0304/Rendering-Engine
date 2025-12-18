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
#include "animation/Animator.h"	//animation dependency
#include "graphics/framework/OpenGL/core/TextureOpenGL.h"
#include "core/resources/managers/Manager.h"

class Camera;

class SceneManager : public Manager
{
public:
	std::unordered_map<std::string, std::shared_ptr<ModelOpenGL>> models;
	std::unordered_map<std::string, std::shared_ptr<Animation>> animations;

	static Camera* cameraController;


public:
	~SceneManager();

	static SceneManager& getInstance();

	bool init(WindowConfig config) override;
	bool onClose() override;
	void onUpdate() override;

	bool addScene(const std::string& name);
	bool addScene(std::unique_ptr<Scene> scene);
	Scene* getScene(const std::string& name);
	Scene* getActiveScene();
	void setActiveScene(const std::string& name);
	bool removeScene(const std::string& name);
	void onGuiUpdate();
	std::string addModel(const std::string& path);
	std::string addModelFromMeshes(std::vector<MeshOpenGL>& meshes);
	bool removeModel(const std::string& path);
	std::string addAnimation(const std::string& path, ModelOpenGL* model);
	bool removeAnimation(const std::string& path);

private:
	SceneManager();


	virtual std::vector<uint32_t> listIDs() const { return {}; };
	virtual void destroy(uint32_t id) override {};

private:
	static std::unordered_map<std::string, std::unique_ptr<ShaderOpenGL>> shaders;
	static std::string selectedID;
	std::mutex animationsLock;
	std::mutex animatorsLock;
	std::mutex modelsLock;
	std::string activeScene;


	std::unordered_map<std::string, std::unique_ptr<Scene>> scenes;

};

