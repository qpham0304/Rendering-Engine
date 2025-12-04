#pragma once

#include <vulkan/vulkan.h>

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
#include "../../src/core/features/Camera.h"
//#include "OrbitCamera.h"
#include "../../src/core/features/ServiceLocator.h"
#include "../../src/core/features/PlatformFactory.h"
#include "../../src/core/layers/EditorLayer.h"
#include "../../src/core/layers/LayerManager.h"
#include "../../src/core/scene/SceneManager.h"
#include "src/graphics/framework/Vulkan/Renderers/RendererVulkan.h" //TODO: use generic interface ptr instead
#include "src/core/resources/managers/TextureManager.h"

class RenderDeviceVulkan;
class RendererVulkan;
//class TextureManager;

class VulkanApplication
{
public:
    VulkanApplication() = default;

    void pushLayer(Layer* layer);
    void init(WindowConfig config);
    void start();
    void run();
    void end();
    void onClose();

    struct PushConstantData {
        alignas(16) glm::vec3 color;
        alignas(16) glm::vec3 range;
        alignas(4)  bool flag;
        alignas(4)  float data;
    };

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
    RenderDeviceVulkan* renderDeviceVulkan{ nullptr };  //TODO: should not be able to use concrete type object remove later
    RendererVulkan vulkanRenderer;
    EditorLayer* editorLayer;
    uint32_t indexBufferID = 0;
    uint32_t vertexBufferID = 0;
    std::unique_ptr<TextureManager> textureManager;
    //TextureManager* textureManager;

private:
    Camera camera;
    PushConstantData pushConstantData;

    void render();
    void renderGui(void* commandBuffer);
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
};
