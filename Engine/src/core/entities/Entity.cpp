#include "Entity.h"
#include "core/scene/SceneManager.h"
#include "core/components/MComponent.h"
#include "core/events/EventManager.h"
#include "core/features/ServiceLocator.h"
#include "core/resources/managers/TextureManager.h"
#include "core/resources/managers/ModelManager.h"
#include "core/resources/managers/MeshManager.h"
#include "core/resources/managers/MaterialManager.h"

Entity::Entity(const entt::entity& entity, entt::registry& registry)
    : entity(entity), registry(&registry)
{
    
}

bool Entity::operator==(const Entity& other) const
{
    return entity == (entt::entity) other.getID() && registry == other.registry;
}

bool Entity::operator!=(const Entity& other) const
{
    return !operator==(other);
}

Entity::operator entt::entity()
{
    return entity;
}

uint32_t Entity::getID() const
{
    return static_cast<uint32_t>(entity);
}

entt::registry *Entity::getRegistry()
{
    return registry;
}

void Entity::onCameraComponentAdded()
{
    if (!this->hasComponent<ModelComponent>()) {
        Scene& scene = *SceneManager::getInstance().getActiveScene();
        auto& modelComponent = this->addComponent<ModelComponent>();
    }
}

void Entity::onModelComponentAdded()
{
    ModelComponent& modelComponent = getComponent<ModelComponent>();
    ModelLoadEvent event(modelComponent.path, *this);
    EventManager::getInstance().publish(event);
}

void Entity::onSpriteComponentAdded()
{
    SpriteComponent& sprite = getComponent<SpriteComponent>();
    auto textureManager = &ServiceLocator::GetService<TextureManager>("TextureManagerVulkan");
    auto modelManager = &ServiceLocator::GetService<ModelManager>("ModelManager");
    auto meshManager = &ServiceLocator::GetService<MeshManager>("MeshManager");
    auto materialManager = &ServiceLocator::GetService<MaterialManager>("MaterialManagerVulkan");
    
    assert(textureManager && modelManager && meshManager && materialManager && "failed to get texture manager");

    if(!hasComponent<ModelComponent>()) {
        addComponent<ModelComponent>("$prim$Quad");
        onModelComponentAdded();
    }
    
    if(!hasComponent<AnimationComponent>()) {
        addComponent<AnimationComponent>();
        onAnimationComponentAdded();
    }

    if(sprite.path != "None") {
        sprite.textureID = textureManager->loadTexture(sprite.path);
    }

    if(sprite.textureID != 0) {
        ModelComponent& modelComponent = getComponent<ModelComponent>();
        Model* model = modelManager->getModel(modelComponent.modelID);
        Mesh* mesh = meshManager->getMesh(model->meshIDs[0]);
        MaterialDesc material = materialManager->getMaterial(mesh->materialID);
        material.albedoIDs = { sprite.textureID };
        materialManager->updateMaterial(mesh->materialID, material);
    }
}

void Entity::onAnimationComponentAdded()
{

}