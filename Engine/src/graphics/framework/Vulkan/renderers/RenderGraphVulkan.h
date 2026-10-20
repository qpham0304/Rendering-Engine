#pragma once

#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <string>
#include <vector>
#include <string_view>
#include <functional>
#include "graphics/framework/Vulkan/resources/textures/TextureVulkan.h"

struct RGHandle { 
    uint32_t index; 
};

struct RGAccess {
    RGHandle handle;
    VkImageLayout layout;
};

struct RenderPassNode {
    std::string name{"Undefined"};
    std::vector<RGAccess> reads{};  // Dependencies during the pass, use member path for name/type
    std::vector<RGAccess> writes{}; // Outputs during the pass, use member path for name/type
    std::function<void(VkCommandBuffer, uint32_t)> execute;
};


struct RGResource {
    TextureVulkan* texture;
    VkImageLayout  initialLayout;   // layout before the first pass
    VkImageLayout  finalLayout;     // layout after the last pass
};

class RenderGraphVulkan {
public:
    RenderGraphVulkan();
    void addPass(RenderPassNode node);
    // bool compile();
    // const std::vector<std::shared_ptr<RenderPassNode>>& getExecutionOrder() const;
    void execute(VkCommandBuffer cmd, uint32_t currentFrame);
    void reset();
    RGHandle importTexture(
        TextureVulkan* texture,
        VkImageLayout initial = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VkImageLayout final = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );

private:
    // bool dfs(
    //     const std::string& node, 
    //     std::unordered_map<std::string, std::vector<std::string>>& adj,
    //     std::unordered_set<std::string>& visited, 
    //     std::unordered_set<std::string>& visiting
    // );
    std::vector<RGResource> m_resources;
    std::vector<RenderPassNode> m_passes;   // executed in the order they were added
};