#pragma once

#define GLFW_INCLUDE_NONE
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

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
#include "Camera.h"
//#include "OrbitCamera.h"
#include "../../src/core/features/ServiceLocator.h"
#include "../../src/core/features/PlatformFactory.h"
#include "../../src/core/layers/EditorLayer.h"
#include "../../src/core/layers/LayerManager.h"
#include "../../src/core/scene/SceneManager.h"

//TODO: remove once done
#include "imgui.h"
#include <imgui_impl_vulkan.h>


class RenderDeviceVulkan;

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
    EditorLayer* editorLayer;


private:
    GLFWwindow* windowHandle;

    Camera camera;
    PushConstantData pushConstantData;

    void initVulkan();
    void render();
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
};
