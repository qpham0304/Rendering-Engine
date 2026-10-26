#include "Entity.h"
#include "core/scene/SceneManager.h"
#include "core/components/MComponent.h"
#include "core/events/EventManager.h"
#include "core/features/ServiceLocator.h"
#include "core/resources/managers/TextureManager.h"
#include "core/resources/managers/SkeletonManager.h"
#include "core/resources/managers/ModelManager.h"
#include "core/resources/managers/MeshManager.h"
#include "core/resources/managers/MaterialManager.h"
#include "physics/PhysicsManager.h"
#include "scripting/ScriptManager.h"
#include "window/AppWindow.h"
#include "particle/ParticleContainer.h"
#include "animation/AnimationManager.h"

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

Entity::operator uint32_t()
{
    return static_cast<uint32_t>(entity);
}

uint32_t Entity::getID() const
{
    return static_cast<uint32_t>(entity);
}

entt::registry *Entity::getRegistry()
{
    return registry;
}

bool Entity::hasParent() const
{
    return hasComponent<RelationshipComponent>() && getComponent<RelationshipComponent>().parent != entt::null;
}

Entity Entity::getParent() const
{
    return Entity(getComponent<RelationshipComponent>().parent, *registry);
}

Entity::HookFunction Entity::_onCameraComponentAdded()
{
    HookFunction function = [](Entity entity)
    {
        TransformComponent& transformComponent = entity.getComponent<TransformComponent>();
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

        CameraComponent& cameraComponent = entity.getComponent<CameraComponent>();
        cameraComponent.viewWidth = width;
        cameraComponent.viewHeight = height;
        cameraComponent.projection = projection;
        cameraComponent.view = view;
        cameraComponent.orientation = -transformComponent.translateVec;

        CameraUpdateEvent cameraUpdateEvent(entity);
        EventManager::getInstance().publish(cameraUpdateEvent);
    };
    return function;
}

Entity::HookFunction Entity::_onParticleEmitterAdded()
{
    HookFunction function = [](Entity entity)
    {
        auto particleManager = &ServiceLocator::GetService<ParticleManager>("ParticleManager");

        ParticleEmitter& emitter = entity.getComponent<ParticleEmitter>();
    };
    return function;
}

Entity::HookFunction Entity::_onSkinningComponentAdded()
{
    HookFunction function = [](Entity entity)
    {
        auto animationManager = &ServiceLocator::GetService<AnimationManager>("AnimationManager");
        auto skeletonManager = &ServiceLocator::GetService<SkeletonManager>("SkeletonManager");
        auto modelManager = &ServiceLocator::GetService<ModelManager>("ModelManager");

        SkinningComponent& skinningComponent = entity.getComponent<SkinningComponent>();

        if (!entity.hasComponent<ModelComponent>()) {
            return;
        }
        ModelComponent& modelComponent = entity.getComponent<ModelComponent>();
        Model* model = modelManager->getModel(modelComponent.modelID);
        if (!model) {
            return;
        }

        auto skeleton = skeletonManager->getSkeleton(model->skeletonID);
        if (!skeleton) {
            return;
        }
        skinningComponent.boneMatricesID = animationManager->addBoneMatrices(skeleton->nodes.size());
        skinningComponent.animatorID = animationManager->addAnimator(model->skeletonID);
        skinningComponent.animationPath = model->path;  //For now animation is bound to model load
        auto animator = animationManager->getAnimator(skinningComponent.animatorID);

        animator->animationPath = skinningComponent.animationPath;

        if(skinningComponent.initialAnimation == "") {
            const AnimationData* animationData = animationManager->getAnimationData(skinningComponent.animationPath);
            if(!animationData) {
                return;
            }
            if (!animationData->empty()) {
                auto it = animationData->begin();
                animator->animationName = it->first;
            }
        } else {
            animator->animationName = skinningComponent.initialAnimation;
        }
    };
    return function;
}

Entity::HookFunction Entity::_onModelComponentAdded()
{
    HookFunction function = [](Entity entity)
    {
        ModelComponent& modelComponent = entity.getComponent<ModelComponent>();
        ModelLoadEvent event(modelComponent.path, entity);
        EventManager::getInstance().publish(event);
    };
    return function;
}

