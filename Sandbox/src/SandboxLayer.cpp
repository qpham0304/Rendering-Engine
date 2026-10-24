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
#include <cfloat>

SandBoxLayer::SandBoxLayer(const std::string& name)
    : Layer(name)
{
    setFootprint("SandBoxLayer", sizeof(SandBoxLayer), MemorySystem::General);
    m_eventSubscriptions.push_back(EventManager::getInstance().subscribe(EventType::CameraUpdateEvent, [this](Event& event) {
        auto& cameraUpdateEvent = static_cast<CameraUpdateEvent&>(event);
        auto& cameraComponent = cameraUpdateEvent.entity.getComponent<CameraComponent>();
        camera->setCamera(&cameraComponent);
    }));
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

    Scene* scene = SceneManager::getInstance().addScene("sandbox-scene");
    scene->loadScene("assets/data/sandbox-scene.json");
    // Scene* scene1 = SceneManager::getInstance().addScene("Level1");
    // scene1->loadScene("assets/data/Level1.json");
    // Scene* scene2 = SceneManager::getInstance().addScene("Level2");
    // scene2->loadScene("assets/data/Level2.json");
    // Scene* scene3 = SceneManager::getInstance().addScene("Sanbox scene");
    // scene3->loadScene("assets/data/default-scene.json");

    EventManager& eventManager = EventManager::getInstance();
    SceneManager::getInstance().setActiveScene(scene->getName());

    // createScriptableCamera();
    // createLights();
	createLightProbes();
    // createParticle();
    createRibbons();

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

void SandBoxLayer::onUpdate(const FrameTime& frameTime)
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
    // maximum number of probes defined by the ddgi pass
    const uint32_t maxProbesPerAxis = 24;
	float spacing = 2.0f;

    Scene* activeScene = SceneManager::getInstance().getActiveScene();

    // fit the grid to everything in the scene: a surface outside the grid only gets the open sky, not the probes
    glm::vec3 sceneMin(FLT_MAX);
    glm::vec3 sceneMax(-FLT_MAX);
    for (auto& entity : activeScene->getEntitiesWith<TransformComponent, ModelComponent>()) {
        glm::mat4 modelMatrix = entity.getComponent<TransformComponent>().getModelMatrix();
        const Model* model = modelManager->getModel(entity.getComponent<ModelComponent>().modelID);
        if (!model) {
            continue;
        }
        for (uint32_t meshID : model->meshIDs) {
            for (const auto& vertex : meshManager->getMesh(meshID)->vertices) {
                glm::vec3 worldPos = glm::vec3(modelMatrix * glm::vec4(vertex.positions, 1.0f));
                sceneMin = (glm::min)(sceneMin, worldPos);
                sceneMax = (glm::max)(sceneMax, worldPos);
            }
        }
    }
    if (sceneMin.x > sceneMax.x) {
        sceneMin = glm::vec3(-7.0f);
        sceneMax = glm::vec3(7.0f);
    }

    // widen the spacing instead of cutting the grid short when the scene is too big for the probe limit
    glm::vec3 extent = sceneMax - sceneMin;
    float largestExtent = (glm::max)(extent.x, (glm::max)(extent.y, extent.z));
    spacing = (glm::max)(spacing, largestExtent / (float)(maxProbesPerAxis - 1));
    glm::uvec3 probeCounts = glm::clamp(glm::uvec3(glm::ceil(extent / spacing)) + 1u, glm::uvec3(2), glm::uvec3(maxProbesPerAxis));

    uint32_t lightProbeEntityID = activeScene->addEntity("probe manager");
    Entity lightProbeEntity = activeScene->getEntity(lightProbeEntityID);

    // the grid is centered on the entity
    TransformComponent& transform = lightProbeEntity.getComponent<TransformComponent>();
    transform.translate((sceneMin + sceneMax) * 0.5f);

    glm::vec3 offset = glm::vec3(probeCounts - 1u) * spacing * 0.5f;
    auto& lightProbeComponent = lightProbeEntity.addComponent<LightProbeComponent>();
    lightProbeComponent.probeGrid.resize(probeCounts.x * probeCounts.y * probeCounts.z);
    lightProbeComponent.bufferSize = lightProbeComponent.probeGrid.size() * sizeof(glm::vec4);  //NOTE: assuming probe is glm::vec4
    lightProbeComponent.probesPerDimension = probeCounts.x;
    lightProbeComponent.probeCounts = probeCounts;
    lightProbeComponent.spacing = spacing;
    // world position of probe (0, 0, 0), the ddgi shaders rebuild every probe position from this and the spacing
    lightProbeComponent.gridOrigin = glm::vec4(transform.translateVec - offset, 1.0);
    
    // local positions (relative to the entity) for drawing the probes
    // index order has to match ddgiProbeIndex in assets/shaders/common/ddgi.glsl
    for (uint32_t y = 0; y < probeCounts.y; y++) {
        for (uint32_t z = 0; z < probeCounts.z; z++) {
            for (uint32_t x = 0; x < probeCounts.x; x++) {
                uint32_t index = x + (z * probeCounts.x) + (y * probeCounts.x * probeCounts.z);
                
                lightProbeComponent.probeGrid[index] = glm::vec4(glm::vec3(x, y, z) * spacing - offset, 1.0);
            }
        }
    }

    // debug spheres, one mesh per probe
    // MaterialDesc materialDesc;
    // materialDesc.albedoIDs.push_back(
    //     textureManager->loadTexture("assets/textures/pbr/gold/metallic.png", 1, false)
    // );
    // Mesh mesh = EngineUtils::drawSphere(0.25f, 18, 18);
    // mesh.materialID = materialManager->createMaterial(materialDesc);
    // Model model {};
    // uint32_t meshID = meshManager->loadMesh(mesh);
    // model.meshIDs.push_back(meshID);
    // for(int i = 1; i < lightProbeComponent.probeGrid.size(); i++) {
    //     model.meshIDs.push_back(meshID);
    // }
    // ModelComponent modelComponent;
    // modelComponent.modelID = modelManager->addModel(model);
    // lightProbeEntity.addComponent<ModelComponent>(modelComponent);
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
    emitter.maxParticles = 5000;
    emitter.emitRate = 1000.0f;
    emitter.lifetimeMin = 1.5f;
    emitter.lifetimeMax = 3.0f;
    emitter.spawnShape = static_cast<int>(ParticleSpawnShape::Circle);
    emitter.shapeRadius = 1.0f;
    emitter.speedMin = 1.0f;
    emitter.speedMax = 3.0f;
    emitter.spreadAngle = 10.0f;
    emitter.turbulenceStrength = 2.0f;
    emitter.sizeMin = 0.2f;
    emitter.sizeMax = 0.4f;
    emitter.sizeEndScale = 0.2f;
    emitter.angularVelocityMin = -90.0f;
    emitter.angularVelocityMax = 90.0f;
    emitter.colorStart = glm::vec4(1.0f, 0.8f, 0.4f, 1.0f);
    emitter.colorEnd = glm::vec4(1.0f, 0.2f, 0.05f, 0.0f);
    emitter.fadeIn = 0.1f;
    emitter.texturePath = "assets/textures/mobi-padoru.png";
    particleEntity.getComponent<TransformComponent>().translate(glm::vec3(-5.0f, 0.0f, 0.0f));
    emitter.containerID = particleManager->createContainer(emitter.maxParticles);

    
    
    Entity particleEntity2 = activeScene->getEntity(activeScene->addEntity("particleEntity2"));
    particleEntity2.addComponent<ParticleEmitter>();
    TransformComponent& transform2 = particleEntity2.getComponent<TransformComponent>();
    transform2.translate(glm::vec3(5.0, 0.0, 0.0));
    ParticleEmitter& emitter2 = particleEntity2.getComponent<ParticleEmitter>();
    emitter2.maxParticles = 2000;
    emitter2.emitRate = 0.0f;
    emitter2.burstCount = 300;
    emitter2.burstInterval = 2.0f;
    emitter2.lifetimeMin = 1.5f;
    emitter2.lifetimeMax = 2.0f;
    emitter2.velocityFromShape = true;
    emitter2.spreadAngle = 0.0f;
    emitter2.speedMin = 4.0f;
    emitter2.speedMax = 8.0f;
    emitter2.gravityScale = 1.0f;
    emitter2.drag = 0.5f;
    emitter2.sizeMin = 0.1f;
    emitter2.sizeMax = 0.25f;
    emitter2.angularVelocityMin = 180.0f;
    emitter2.angularVelocityMax = 540.0f;
    emitter2.colorStart = glm::vec4(0.6f, 0.8f, 1.0f, 1.0f);
    emitter2.colorEnd = glm::vec4(0.2f, 0.3f, 1.0f, 1.0f);
    emitter2.fadeOut = 0.3f;
    emitter2.renderMode = static_cast<int>(ParticleRenderMode::Mesh);
    emitter2.blendMode = static_cast<int>(ParticleBlendMode::Alpha);
    emitter2.meshPath = "$prim$Cube";
    emitter2.containerID = particleManager->createContainer(emitter2.maxParticles);
}

void SandBoxLayer::createRibbons()
{
    Scene* activeScene = SceneManager::getInstance().getActiveScene();

    // lightning striking down from above the origin, bright HDR blue so it blooms
    Entity lightningEntity = activeScene->getEntity(activeScene->addEntity("lightning"));
    lightningEntity.getComponent<TransformComponent>().translate(glm::vec3(0.0f, 6.0f, 0.0f));
    lightningEntity.addComponent<RibbonComponent>();
    RibbonComponent& lightning = lightningEntity.getComponent<RibbonComponent>();
    lightning.mode = static_cast<int>(RibbonMode::Lightning);
    lightning.target = glm::vec3(0.0f, -6.0f, 0.0f);
    lightning.segments = 32;
    lightning.jitter = 0.12f;
    lightning.branches = 3;
    lightning.widthStart = 0.08f;
    lightning.widthEnd = 0.04f;
    lightning.colorStart = glm::vec4(2.0f, 2.4f, 4.0f, 1.0f);
    lightning.colorEnd = glm::vec4(1.2f, 1.5f, 3.0f, 0.8f);
}
