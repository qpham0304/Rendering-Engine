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
    std::vector<RGAccess> reads{};  // Dependencies, use member path for name/type
    std::vector<RGAccess> writes{}; // Outputs, use member path  for name/type
    std::function<void(VkCommandBuffer, uint32_t)> execute;
};


struct RGResource {
    TextureVulkan* texture;
    VkImageLayout  initialLayout;   // for external textures
    VkImageLayout  finalLayout;
};

class RenderGraphVulkan {
public:
    RenderGraphVulkan();
    void addPass(RenderPassNode node);
    // bool compile();
    // const std::vector<std::shared_ptr<RenderPassNode>>& getExecutionOrder() const;
    void execute(VkCommandBuffer cmd, uint32_t currentFrame);
    void reset();
    RGHandle importTexture(TextureVulkan* texture, VkImageLayout initial, VkImageLayout final);


private:
    // bool dfs(
    //     const std::string& node, 
    //     std::unordered_map<std::string, std::vector<std::string>>& adj,
    //     std::unordered_set<std::string>& visited, 
    //     std::unordered_set<std::string>& visiting
    // );
    std::vector<RGResource> m_resources;
    std::unordered_map<std::string, std::shared_ptr<RenderPassNode>> m_passes;
    std::vector<std::shared_ptr<RenderPassNode>> m_sortedPasses;
};