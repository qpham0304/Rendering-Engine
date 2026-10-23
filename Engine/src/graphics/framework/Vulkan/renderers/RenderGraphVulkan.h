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
    std::vector<RGAccess> reads{};  // Dependencies during the pass
    std::vector<RGAccess> writes{}; // Outputs during the pass
    std::function<void(VkCommandBuffer, uint32_t)> execute;
};


struct RGResource {
    TextureVulkan* texture;
    VkImageLayout initialLayout;    // layout before the first pass
    VkImageLayout finalLayout;      // layout after the last pass
    bool lastWasWrite { false };    // for write to write of the same layout
};

// user specified config.width and config.height will be fixed size and wont be resized automatically by the graph
struct RGTextureDesc {
    TextureConfig config{};
    TextureSamplerConfig sampler{};
    float scale { 1.0f };   // 0.5 = half the render size, ignored for fixed size
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

    void setRenderSize(uint32_t width, uint32_t height);

    // graph owned texture: created on first use, recreated when the size or desc changes, and already imported
    // for this frame (it ends every frame in SHADER_READ_ONLY, and starts from UNDEFINED when config.initialLayout asks for it)
    RGHandle createRenderTexture(const std::string& name, const RGTextureDesc& desc);
    TextureVulkan* getTexture(RGHandle handle);

private:
    // bool _dfs(
    //     const std::string& node, 
    //     std::unordered_map<std::string, std::vector<std::string>>& adj,
    //     std::unordered_set<std::string>& visited, 
    //     std::unordered_set<std::string>& visiting
    // );
    TextureVulkan* _createTexture(const std::string& name, const RGTextureDesc& desc);
    void _prepareAccess(VkCommandBuffer cmd, const RGAccess& access, bool isWrite);

    struct RenderTexture {
        RGTextureDesc desc;
        uint32_t width { 0 };
        uint32_t height { 0 };
        TextureVulkan* texture { nullptr };
    };

    std::vector<RGResource> m_resources;
    std::vector<RenderPassNode> m_passes;   // atm are executed in the order they were added

    std::unordered_map<std::string, RenderTexture> m_ownedTextures;   // do not clear by reset
    uint32_t m_renderWidth { 0 };
    uint32_t m_renderHeight { 0 };
};