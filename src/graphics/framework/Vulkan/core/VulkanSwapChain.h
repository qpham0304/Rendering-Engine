#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include "VulkanDevice.h"
//struct SwapChainSupportDetails {
//    VkSurfaceCapabilitiesKHR capabilities;
//    std::vector<VkSurfaceFormatKHR> formats;
//    std::vector<VkPresentModeKHR> presentModes;
//};

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


    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;

public:

    VulkanSwapChain(VulkanDevice& deviceRef);
    ~VulkanSwapChain();

    void create();
    void destroy();

    //SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
    void createFramebuffers();
    void recreateSwapchain();

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

private:
    VulkanDevice& device;

};