Entity::HookFunction Entity::_onSpriteComponentAdded()
{
    HookFunction function = [](Entity entity)
    {
        SpriteComponent& sprite = entity.getComponent<SpriteComponent>();
        auto textureManager = &ServiceLocator::GetService<TextureManager>("TextureManagerVulkan");
        auto modelManager = &ServiceLocator::GetService<ModelManager>("ModelManager");
        auto meshManager = &ServiceLocator::GetService<MeshManager>("MeshManager");
        auto materialManager = &ServiceLocator::GetService<MaterialManager>("MaterialManagerVulkan");

        assert(textureManager && modelManager && meshManager && materialManager && "failed to get texture manager");

        if(!entity.hasComponent<ModelComponent>()) {
            entity.addComponent<ModelComponent>("$prim$Quad");
            _onModelComponentAdded()(entity);
        }

        if(!entity.hasComponent<AnimationComponent>()) {
            entity.addComponent<AnimationComponent>();
            _onAnimationComponentAdded()(entity);
        }

        if(sprite.path != "None") {
            sprite.textureID = textureManager->loadTexture(sprite.path);
        }

        if(sprite.textureID != 0) {
            ModelComponent& modelComponent = entity.getComponent<ModelComponent>();
            Model* model = modelManager->getModel(modelComponent.modelID);
            Mesh* mesh = meshManager->getMesh(model->meshIDs[0]);
            MaterialDesc material = materialManager->getMaterial(mesh->materialID);
            material.albedoIDs = { sprite.textureID };
            materialManager->updateMaterial(mesh->materialID, material);
        }
    };
    return function;
}

Entity::HookFunction Entity::_onAnimationComponentAdded()
{
    HookFunction function = [](Entity entity)
    {

    };
    return function;
}

Entity::HookFunction Entity::_onAnimationStateComponentAdded()
{
    HookFunction function = [](Entity entity)
    {
        // if (entity.hasComponent<NameComponent>()) {
        //     printf("animation state name: %s\n", entity.getComponent<NameComponent>().name.c_str());
        // } else {
        //     printf("animation state name: [NameComponent not parsed yet]\n");
        // }
    };
    return function;
}

Entity::HookFunction Entity::_onScriptComponentAdded()
{
    HookFunction function = [](Entity entity)
    {
        ScriptComponent& scriptComponent = entity.getComponent<ScriptComponent>();
        auto scriptManager = &ServiceLocator::GetService<ScriptManager>("ScriptManager");
        scriptManager->loadScript(entity, scriptComponent.path);
    };
    return function;
}

Entity::HookFunction Entity::_onScriptComponentRemoved()
{
    HookFunction function = [](Entity entity)
    {
        ScriptComponent& scriptComponent = entity.getComponent<ScriptComponent>();
        scriptComponent.onDestroy();
    };
    return function;
}

Entity::HookFunction Entity::_onColliderComponentAdded()
{
    HookFunction function = [](Entity entity)
    {
        auto modelManager = &ServiceLocator::GetService<ModelManager>("ModelManager");
        auto meshManager = &ServiceLocator::GetService<MeshManager>("MeshManager");
        auto physicsManager = &ServiceLocator::GetService<PhysicsManager>("PhysicsManager");

        if(!entity.hasComponent<ColliderComponent>()){
            printf("critical collider not found");
            return;
        }

        if(!entity.hasComponent<ModelComponent>()){
            printf("critical ModelComponent not found");
            return;
        }

        TransformComponent& transformComponent = entity.getComponent<TransformComponent>();
        ColliderComponent& colliderComponent = entity.getComponent<ColliderComponent>();

        ModelComponent& modelComponent = entity.getComponent<ModelComponent>();
        Model* model = modelManager->getModel(modelComponent.modelID);
        Mesh combinedMesh = meshManager->combineMeshes(model->meshIDs);
        Mesh* mesh = &combinedMesh;

        auto shapeID = physicsManager->createBoxBody(entity, *mesh, transformComponent.translateVec, transformComponent.scaleVec, colliderComponent.colliderType);
        colliderComponent.shapeID = shapeID;
    };
    return function;
}

Entity::HookFunction Entity::_onColliderComponentRemoved()
{
    HookFunction function = [](Entity entity)
    {
        auto physicsManager = &ServiceLocator::GetService<PhysicsManager>("PhysicsManager");
        physicsManager->destroy(entity.getComponent<ColliderComponent>().shapeID);
    };
    return function;
}
