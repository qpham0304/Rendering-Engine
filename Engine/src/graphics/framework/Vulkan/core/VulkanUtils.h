#pragma once

#include <unordered_map>
#include <string>
#include <stdexcept>
#include <functional>
#include <memory>
#include <vulkan/vulkan.h>

namespace VulkanUtils
{
    uint32_t findMemoryType(
        VkPhysicalDevice physicalDevice, 
        uint32_t typeFilter, 
        VkMemoryPropertyFlags properties
    );


};

