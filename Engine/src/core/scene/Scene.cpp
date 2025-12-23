#include "Scene.h"
#include "graphics/utils/Utils.h"
#include "core/entities/Entity.h"
#include "core/components/MComponent.h"
#include "core/events/EventManager.h"
#include "core/features/ServiceLocator.h"
#include "Logging/Logger.h"
#include "window/AppWindow.h"
#include "core/features/Stream.h"

Scene::Scene(std::string name) 
	: 	sceneName(name),
		isEnabled(true),
		selectedMesh(0),
		m_logger(ServiceLocator::GetService<Logger>("Engine_LoggerPSD"))
{
	EventManager& eventManager = EventManager::getInstance();
	eventManager.subscribe(EventType::KeyPressed, [&](Event& event) {
		KeyPressedEvent& keyPressedEvent = static_cast<KeyPressedEvent&>(event);
		int keyCode = keyPressedEvent.keyCode;
		
		// two keys ctrl and KEY_S pressed do not happen at the same frame
		if(keyCode == KEY_LEFT_CONTROL || keyCode == KEY_RIGHT_CONTROL) {
			controlPressed = true;
		}

		if(keyCode == KEY_S) {
			if(controlPressed){
				//TODO: resolve write got overwritten on next build artifact
				std::string directory = "../../";
				saveScene(directory + "assets/data/Level1-test.json");
				controlPressed = false;
			}
		}
	});
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

const std::string& Scene::getName() const
{
	return sceneName;
}

bool Scene::saveScene(std::string_view filePath)
{
    std::filesystem::path path(filePath);

    // Ensure the parent directories exist
    std::filesystem::create_directories(path.parent_path());
    processing = true;
    nlohmann::json sceneJson;

    sceneJson["scene_name"] = sceneName;
    
    for(auto& [id, entity] : entities) {
        bool hasParent = false;
        if(entity.hasComponent<RelationshipComponent>()) {
            hasParent = entity.getComponent<RelationshipComponent>().parent != entt::null;
        }

        if (!hasParent) {
            sceneJson["entities"].push_back(m_serializer.saveEntity(entity, registry));
        }
    }

    // Use the filesystem path directly
    std::ofstream out(path, std::ios::out | std::ios::trunc);
    if (!out) {
        m_logger.error("Stream error: Could not open file at {}", filePath);
        processing = false;
        return false;
    }

    try {
        out << sceneJson.dump(2) << std::endl;
        out.flush();
        if (out.fail()) {
            m_logger.error("Failed writing data to disk for {}", filePath);
        }
        out.close();
    } 
    catch (const std::exception& e) {
        m_logger.error("JSON/File error: {}", e.what());
    }

    m_logger.info("Scene saved: {} root entities written", sceneJson["entities"].size());
    processing = false;
    return true;
}


bool Scene::loadScene(std::string_view filePath) 
{
	processing = true;
    nlohmann::json sceneJson = m_serializer.loadJson(filePath.data());

	if(!sceneJson.contains("scene_name")) {
		m_logger.error("missing scene name");
		return false;
	}
	
	sceneName = sceneJson["scene_name"];

	if(sceneJson.contains("entities")){
		for (const auto& entityData : sceneJson["entities"]) {
			m_serializer.loadEntity(entityData, registry, entities, entt::null);
		}

		for (auto& [id, entity] : entities) {
			if (entity.hasComponent<ModelComponent>()) {
				entity.onModelComponentAdded();
			}
			if(entity.hasComponent<MeshComponent>()) {
				entity.onMeshComponentAdded();
			}
		}
    	m_logger.info("scene loaded with {} entities", entities.size());
	}

    processing = false;
    return true;
}
