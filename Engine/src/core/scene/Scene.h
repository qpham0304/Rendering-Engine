#pragma once

#include <glm/glm.hpp>
#include "entt.hpp"
#include "graphics/framework/OpenGL/core/ShaderOpenGL.h"
#include "core/entities/Entity.h"

class Scene
{
public:
	bool isEnabled;
	std::unordered_map<uint32_t, Entity> entities;

public:
	Scene(const std::string name);
	~Scene() = default;

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
	bool addShader(const std::string& name, ShaderOpenGL& shader);
	bool addShader(const std::string& name, const std::string& vertPath, const std::string& fragPath);
	std::shared_ptr<ShaderOpenGL> getShader(const std::string& name);
	bool removeShader(const std::string& name);

	void onStart();
	void onStop();
	void onUpdate(const float& deltaTime);
	void onGuiUpdate(const float& deltaTime);
	const std::string& getName() const;

private:
	std::string sceneName;
	entt::registry registry;
	std::vector<Entity> selectedEntities;
	std::unordered_map <std::string, std::shared_ptr<ShaderOpenGL>> shaders;
};

