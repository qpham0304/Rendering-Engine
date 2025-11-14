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

//TODO: remove once done
#include "imgui.h"
#include <imgui_impl_vulkan.h>
#include "../../src/graphics/framework/Vulkan/RenderDeviceVulkan.h"	//TODO: remove conrete type access dependency


class Demo
{
public:

public:

private:
    bool isRunning = true;
    ServiceLocator serviceLocator;
    PlatformFactory platformFactory{ serviceLocator };
    std::unique_ptr<AppWindow> appWindow;
    std::unique_ptr<GuiManager> guiManager;
    std::unique_ptr<RenderDevice> renderDevice;
    RenderDeviceVulkan* renderDeviceVulkan{ nullptr };  //TODO: should not be able to use concrete type object remove later

public:
    Demo() = default;
    void run();

    bool framebufferResized = false;


private:
    static const int MAX_FRAMES_IN_FLIGHT = 2;

    GLFWwindow* windowHandle;
    VkDevice device;

    VkSwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    std::vector<VkImageView> swapChainImageViews;
    //std::vector<VkFramebuffer> swapChainFramebuffers;
    VkRenderPass renderPass;


    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorPool descriptorPool;
    std::vector<VkDescriptorSet> descriptorSets;


    VkCommandPool commandPool;
    std::vector<VkCommandBuffer> commandBuffers;

    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;

    uint32_t currentFrame = 0;

    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;

    VkBuffer combinedBuffer;
    VkDeviceMemory combinedBufferMemory;

    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;
    std::vector<void*> uniformBuffersMapped;


    Camera camera;
    //OrbitCamera orbitCamera;
    RenderDeviceVulkan::PushConstantData pushConstantData;

    void initVulkan();
    void mainLoop();
    void cleanup();

    void createCommandPool();
    void createVertexBuffer();
    void createIndexBuffer();
    void createCombinedBuffer();
    void createCommandBuffers();
    void createSyncObject();
    void createDescriptorSetLayout();
    void createUniformBuffers();
    void updateUniformBuffer(uint32_t currentImage);
    void createDescriptorPool();
    void createDescriptorSets();
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void drawFrame();

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    void createBuffer(
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkBuffer& buffer,
        VkDeviceMemory& bufferMemory
    );
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);


    VkDescriptorPool guiDescriptorPool;

    void createGuiDescriptorPool();
    void destroyGuiDescriptorPool();
    void initGuiContext();
    void renderGui(VkCommandBuffer commandBuffer);
};
