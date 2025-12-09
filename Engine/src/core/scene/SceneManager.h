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
#include "src/animation/Animator.h"	//animation dependency
#include "src/graphics/framework/OpenGL/core/TextureOpenGL.h"

class Camera;

class SceneManager {
private:
	static std::unordered_map<std::string, std::unique_ptr<ShaderOpenGL>> shaders;
	static std::string selectedID;
	std::mutex animationsLock;
	std::mutex animatorsLock;
	std::mutex modelsLock;
	std::string activeScene;

	SceneManager();

	std::unordered_map<std::string, std::unique_ptr<Scene>> scenes;


public:
	static std::mutex mtx;
	std::unordered_map<std::string, std::shared_ptr<ModelOpenGL>> models;
	std::unordered_map<std::string, std::shared_ptr<Animation>> animations;

	TextureOpenGL defaultAlbedo;
	TextureOpenGL defaultNormal;
	TextureOpenGL defaultMetallic;
	TextureOpenGL defaultRoughness;
	TextureOpenGL defaultAO;

	~SceneManager();

	static SceneManager& getInstance();

	static Camera* cameraController;

	bool addScene(const std::string& name);
	bool addScene(std::unique_ptr<Scene> scene);
	Scene* getScene(const std::string& name);
	Scene* getActiveScene();
	bool removeScene(const std::string& name);
	void onUpdate(const float deltaTime);
	void onGuiUpdate(const float deltaTime);
	std::string addModel(const std::string& path);
	std::string addModelFromMeshes(std::vector<MeshOpenGL>& meshes);
	bool removeModel(const std::string& path);
	std::string addAnimation(const std::string& path, ModelOpenGL* model);
	bool removeAnimation(const std::string& path);
};

