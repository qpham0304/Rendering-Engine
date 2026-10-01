#include "SandboxLayer.h"
#include "window/AppWindow.h"
#include "physics/PhysicsManager.h"
#include "particle/ParticleManager.h"
#include "core/scene/SceneManager.h"
#include "core/features/camera.h"
#include "core/features/ScriptableCamera.h"
#include "core/resources/managers/MeshManager.h"
#include "core/resources/managers/MaterialManager.h"
#include "core/resources/managers/ModelManager.h"
#include "core/resources/managers/TextureManager.h"
#include "core/resources/managers/RendererManager.h"
#include "core/features/ServiceLocator.h"
#include "core/events/EventManager.h"
#include "core/features/Mesh.h"
#include "core/features/EngineUtils.h"
#include "core/features/EngineStates.h"
#include <random>

SandBoxLayer::SandBoxLayer(const std::string& name)
    : Layer(name)
{
    EventManager::getInstance().subscribe(EventType::CameraUpdateEvent, [this](Event& event) {
        auto& cameraUpdateEvent = static_cast<CameraUpdateEvent&>(event);
        auto& cameraComponent = cameraUpdateEvent.entity.getComponent<CameraComponent>();
        camera->setCamera(&cameraComponent);
    });
}

bool SandBoxLayer::init()
{
    setLogScopeClient();
    meshManager = &ServiceLocator::GetService<MeshManager>("MeshManager");
    materialManager = &ServiceLocator::GetService<MaterialManager>("MaterialManagerVulkan");
    textureManager = &ServiceLocator::GetService<TextureManager>("TextureManagerVulkan");
    modelManager = &ServiceLocator::GetService<ModelManager>("ModelManager");
    rendererManager = &ServiceLocator::GetService<RendererManager>("RendererManagerVulkan");
    physicsManager = &ServiceLocator::GetService<PhysicsManager>("PhysicsManager");
    particleManager = &ServiceLocator::GetService<ParticleManager>("ParticleManager");

    
    // Scene* scene1 = SceneManager::getInstance().addScene("Level1");
    Scene* scene2 = SceneManager::getInstance().addScene("Level2");
    // Scene* scene3 = SceneManager::getInstance().addScene("Sanbox scene");
    
    // scene1->loadScene("assets/data/Level1.json");
    scene2->loadScene("assets/data/Level2.json");
    // scene3->loadScene("assets/data/default-scene.json");

    EventManager& eventManager = EventManager::getInstance();
    SceneManager::getInstance().setActiveScene(scene2->getName());

    createScriptableCamera();
    createLights();
	createLightProbes();
    createParticle();

    // rendererManager->addRenderer();

	return true;
}

void SandBoxLayer::onAttach(LayerManager *manager)
{
    Layer::onAttach(manager);
}

void SandBoxLayer::onDetach()
{

}

void SandBoxLayer::onUpdate()
{
    if(EngineState::isPlaying()) {
        // SceneManager::cameraController = camera.get();
    }
}

void SandBoxLayer::onGuiUpdate()
{

}

void SandBoxLayer::onEvent(Event &event)
{
    
}

void SandBoxLayer::createLights()
{
    Scene* activeScene = SceneManager::getInstance().getActiveScene();

    const int numLights = 10;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> posDist(-numLights, numLights);
    std::uniform_real_distribution<float> colorDist(0.0f, 1.0f);

    for (int i = 0; i < numLights; ++i) {
        std::string entityName = "light_sphere_" + std::to_string(i);
        Entity lightEntity = activeScene->getEntity(activeScene->addEntity(entityName));

        TransformComponent& transform = lightEntity.getComponent<TransformComponent>();
        float xPos = posDist(gen);
        float yPos = posDist(gen);
        float zPos = posDist(gen);
        transform.translate(glm::vec3(xPos, yPos, zPos));

        MaterialDesc materialDesc;
        materialDesc.albedoIDs.push_back(
            textureManager->loadTexture("assets/textures/pbr/gold/metallic.png", 1, false)
        );

        materialDesc.emissiveIDs.push_back(
            textureManager->loadTexture("assets/textures/pbr/gold/metallic.png", 1, false)
        );
        
        materialDesc.emissive = 5.0;    // these need to be set to have emision

        float radius = 0.5;
        Mesh mesh = EngineUtils::drawSphere(radius, 36, 36);
        mesh.materialID = materialManager->createMaterial(materialDesc);

        Model model {};
        model.meshIDs.push_back(meshManager->loadMesh(mesh));
        ModelComponent modelComponent;
        modelComponent.modelID = modelManager->addModel(model);
        lightEntity.addComponent<ModelComponent>(modelComponent);

        glm::vec4 randomColor(colorDist(gen), colorDist(gen), colorDist(gen), 1.0f);
        lightEntity.addComponent<LightComponent>(randomColor, 15.0f, 1.0f);

        //TODO: body should be according to transform as well
        // uint32_t bodyID = physicsManager->createSphereBody(
        //     lightEntity,
        //     mesh,
        //     transform.translateVec,
        //     transform.scaleVec,
        //     radius,
        //     static_cast<uint32_t>(ColliderType::Dynamic)
        // );
        // lightEntity.addComponent<ColliderComponent>(bodyID, static_cast<uint32_t>(ColliderType::Dynamic));
    }
}

