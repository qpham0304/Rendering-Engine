#include "Scene.h"
#include "imgui.h"
#include "../../graphics/utils/Utils.h"
#include "../entities/Entity.h"
#include "../components/MComponent.h"

Scene::Scene(const std::string name) : sceneName(name), isEnabled(true)
{

}
/*
bool Scene::addLayer(Layer* layer)
{
	return layerManager.AddLayer(layer);
}

bool Scene::removeLayer(int&& index)
{
	return layerManager.RemoveLayer(std::move(index));
}
*/

uint32_t Scene::addEntity(const std::string& name)
{
	entt::entity e = registry.create();
	//std::string uuid = Utils::uuid::get_uuid();
	uint32_t uuid = (uint32_t)e;
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

const std::vector<Entity>& Scene::getSelectedEntities()
{
	return selectedEntities;
}

bool Scene::addShader(const std::string& name, Shader& shader)
{
	if (shaders.find(name) == shaders.end()) {
		shaders[name] = std::make_shared<Shader>(std::move(shader));
		return true;
	}
	return false;
}

bool Scene::addShader(const std::string& name, const std::string& vertPath, const std::string& fragPath)
{
	//if (shaders.find(name) == shaders.end()) {
		shaders[name] = std::make_shared<Shader>(vertPath.c_str(), fragPath.c_str());
		return true;
	//}
	//return false;
}

std::shared_ptr<Shader> Scene::getShader(const std::string& name)
{
	if (shaders.find(name) != shaders.end()) {
		return shaders[name];
	}
	return nullptr;
}

bool Scene::removeShader(const std::string& name)
{
	if (shaders.find(name) != shaders.end()) {
		shaders.erase(name);
		return true;
	}
	return false;
}

void Scene::onUpdate(const float& deltaTime)
{
	entt::basic_view view = registry.view<TransformComponent>();

	view.each([&deltaTime](auto& trans) {

	});

	/*
	for (const auto& layer : layerManager) {
		if (!layer->m_Enabled) {
			continue;
		}
		layer->OnUpdate();
	}
	*/
}

void Scene::onGuiUpdate(const float& deltaTime)
{
	/*
	for (auto& layer : layerManager) {
		if (!layer->m_Enabled) {
			continue;
		}
		layer->OnGuiUpdate();
	}
	for (auto& layer : layerManager) {
		if (ImGui::Begin("Layers")) {
			ImGui::Checkbox(layer->GetName().c_str(), &layer->m_Enabled);
			ImGui::End();
		}
	}
	*/
}

const std::string& Scene::getName() const
{
	return sceneName;
}
