#pragma once

#include <core/features/Camera.h>
#include <core/features/ServiceLocator.h>
#include <core/features/PlatformFactory.h>
#include <core/layers/EditorLayer.h>
#include <core/layers/LayerManager.h>
#include <core/scene/SceneManager.h>
#include <core/resources/managers/TextureManager.h>
#include <core/resources/managers/BufferManager.h>
#include <core/resources/managers/MeshManager.h>
#include <core/resources/managers/ModelManager.h>
#include <core/resources/managers/DescriptorManager.h>
#include <core/resources/managers/MaterialManager.h>
#include <graphics/renderers/Renderer.h>
#include <services/Service.h>

class Sandbox
{
public:
    Sandbox(WindowConfig config);

    void pushLayer(Layer* layer);
    void init();
    void start();
    void run();
    void close();


private:
    bool showGui = false;
    bool isRunning = true;
    WindowConfig windowConfig;
    ServiceLocator serviceLocator;
    PlatformFactory platformFactory{ serviceLocator };

    SceneManager& sceneManager = SceneManager::getInstance();
    EventManager& eventManager = EventManager::getInstance();
    std::unique_ptr<AppWindow> appWindow;
    std::unique_ptr<LayerManager> layerManager;
    std::unique_ptr<GuiManager> guiManager;
    std::unique_ptr<RenderDevice> renderDevice;
    std::unique_ptr<Logger> engineLogger;
    std::unique_ptr<Logger> clientLogger;
    std::unique_ptr<TextureManager> textureManager;
    std::unique_ptr<BufferManager> bufferManager;
    std::unique_ptr<MeshManager> meshManager;
    std::unique_ptr<ModelManager> modelManager;
    std::unique_ptr<DescriptorManager> descriptorManager;
    std::unique_ptr<MaterialManager> materialManager;
    std::unique_ptr<Renderer> renderer;

private:
    std::vector<Service*> services;

    uint32_t aruID;
    uint32_t reimuID;
    std::pair<std::vector<uint32_t>, std::vector<uint32_t>> aruMeshIDs;
    std::pair<std::vector<uint32_t>, std::vector<uint32_t>> reimuMeshIDs;
};
