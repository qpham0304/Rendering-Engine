#include "Entity.h"
#include "../scene/SceneManager.h"
#include "../components/MComponent.h"
#include "../events/EventManager.h"

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

uint32_t Entity::getID() const
{
    return static_cast<uint32_t>(entity);
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
