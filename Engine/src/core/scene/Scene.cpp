#include "Scene.h"
#include "../../graphics/utils/Utils.h"
#include "../entities/Entity.h"
#include "../components/MComponent.h"

Scene::Scene(std::string name) : sceneName(name), isEnabled(true), selectedMesh(0)
{

}

uint32_t Scene::addEntity(const std::string& name)
{
	this->getName();
	entt::entity e = registry.create();
	//std::string uuid = Utils::uuid::get_uuid();
	uint32_t uuid = entt::to_integral(e);
	if (entities.find(uuid) != entities.end()) {
		return -1;	//MAX_VALUE of uint32_t
	}
	entities[uuid] = Entity(e, registry);
	entities[uuid].addComponent<TransformComponent>();
	entities[uuid].addComponent<NameComponent>(name);

	return uuid;
}

bool Scene::removeEntity(const uint32_t& uuid)
{
	if (entities.find(uuid) != entities.end()) {
		int index = 0;
		for (auto& entity : selectedEntities) {
			if (uuid == entity.getID()) {
				selectedEntities.erase(selectedEntities.begin() + index);
			}
			index++;
		}
		
		registry.destroy(static_cast<entt::entity>(uuid));
		entities.erase(uuid);
		return true;
	}
	return false;
}

bool Scene::hasEntity(const uint32_t& uuid)
{
	return (entities.find(uuid) != entities.end());
}

Entity Scene::getEntity(const uint32_t& uuid)
{
	if (entities.find(uuid) != entities.end()) {
		return entities[uuid];
	}
	throw std::runtime_error("Entity does not exist");
}

void Scene::selectEntities(std::vector<Entity> entities)
{
	selectedEntities = entities;
}

void Scene::selectMesh(const uint32_t &meshID)
{
	selectedMesh = meshID;
}

const std::vector<Entity>& Scene::getSelectedEntities()
{
    return selectedEntities;
}

const uint32_t Scene::getSelectedMeshID() const
{
    return selectedMesh;
}


void Scene::onUpdate(const float& deltaTime)
{
	entt::basic_view view = registry.view<TransformComponent>();

	view.each([&deltaTime](auto& trans) {

	});
}

void Scene::onGuiUpdate(const float& deltaTime)
{
}

const std::string& Scene::getName() const
{
	return sceneName;
}
