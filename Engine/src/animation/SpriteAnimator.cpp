#include "SpriteAnimator.h"
#include "core/components/MComponent.h"
#include "core/features/ServiceLocator.h"
#include "core/scene/SceneManager.h"
#include "core/events/EventManager.h"
#include "core/resources/managers/ModelManager.h"
#include "core/resources/managers/MeshManager.h"
#include "core/resources/managers/MaterialManager.h"


SpriteAnimator::SpriteAnimator()
    : sceneManager(SceneManager::getInstance())
{

}

SpriteAnimator::~SpriteAnimator()
{

}

void SpriteAnimator::play(std::string_view name)
{

}

void SpriteAnimator::stop()
{
    
}

void SpriteAnimator::isPlaying()
{

}

void SpriteAnimator::onUpdate(float dt)
{
    for(uint32_t sceneID : sceneManager.listIDs()) {
        Scene* scene = sceneManager.getScene(sceneID);

        auto func = std::function<void(Entity)>([&](Entity entity) -> void {
            auto& animation = entity.getComponent<AnimationComponent>();
            auto& sprite = entity.getComponent<SpriteComponent>();

            if (!animation.isRunning) {
                return;
            }

            animation.frameDelay -= dt;
            if (animation.frameDelay <= 0.0f) {
                int nextFrame = sprite.frameIndex + 1;
                
                if (nextFrame >= animation.frameCount) {
                    if (animation.isLooping) {
                        nextFrame = 0;
                    } else {
                        nextFrame = animation.frameCount - 1;
                        animation.isRunning = false;
                    }
                }
                
                if (animation.isRunning) {
                    sprite.setFrame(nextFrame); 
                    animation.frameDelay += animation.frameDuration;
                } else {
                    animation.frameDelay = 0;
                }
            }

            auto textureManager = &ServiceLocator::GetService<TextureManager>("TextureManagerVulkan");
            auto modelManager = &ServiceLocator::GetService<ModelManager>("ModelManager");
            auto meshManager = &ServiceLocator::GetService<MeshManager>("MeshManager");
            auto materialManager = &ServiceLocator::GetService<MaterialManager>("MaterialManagerVulkan");
            
            glm::vec2 uvScale = {1.0 / sprite.numRows, 1.0 / sprite.numCols};
            int row = sprite.frameIndex % sprite.numRows;
            int col = sprite.frameIndex % sprite.numCols;
            glm::vec2 uvOffset = {uvScale.x * row, uvScale.y * col};

            ModelComponent& modelComponent = entity.getComponent<ModelComponent>();
            Model* model = modelManager->getModel(modelComponent.modelID);
            Mesh* mesh = meshManager->getMesh(model->meshIDs[0]);
            MaterialDesc material = materialManager->getMaterial(mesh->materialID);
            material.uv = uvOffset;
            material.uvScale = uvScale;
            materialManager->updateMaterial(mesh->materialID, material);
        
        });

        scene->forEnitiesWith<SpriteComponent, AnimationComponent>(func);
    }
}