void SandBoxLayer::createLightProbes()
{
    const uint32_t probesPerDimension = 8;
	float spacing = 2.0f;

    Scene* activeScene = SceneManager::getInstance().getActiveScene();
    uint32_t lightProbeEntityID = activeScene->addEntity("probe manager");
    Entity lightProbeEntity = activeScene->getEntity(lightProbeEntityID);
    
    TransformComponent& transform = lightProbeEntity.getComponent<TransformComponent>();
    transform.translate(glm::vec3(0.0f, 0.0f, 0.0f));

    MaterialDesc materialDesc;
    materialDesc.albedoIDs.push_back(
        textureManager->loadTexture("assets/textures/pbr/gold/metallic.png", 1, false)
    );

    Mesh mesh = EngineUtils::drawSphere(0.25f, 18, 18);
    mesh.materialID = materialManager->createMaterial(materialDesc);

    
    float offset = (probesPerDimension - 1) * spacing * 0.5f;
    auto& lightProbeComponent = lightProbeEntity.addComponent<LightProbeComponent>();
    lightProbeComponent.probeGrid.resize(probesPerDimension * probesPerDimension * probesPerDimension);
    lightProbeComponent.bufferSize = lightProbeComponent.probeGrid.size() * sizeof(glm::vec4);  //NOTE: assuming probe is glm::vec4
    lightProbeComponent.probesPerDimension = probesPerDimension;
    lightProbeComponent.spacing = spacing;
    lightProbeComponent.gridOrigin = glm::vec4(-offset, -offset, -offset, 1.0);
    
    // Offset to center the grid (so 0,0,0 is the middle the volume)
    for (uint32_t z = 0; z < probesPerDimension; z++) {
        for (uint32_t y = 0; y < probesPerDimension; y++) {
            for (uint32_t x = 0; x < probesPerDimension; x++) {
                uint32_t index = x + (y * probesPerDimension) + (z * probesPerDimension * probesPerDimension);
                
                lightProbeComponent.probeGrid[index] = glm::vec4(
                    (float)x * spacing - offset,
                    (float)y * spacing - offset,
                    (float)z * spacing - offset,
                    1.0
                );
            }
        }
    }
    for (uint32_t y = 0; y < probesPerDimension; y++) {
        for (uint32_t z = 0; z < probesPerDimension; z++) {
            for (uint32_t x = 0; x < probesPerDimension; x++) {
                
                // Re-map the index calculation to match this layout
                uint32_t index = x + (z * probesPerDimension) + (y * probesPerDimension * probesPerDimension);
                
                lightProbeComponent.probeGrid[index] = glm::vec4(
                    (float)x * spacing - offset,
                    (float)y * spacing - offset,
                    (float)z * spacing - offset,
                    1.0
                );
            }
        }
    }


    Model model {};
    uint32_t meshID = meshManager->loadMesh(mesh);
    model.meshIDs.push_back(meshID);
    for(int i = 1; i < lightProbeComponent.probeGrid.size(); i++) {
        model.meshIDs.push_back(meshID);
    }
    ModelComponent modelComponent;
    modelComponent.modelID = modelManager->addModel(model);
    lightProbeEntity.addComponent<ModelComponent>(modelComponent);
}

