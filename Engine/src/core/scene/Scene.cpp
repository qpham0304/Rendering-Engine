#include "Scene.h"
#include "graphics/utils/Utils.h"
#include "core/entities/Entity.h"
#include "core/components/MComponent.h"
#include "core/events/EventManager.h"
#include "core/features/ServiceLocator.h"
#include "Logging/Logger.h"
#include "window/AppWindow.h"

Scene::Scene(std::string name) 
	: 	sceneName(name),
		isEnabled(true),
		selectedMesh(0),
		id(0),
		m_logger(ServiceLocator::GetService<Logger>("Engine_LoggerSPD"))
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
				std::string directory = "../../";
				saveScene(directory + "assets/data/Level1-test.json");
				controlPressed = false;
			}
		}

		if(keyCode == KEY_D) {
			if(controlPressed){
				duplicateEntity(selectedEntities[0]);
				controlPressed = false;
			}
		}
	});

	// entities.reserve(1000);
}

uint32_t Scene::addEntity(const std::string& name)
{
	entt::entity e = registry.create();
    uint32_t id = entt::to_integral(e);
    
    Entity entity(e, registry);
    entity.addComponent<TransformComponent>();
    entity.addComponent<NameComponent>(name);

    frameNewEntities.push_back(entity); 
	selectEntities({ entity }); 

    return id;
}

template<typename T>
void copyComponentIfExists(Entity source, Entity dest) {
    if (source.hasComponent<T>()) {
        T componentData = source.getComponent<T>();
        if constexpr (std::is_same_v<T, TransformComponent>) {
			glm::vec3 translation = componentData.translateVec;
			translation.x += 0.25;
			translation.y += 0.0;
			translation.z += 0.0;
			componentData.translate(translation);
        }
        dest.addComponent<T>(componentData); 
    }
}

uint32_t Scene::duplicateEntity(Entity source)
{
    Entity destination(registry.create(), registry);

    // ideally want to use reflection to get all the components
    copyComponentIfExists<TransformComponent>(source, destination);
    copyComponentIfExists<NameComponent>(source, destination);
    copyComponentIfExists<MeshComponent>(source, destination);
    copyComponentIfExists<ModelComponent>(source, destination);
    copyComponentIfExists<LightComponent>(source, destination);

    // if (destination.hasComponent<MeshComponent>()) {
	// 	destination.onMeshComponentAdded();
	// }
    
    return destination.getID();
}


bool Scene::removeEntity(const uint32_t& uuid)
{
	frameDeletedEntities.push_back(uuid);
	return true;
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

	for (auto& entity : frameNewEntities) {
        if (entt::to_integral((entt::entity)entity) == uuid) {
            return entity;
        }
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
	for(auto& uuid : frameDeletedEntities) {
        _removeEntity(uuid);
    }
    frameDeletedEntities.clear();

	for(auto& entity : frameNewEntities) {
        uint32_t uuid = entity.getID();
        entities[uuid] = entity; 
    }
    frameNewEntities.clear();

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

bool Scene::unloadScene() 
{
	
	return true;
}

void Scene::_addEntity(Entity& entity)
{
	uint32_t uuid = entt::to_integral(static_cast<entt::entity>(entity));
	entities[uuid] = std::move(entity);
}

bool Scene::_removeEntity(const uint32_t &uuid)
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
