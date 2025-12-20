#include "SandboxLayer.h"
#include "core/scene/SceneManager.h"
#include "core/features/camera.h"
#include "window/AppWindow.h"

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

std::vector<uint16_t> indices = {
	0, 1, 2, 2, 3, 0,
	4, 5, 6, 6, 7, 4
};

SandBoxLayer::SandBoxLayer(const std::string& name)
    : Layer(name)
{

}

bool SandBoxLayer::init()
{
    camera = std::make_unique<Camera>();
    camera->init(
        AppWindow::getWidth(),
        AppWindow::getHeight(),
        glm::vec3(1.0, 0.0, 0.0),
        glm::vec3(-1.0)
    );

    SceneManager::cameraController = camera.get();

    SceneManager::getInstance().addScene("Sanbox scene");
    Scene* scene = SceneManager::getInstance().getActiveScene();
    if (!scene) {
        return false;
    }

    uint32_t aruEntityID = scene->addEntity("Aru Entity");
    Entity aruEntity = scene->getEntity(aruEntityID);
    aruEntity.addComponent<ModelComponent>("assets/models/aru/aru.gltf");
    aruEntity.onModelComponentAdded();
    aruEntity.getComponent<TransformComponent>().translate(glm::vec3(1.0));

    uint32_t aruEntityID2 = scene->addEntity("Aru Entity2");
    Entity aruEntity2 = scene->getEntity(aruEntityID2);
    aruEntity2.addComponent<ModelComponent>("assets/models/aru/aru.gltf");
    aruEntity2.onModelComponentAdded();
    aruEntity2.getComponent<TransformComponent>().translate(glm::vec3(-1.0));

    uint32_t helmetEntityID = scene->addEntity("Damage Helmet");
    Entity helmetEntity = scene->getEntity(helmetEntityID);
    helmetEntity.addComponent<ModelComponent>("assets/models/DamagedHelmet/gltf/DamagedHelmet.gltf");
    helmetEntity.onModelComponentAdded();
    helmetEntity.getComponent<TransformComponent>().translate(glm::vec3(2.0));

    uint32_t cubEntityID = scene->addEntity("cube");
    Entity cubeEntity = scene->getEntity(cubEntityID);
    cubeEntity.addComponent<ModelComponent>("assets/models/cube/cube-notex.gltf");
    cubeEntity.onModelComponentAdded();
    cubeEntity.getComponent<TransformComponent>().translate(glm::vec3(-2.0));

    uint32_t reimuID = scene->addEntity("cube");
    Entity reimuEntity = scene->getEntity(reimuID);
    reimuEntity.addComponent<ModelComponent>("assets/models/reimu/reimu.obj");
    reimuEntity.onModelComponentAdded();
    reimuEntity.getComponent<TransformComponent>().scale(glm::vec3(0.1));

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
