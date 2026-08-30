#include "Entity.h"
#include "core/scene/SceneManager.h"
#include "core/components/MComponent.h"
#include "core/events/EventManager.h"
#include "core/features/ServiceLocator.h"
#include "core/resources/managers/TextureManager.h"
#include "core/resources/managers/ModelManager.h"
#include "core/resources/managers/MeshManager.h"
#include "core/resources/managers/MaterialManager.h"
#include "physics/PhysicsManager.h"
#include "scripting/ScriptManager.h"
#include "window/AppWindow.h"

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
    TransformComponent& transformComponent = getComponent<TransformComponent>();
    glm::mat4 view = transformComponent.getModelMatrix();

    float width = static_cast<float>(AppWindow::getWidth());
    float height = static_cast<float>(AppWindow::getHeight());
    float aspectRatio = width / height;

    glm::mat4 projection = glm::ortho(
        -aspectRatio,		// Left
        aspectRatio,		// Right
        -1.0f,				// Bottom
        1.0f,				// Top
        -1.0f,				// Near
        1.0f				// Far
    );

    CameraComponent& cameraComponent = getComponent<CameraComponent>();
    cameraComponent.viewWidth = width;
    cameraComponent.viewHeight = height;
    cameraComponent.projection = projection;
    cameraComponent.view = view;
    cameraComponent.orientation = -transformComponent.translateVec;

    CameraUpdateEvent cameraUpdateEvent(*this);
    EventManager::getInstance().publish(cameraUpdateEvent);
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

void Entity::onAnimationStateComponentAdded()
{
    // if (hasComponent<NameComponent>()) {
    //     printf("animation state name: %s\n", getComponent<NameComponent>().name.c_str());
    // } else {
    //     printf("animation state name: [NameComponent not parsed yet]\n");
    // }
}

void Entity::onScriptComponentAdded()
{
    ScriptComponent& scriptComponent = getComponent<ScriptComponent>();
    auto scriptManager = &ServiceLocator::GetService<ScriptManager>("ScriptManager");
    scriptManager->loadScript(*this, scriptComponent.path);
}

void Entity::onColliderComponentAdded()
{
    auto modelManager = &ServiceLocator::GetService<ModelManager>("ModelManager");
    auto meshManager = &ServiceLocator::GetService<MeshManager>("MeshManager");
    auto physicsManager = &ServiceLocator::GetService<PhysicsManager>("PhysicsManager");
    
    if(!hasComponent<ColliderComponent>()){
        printf("critical collider not found");
        return;
    }

    if(!hasComponent<ModelComponent>()){
        printf("critical ModelComponent not found");
        return;
    }

    TransformComponent& transformComponent = getComponent<TransformComponent>();
    ColliderComponent& colliderComponent = getComponent<ColliderComponent>();
    
    ModelComponent& modelComponent = getComponent<ModelComponent>();
    Model* model = modelManager->getModel(modelComponent.modelID);
    Mesh* mesh = meshManager->getMesh(model->meshIDs[0]);

    auto shapeID = physicsManager->createBody(*mesh, transformComponent.translateVec, transformComponent.scaleVec, colliderComponent.isStatic);
    colliderComponent.shapeID = shapeID;
}
