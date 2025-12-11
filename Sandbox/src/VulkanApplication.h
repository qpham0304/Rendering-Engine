#pragma once

#include <iostream>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <limits>
#include <optional>
#include <set>
#include <glm/glm.hpp>
#include <array>
#include <memory>
#include <core/features/Camera.h>
#include <core/features/ServiceLocator.h>
#include <core/features/PlatformFactory.h>
#include <core/layers/EditorLayer.h>
#include <core/layers/LayerManager.h>
#include <core/scene/SceneManager.h>
#include <core/resources/managers/TextureManager.h>
#include <core/resources/managers/MeshManager.h>
#include <core/resources/managers/ModelManager.h>

//TODO: eventually have all these concrete changed to using interface
#include <graphics/framework/Vulkan/Renderers/RendererVulkan.h> 
#include <graphics/framework/Vulkan/core/WrapperStructs.h>
class RenderDeviceVulkan;
class RendererVulkan;

class VulkanApplication : protected VkWrap
{
public:
    VulkanApplication() = default;

    void pushLayer(Layer* layer);
    void init(WindowConfig config);
    void start();
    void run();
    void end();
    void onClose();


private:
    bool showGui = true;
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
    RenderDeviceVulkan* renderDeviceVulkan{ nullptr };
    RendererVulkan vulkanRenderer;
    EditorLayer* editorLayer;
    uint32_t indexBufferID = 0;
    uint32_t vertexBufferID = 0;
    std::unique_ptr<TextureManager> textureManager;
    std::unique_ptr<MeshManager> meshManager;
    std::unique_ptr<ModelManager> modelManager;

private:
    Camera camera;

    uint32_t aruID;
    uint32_t reimuID;
    std::pair<std::vector<uint32_t>, std::vector<uint32_t>> aruMeshIDs;
    std::pair<std::vector<uint32_t>, std::vector<uint32_t>> reimuMeshIDs;

    void render();
    void renderGui(void* commandBuffer);
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
};
