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

//TODO: remove once done
#include "imgui.h"
#include <imgui_impl_vulkan.h>
#include "../../src/graphics/framework/Vulkan/RenderDeviceVulkan.h"	//TODO: remove conrete type access dependency


class Demo
{
public:
    Demo() = default;

    void pushLayer(Layer* layer);
    void init();
    void start();
    void run();
    void end();
    void onClose();

private:
    bool isRunning = true;
    WindowConfig windowConfig;
    ServiceLocator serviceLocator;
    PlatformFactory platformFactory{ serviceLocator };

    std::unique_ptr<AppWindow> appWindow;
    std::unique_ptr<GuiManager> guiManager;
    std::unique_ptr<RenderDevice> renderDevice;
    std::unique_ptr<Logger> engineLogger;
    std::unique_ptr<Logger> clientLogger;
    RenderDeviceVulkan* renderDeviceVulkan{ nullptr };  //TODO: should not be able to use concrete type object remove later
    EditorLayer* editorLayer;


private:
    GLFWwindow* windowHandle;

    Camera camera;
    RenderDeviceVulkan::PushConstantData pushConstantData;

    void initVulkan();
    void mainLoop();
    void cleanup();
    void drawFrame();

    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

    VkDescriptorPool guiDescriptorPool;
    void createGuiDescriptorPool();
    void destroyGuiDescriptorPool();
    void initGuiContext();
    void renderGui(VkCommandBuffer commandBuffer);
};
