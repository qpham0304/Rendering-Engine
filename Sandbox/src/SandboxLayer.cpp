#include "SandboxLayer.h"
#include "core/scene/SceneManager.h"
#include "core/features/camera.h"
#include "window/AppWindow.h"
#include "core/resources/managers/MeshManager.h"
#include "core/resources/managers/MaterialManager.h"
#include "core/resources/managers/ModelManager.h"
#include "core/resources/managers/TextureManager.h"
#include "core/features/ServiceLocator.h"
#include "core/features/Mesh.h"
#include "core/features/EngineUtils.h"
#include <random>

SandBoxLayer::SandBoxLayer(const std::string& name)
    : Layer(name)
{

}

bool SandBoxLayer::init()
{
    meshManager = &ServiceLocator::GetService<MeshManager>("MeshManager");
    materialManager = &ServiceLocator::GetService<MaterialManager>("MaterialManagerVulkan");
    textureManager = &ServiceLocator::GetService<TextureManager>("TextureManagerVulkan");
    modelManager = &ServiceLocator::GetService<ModelManager>("ModelManager");

    // camera = std::make_unique<Camera>();
    // camera->init(
    //     AppWindow::getWidth(),
    //     AppWindow::getHeight(),
    //     glm::vec3(5.0),
    //     glm::vec3(-5.0)
    // );

    // SceneManager::cameraController = camera.get();

    SceneManager::getInstance().addScene("Sanbox scene");
    Scene* scene = SceneManager::getInstance().getActiveScene();
    if (!scene) {
        return false;
    }
    
    setLogScopeEngine();
    scene->loadScene("assets/data/Level1-test.json");
    // scene->loadScene("assets/data/default-scene.json");

    const int numLights = 1;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> posDist(-numLights, numLights);
    std::uniform_real_distribution<float> colorDist(0.0f, 1.0f);

    for (int i = 0; i < numLights; ++i) {
        std::string entityName = "light_sphere_" + std::to_string(i);
        uint32_t lightID = scene->addEntity(entityName);
        Entity lightEntity = scene->getEntity(lightID);

        TransformComponent& transform = lightEntity.getComponent<TransformComponent>();
        float xPos = posDist(gen);
        float zPos = posDist(gen);
        transform.translate(glm::vec3(xPos, 2.0f, zPos));

        MaterialDesc materialDesc;
        materialDesc.albedoIDs.push_back(
            textureManager->loadTexture("assets/textures/mobi-padoru.png", 1, false)
        );

        materialDesc.emissiveIDs.push_back(
            textureManager->loadTexture("assets/textures/pbr/gold/metallic.png", 1, false)
        );
        // these need to be set to have emision
        materialDesc.emissive = 1.0;

        Mesh mesh = EngineUtils::drawSphere(0.5f, 36, 36);
        mesh.materialID = materialManager->createMaterial(materialDesc);

        Model model {};
        model.meshIDs.push_back(meshManager->loadMesh(mesh));
        ModelComponent modelComponent;
        modelComponent.modelID = modelManager->addModel(model);
        lightEntity.addComponent<ModelComponent>(modelComponent);

        glm::vec4 randomColor(colorDist(gen), colorDist(gen), colorDist(gen), 1.0f);

        lightEntity.addComponent<LightComponent>(randomColor, 15.0f, 1.0f);
    }

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

}

void SandBoxLayer::onGuiUpdate()
{

}

void SandBoxLayer::onEvent(Event &event)
{
    
}
