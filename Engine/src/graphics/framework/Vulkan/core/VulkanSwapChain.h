#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include "VulkanDevice.h"

class RenderDeviceVulkan;

class VulkanSwapChain
{
public:
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

    VkSwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    std::vector<VkImageView> swapChainImageViews;
    std::vector<VkFramebuffer> swapChainFramebuffers;
    VkRenderPass renderPass;
    bool framebufferResized = false;

    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;

public:

    VulkanSwapChain(VulkanDevice& deviceRef, RenderDeviceVulkan& renderDeviceRef);
    ~VulkanSwapChain();

    void create();
    void destroy();

    void createSyncObject();
    void createFramebuffers();
    void recreateSwapchain();

    void accquireNextImage(const uint32_t& currentFrame);
    void submitAndPresent(const uint32_t& currentFrame, const VkCommandBuffer& commandBuffer);
    virtual const uint32_t& getImageIndex() const;

private:
    VulkanSwapChain(const VulkanSwapChain& other) = delete;
    VulkanSwapChain& operator=(const VulkanSwapChain& other) = delete;
    VulkanSwapChain(const VulkanSwapChain&& other) = delete;
    VulkanSwapChain& operator=(const VulkanSwapChain&& other) = delete;

    void createSwapChain();
    void createImageViews();
    void createRenderPass();
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
    void cleanupSwapChain();

private:
    VulkanDevice& device;
    RenderDeviceVulkan& renderDevice;
    uint32_t imageIndex = 0;
};