void SandBoxLayer::createScriptableCamera()
{
    // camera = std::make_unique<Camera>();
    // camera->init(AppWindow::getWidth(), AppWindow::getHeight(), glm::vec3(5.0), glm::vec3(-5.0));
    // SceneManager::cameraController = camera.get();
    camera = std::make_unique<ScriptableCamera>();

    // Entity cameraEntity = activeScene->getEntity(activeScene->addEntity("Camera"));
    // TransformComponent& cameraTransform = cameraEntity.getComponent<TransformComponent>();
    // cameraTransform.translateVec = glm::vec3(5.0);
    
    // float width = static_cast<float>(AppWindow::getWidth());
    // float height = static_cast<float>(AppWindow::getHeight());
    // float aspectRatio = width / height;

    // // glm::mat4 view = glm::lookAt(cameraTransform.translateVec, glm::vec3(0.0), glm::vec3(0.0f, 1.0f, 0.0f));
    // glm::mat4 view = cameraTransform.getModelMatrix();

    // glm::mat4 projection = glm::ortho(
    //     -aspectRatio,		// Left
    //     aspectRatio,		// Right
    //     -1.0f,				// Bottom
    //     1.0f,				// Top
    //     -1.0f,				// Near
    //     1.0f				// Far
    // );

    // CameraComponent cameraComponent;
    // cameraComponent.viewWidth = AppWindow::getWidth();
    // cameraComponent.viewHeight = AppWindow::getHeight();
    // cameraComponent.projection = projection;
    // cameraComponent.view = view;
    // cameraComponent.orientation = -cameraTransform.translateVec;
    // cameraEntity.addComponent<CameraComponent>(cameraComponent);
    
    // camera->setCamera(&cameraEntity.getComponent<CameraComponent>());
}

void SandBoxLayer::createParticle()
{
    Scene* activeScene = SceneManager::getInstance().getActiveScene();
    
    Entity particleEntity = activeScene->getEntity(activeScene->addEntity("particleEntity"));
    particleEntity.addComponent<ParticleEmitter>();
    ParticleEmitter& emitter = particleEntity.getComponent<ParticleEmitter>();
    emitter.emitMax = 5000;
    emitter.emitCount = 5000;
    emitter.areRecycled = true; // Enable particle recycling so dead particles respawn
    emitter.emitRate = 1000.0f; // Spawning rate if using rate-based emission
    emitter.lifetimeMin = 1.5f; // Minimum seconds a particle lives
    emitter.lifetimeMax = 3.0f; // Maximum seconds a particle lives
    emitter.speedMin = 4.0f;    // Minimum upward burst speed
    emitter.speedMax = 8.0f;    // Maximum upward burst speed
    emitter.spawnPosition = glm::vec3(0.0f, 0.0f, 0.0f);
    emitter.force = glm::vec3(0.0, 0.0, 0.0);
    emitter.resetPosition = false;
    emitter.behaviorType = 0;
    emitter.containerID = particleManager->createContainer(5000, glm::vec3(-1.0), glm::vec3(1.0));
    
    particleEntity.addComponent<SpriteComponent>();
    auto& spriteComponent = particleEntity.getComponent<SpriteComponent>();
    // spriteComponent.textureID = textureManager->loadTexture("assets/textures/ParticleAtlas.png", 1, false);;
    spriteComponent.textureID = textureManager->loadTexture("assets/textures/mobi-padoru.png", 1, false);;
    particleEntity.onSpriteComponentAdded();

    Entity particleEntity2 = activeScene->getEntity(activeScene->addEntity("particleEntity2"));
    particleEntity2.addComponent<ParticleEmitter>();
    TransformComponent& transform2 = particleEntity2.getComponent<TransformComponent>();
    transform2.translate(glm::vec3(5.0, 0.0, 0.0));
    ParticleEmitter& emitter2 = particleEntity2.getComponent<ParticleEmitter>();
    emitter2.emitMax = 5000;
    emitter2.emitCount = 5000;
    emitter2.areRecycled = true; // Enable particle recycling so dead particles respawn
    emitter2.emitRate = 1000.0f; // Spawning rate if using rate-based emission
    emitter2.lifetimeMin = 1.5f; // Minimum seconds a particle lives
    emitter2.lifetimeMax = 3.0f; // Maximum seconds a particle lives
    emitter2.speedMin = 4.0f;    // Minimum upward burst speed
    emitter2.speedMax = 8.0f;    // Maximum upward burst speed
    emitter2.spawnPosition = transform2.translateVec;
    emitter2.force = glm::vec3(0.0, 0.0, 0.0);
    emitter2.resetPosition = false;
    emitter2.behaviorType = 0;
    emitter2.containerID = particleManager->createContainer(5000, glm::vec3(1.0), glm::vec3(1.0));

}
