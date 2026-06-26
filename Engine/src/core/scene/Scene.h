#pragma once

#include <iomanip>
#include <fstream>
#include "core/entities/Entity.h"
#include "Serializer.h"

class SceneManager;
class Logger;
class Scene
{
public:
	bool isEnabled;

public:
	Scene(std::string name);
	~Scene() = default;

    Scene(const Scene& other) = delete;
    Scene& operator=(const Scene& other) = delete;
    Scene(Scene&& other) = delete;
    Scene& operator=(Scene&& other) = delete;

	uint32_t addEntity(const std::string& name = "Entity");
	uint32_t duplicateEntity(Entity source);
	bool removeEntity(const uint32_t& uuid);
	bool hasEntity(const uint32_t& id);
	Entity getEntity(const uint32_t& id);
	void selectEntities(std::vector<Entity> entities);
	void selectMesh(const uint32_t& meshID);

	template<typename... Components>
	bool hasComponent(entt::entity entity) {
		return registry.all_of<Components...>(entity);
	}

	// return true entity from entt, in case we want more performance
	template<typename... Components>
	auto getEnttEntities() {
		return registry.view<Components...>();
	}

	// wrapper for entity object, slower due to copies but more consistent API
	template<typename... Components>
	std::vector<Entity> getEntitiesWith() {
		auto view = getEnttEntities<Components...>();
		std::vector<Entity> entitiesList = {};
		for (auto& entity : view) {
			entitiesList.emplace_back(entity, registry);
		}

		return entitiesList;
	}

	const std::vector<Entity>& getSelectedEntities();
	const uint32_t getSelectedMeshID() const;
	
	void onStart();
	void onStop();
	void onUpdate(const float& deltaTime);
	const std::string& getName() const;
	bool saveScene(std::string_view path);
	bool loadScene(std::string_view path);
	bool unloadScene();
	bool reloadScene();


private:
	friend class SceneManager;

	uint32_t id;
	std::string sceneName;
	std::string scenePath;
	entt::registry registry;
	std::vector<Entity> selectedEntities;
	uint32_t selectedMesh;
	std::unordered_map<uint32_t, Entity> entities;
	std::vector<Entity> frameNewEntities;
	std::vector<uint32_t> frameDeletedEntities;
	Logger& m_logger;
	Serializer m_serializer;

	bool controlPressed = false;
	bool shiftPressed = false;
	bool processing = false;
	bool reloading = false;

	void _addEntity(Entity& entity);
	bool _removeEntity(const uint32_t& uuid);
};

