#include "SandboxLayer.h"
#include "core/scene/SceneManager.h"
#include "core/features/camera.h"
#include "window/AppWindow.h"
#include "core/resources/managers/MeshManager.h"
#include "core/resources/managers/MaterialManager.h"
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

    const int numLights = 100;
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
            textureManager->loadTexture(
                // "assets/textures/hdr/photo_studio_loft_hall_2k.hdr", 
                "assets/textures/mobi-padoru.png", 
                1, 
                false
            )
        );

        Mesh mesh = EngineUtils::drawSphere(0.5f, 36, 36);
        mesh.materialID = materialManager->createMaterial(materialDesc);

        MeshComponent m{};
        m.meshIDs.push_back(meshManager->loadMesh(mesh));
        lightEntity.addComponent<MeshComponent>(m);

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
