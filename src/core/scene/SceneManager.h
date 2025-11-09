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
#include "Animator.h"	//animation dependency

class Camera;

class SceneManager {
private:
	static std::unordered_map<std::string, std::unique_ptr<Shader>> shaders;
	static std::string selectedID;
	std::mutex animationsLock;
	std::mutex animatorsLock;
	std::mutex modelsLock;
	std::string activeScene;

	SceneManager();

	std::unordered_map<std::string, std::unique_ptr<Scene>> scenes;


public:
	static std::mutex mtx;
	std::unordered_map<std::string, std::shared_ptr<Model>> models;
	std::unordered_map<std::string, std::shared_ptr<Animation>> animations;

	Texture defaultAlbedo;
	Texture defaultNormal;
	Texture defaultMetallic;
	Texture defaultRoughness;
	Texture defaultAO;

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
	std::string addModelFromMeshes(std::vector<Mesh>& meshes);
	bool removeModel(const std::string& path);
	std::string addAnimation(const std::string& path, Model* model);
	bool removeAnimation(const std::string& path);
};

