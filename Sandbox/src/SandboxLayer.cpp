#include "SandboxLayer.h"
#include "window/AppWindow.h"
#include "physics/PhysicsManager.h"
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

}

bool SandBoxLayer::init()
{
    setLogScopeClient();
    meshManager = &ServiceLocator::GetService<MeshManager>("MeshManager");
    materialManager = &ServiceLocator::GetService<MaterialManager>("MaterialManagerVulkan");
    textureManager = &ServiceLocator::GetService<TextureManager>("TextureManagerVulkan");
    modelManager = &ServiceLocator::GetService<ModelManager>("ModelManager");
    rendererManager = &ServiceLocator::GetService<RendererManager>("RendererManagerVulkan");
    auto physicsManager = &ServiceLocator::GetService<PhysicsManager>("PhysicsManager");

    // camera = std::make_unique<Camera>();
    // camera->init(AppWindow::getWidth(), AppWindow::getHeight(), glm::vec3(5.0), glm::vec3(-5.0));
    // SceneManager::cameraController = camera.get();
    camera = std::make_unique<ScriptableCamera>();

    // Scene* scene1 = SceneManager::getInstance().addScene("Level1");
    Scene* scene2 = SceneManager::getInstance().addScene("Level2");
    // Scene* scene3 = SceneManager::getInstance().addScene("Sanbox scene");
    
    // scene1->loadScene("assets/data/Level1.json");
    scene2->loadScene("assets/data/Level2.json");
    // scene3->loadScene("assets/data/default-scene.json");

    EventManager& eventManager = EventManager::getInstance();
    SceneManager::getInstance().setActiveScene(scene2->getName());
    Scene* activeScene = SceneManager::getInstance().getActiveScene();

    Entity cameraEntity = activeScene->getEntity(activeScene->addEntity("Camera"));
    TransformComponent& cameraTransform = cameraEntity.getComponent<TransformComponent>();
    cameraTransform.translateVec = glm::vec3(5.0);
    
    float width = static_cast<float>(AppWindow::getWidth());
    float height = static_cast<float>(AppWindow::getHeight());
    float aspectRatio = width / height;

    // glm::mat4 view = glm::lookAt(cameraTransform.translateVec, glm::vec3(0.0), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 view = cameraTransform.getModelMatrix();

    glm::mat4 projection = glm::ortho(
        -aspectRatio,		// Left
        aspectRatio,		// Right
        -1.0f,				// Bottom
        1.0f,				// Top
        -1.0f,				// Near
        1.0f				// Far
    );

    CameraComponent cameraComponent;
    cameraComponent.viewWidth = AppWindow::getWidth();
    cameraComponent.viewHeight = AppWindow::getHeight();
    cameraComponent.projection = projection;
    cameraComponent.view = view;
    cameraComponent.orientation = -cameraTransform.translateVec;
    cameraEntity.addComponent<CameraComponent>(cameraComponent);
    
    camera->setCamera(&cameraEntity.getComponent<CameraComponent>());

    eventManager.subscribe(EventType::CameraUpdateEvent, [this](Event& event) {
        CameraUpdateEvent& cameraUpdateEvent = static_cast<CameraUpdateEvent&>(event);
        CameraComponent& cameraComponent = cameraUpdateEvent.camera;
        camera->setCamera(&cameraComponent);
    });
    
    
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
        float zPos = posDist(gen);
        transform.translate(glm::vec3(xPos, 2.0f, zPos));

        MaterialDesc materialDesc;
        materialDesc.albedoIDs.push_back(
            // textureManager->loadTexture("assets/textures/mobi-padoru.png", 1, false)
            textureManager->loadTexture("assets/textures/pbr/gold/metallic.png", 1, false)
        );

        materialDesc.emissiveIDs.push_back(
            textureManager->loadTexture("assets/textures/pbr/gold/metallic.png", 1, false)
        );
        // these need to be set to have emision
        materialDesc.emissive = 5.0;

        Mesh mesh = EngineUtils::drawSphere(0.5f, 36, 36);
        mesh.materialID = materialManager->createMaterial(materialDesc);

        Model model {};
        model.meshIDs.push_back(meshManager->loadMesh(mesh));
        ModelComponent modelComponent;
        modelComponent.modelID = modelManager->addModel(model);
        lightEntity.addComponent<ModelComponent>(modelComponent);

        glm::vec4 randomColor(colorDist(gen), colorDist(gen), colorDist(gen), 1.0f);
        lightEntity.addComponent<LightComponent>(randomColor, 15.0f, 1.0f);

        //TODO: body should be according to transform as well
        uint32_t bodyID = physicsManager->createBody(mesh, transform.translateVec, transform.scaleVec, false);
        lightEntity.addComponent<ColliderComponent>(bodyID);
    }

    // light probe manager component
	// const uint32_t probesPerDimension = 8;
	// float spacing = 2.0f;
    // uint32_t lightProbeEntityID = activeScene->addEntity("probe manager");
    // Entity lightProbeEntity = activeScene->getEntity(lightProbeEntityID);
    
    // TransformComponent& transform = lightProbeEntity.getComponent<TransformComponent>();
    // transform.translate(glm::vec3(0.0f, 0.0f, 0.0f));

    // MaterialDesc materialDesc;
    // materialDesc.albedoIDs.push_back(
    //     // textureManager->loadTexture("assets/textures/mobi-padoru.png", 1, false)
    //     textureManager->loadTexture("assets/textures/pbr/gold/metallic.png", 1, false)
    // );

    // Mesh mesh = EngineUtils::drawSphere(0.25f, 18, 18);
    // mesh.materialID = materialManager->createMaterial(materialDesc);

    
    // float offset = (probesPerDimension - 1) * spacing * 0.5f;
    // auto& lightProbeComponent = lightProbeEntity.addComponent<LightProbeComponent>();
    // lightProbeComponent.probeGrid.resize(probesPerDimension * probesPerDimension * probesPerDimension);
    // lightProbeComponent.bufferSize = lightProbeComponent.probeGrid.size() * sizeof(glm::vec4);  //NOTE: assuming probe is glm::vec4
    // lightProbeComponent.probesPerDimension = probesPerDimension;
    // lightProbeComponent.spacing = spacing;
    // lightProbeComponent.gridOrigin = glm::vec4(-offset, -offset, -offset, 1.0);
    
    // Offset to center the grid (so 0,0,0 is the middle the volume)
    // for (uint32_t z = 0; z < probesPerDimension; z++) {
    //     for (uint32_t y = 0; y < probesPerDimension; y++) {
    //         for (uint32_t x = 0; x < probesPerDimension; x++) {
    //             uint32_t index = x + (y * probesPerDimension) + (z * probesPerDimension * probesPerDimension);
                
    //             lightProbeComponent.probeGrid[index] = glm::vec4(
    //                 (float)x * spacing - offset,
    //                 (float)y * spacing - offset,
    //                 (float)z * spacing - offset,
    //                 1.0
    //             );
    //         }
    //     }
    // }
    // for (uint32_t y = 0; y < probesPerDimension; y++) {
    //     for (uint32_t z = 0; z < probesPerDimension; z++) {
    //         for (uint32_t x = 0; x < probesPerDimension; x++) {
                
    //             // Re-map the index calculation to match this layout
    //             uint32_t index = x + (z * probesPerDimension) + (y * probesPerDimension * probesPerDimension);
                
    //             lightProbeComponent.probeGrid[index] = glm::vec4(
    //                 (float)x * spacing - offset,
    //                 (float)y * spacing - offset,
    //                 (float)z * spacing - offset,
    //                 1.0
    //             );
    //         }
    //     }
    // }


    // Model model {};
    // uint32_t meshID = meshManager->loadMesh(mesh);
    // model.meshIDs.push_back(meshID);
    // for(int i = 1; i < lightProbeComponent.probeGrid.size(); i++) {
    //     model.meshIDs.push_back(meshID);
    // }
    // ModelComponent modelComponent;
    // modelComponent.modelID = modelManager->addModel(model);
    // lightProbeEntity.addComponent<ModelComponent>(modelComponent);

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
        SceneManager::cameraController = camera.get();
    }
}

void SandBoxLayer::onGuiUpdate()
{

}

void SandBoxLayer::onEvent(Event &event)
{
    
}
