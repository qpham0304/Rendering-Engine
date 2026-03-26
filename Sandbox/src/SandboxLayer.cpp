#include "SandboxLayer.h"
#include "core/scene/SceneManager.h"
#include "core/features/camera.h"
#include "window/AppWindow.h"
#include "core/resources/managers/MeshManager.h"
#include "core/resources/managers/MaterialManager.h"
#include "core/resources/managers/TextureManager.h"
#include "core/features/ServiceLocator.h"
#include "core/features/Mesh.h"


const std::vector<Vertex> vertices = {
   {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
   {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
   {{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
   {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},

   {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
   {{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
   {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
   {{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}
};

std::vector<uint32_t> indices = {
	0, 1, 2, 2, 3, 0,
	4, 5, 6, 6, 7, 4
};

SandBoxLayer::SandBoxLayer(const std::string& name)
    : Layer(name)
{

}

bool SandBoxLayer::init()
{
    meshManager = &ServiceLocator::GetService<MeshManager>("MeshManager");
    materialManager = &ServiceLocator::GetService<MaterialManager>("MaterialManagerVulkan");
    textureManager = &ServiceLocator::GetService<TextureManager>("TextureManagerVulkan");

    camera = std::make_unique<Camera>();
    camera->init(
        AppWindow::getWidth(),
        AppWindow::getHeight(),
        glm::vec3(5.0),
        glm::vec3(-5.0)
    );

    SceneManager::cameraController = camera.get();

    SceneManager::getInstance().addScene("Sanbox scene");
    Scene* scene = SceneManager::getInstance().getActiveScene();
    if (!scene) {
        return false;
    }
    
    setLogScopeEngine();
    scene->loadScene("assets/data/Level1-test.json");

    uint32_t planeID = scene->addEntity("plane");
    Entity planeEntity = scene->getEntity(planeID);
    TransformComponent& transform = planeEntity.getComponent<TransformComponent>();
    transform.translate(glm::vec3(2.0));

    MaterialDesc materialDesc;
    materialDesc.albedoIDs.push_back(textureManager->loadTexture("assets/textures/squish.png"));

    Mesh mesh;
    mesh.vertices = vertices;
    mesh.indices = indices;
    mesh.materialID = materialManager->createMaterial(materialDesc);
    
    MeshComponent m{};
    m.meshIDs.push_back(meshManager->loadMesh(mesh));
    planeEntity.addComponent<MeshComponent>(m);


	return true;
}

void SandBoxLayer::onAttach(LayerManager *manager)
{
    
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
