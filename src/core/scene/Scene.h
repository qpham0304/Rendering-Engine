#pragma once

#include<glm/glm.hpp>
#include "entt.hpp"
#include "../entities/Entity.h"
#include "../layers/LayerManager.h"
#include "Shader.h"

class Scene
{
private:
	std::string sceneName;
	entt::registry registry;
	std::vector<Entity> selectedEntities;
	std::unordered_map <std::string, std::shared_ptr<Shader>> shaders;

public:
	std::unordered_map<uint32_t, Entity> entities;
	//LayerManager layerManager;

	bool isEnabled;

	Scene(const std::string name);
	~Scene() = default;


	//bool addLayer(Layer* layer);
	//bool removeLayer(int&& index);


	uint32_t addEntity(const std::string& name = "Entity");
	bool removeEntity(const uint32_t& uuid);
	bool hasEntity(const uint32_t& id);
	Entity getEntity(const uint32_t& id);
	void selectEntities(std::vector<Entity> entities);

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
			entitiesList.push_back(entities[(uint32_t)entity]);
		}

		return entitiesList;
	}

	const std::vector<Entity>& getSelectedEntities();
	bool addShader(const std::string& name, Shader& shader);
	bool addShader(const std::string& name, const std::string& vertPath, const std::string& fragPath);
	std::shared_ptr<Shader> getShader(const std::string& name);
	bool removeShader(const std::string& name);

	void onStart();
	void onStop();
	void onUpdate(const float& deltaTime);
	void onGuiUpdate(const float& deltaTime);
	const std::string& getName() const;
};

