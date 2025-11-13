#pragma once
class VulkanSwapChain
{
public:
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

    VulkanSwapChain();
    ~VulkanSwapChain();

private:
    VulkanSwapChain(const VulkanSwapChain& other) = delete;
    VulkanSwapChain& operator=(const VulkanSwapChain& other) = delete;
    VulkanSwapChain(const VulkanSwapChain&& other) = delete;
    VulkanSwapChain& operator=(const VulkanSwapChain&& other) = delete;
};

