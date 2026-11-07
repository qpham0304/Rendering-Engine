#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_set>
#include "core/features/Object.h"

struct RGHandle {
    uint32_t index;
};

struct RGTextureInfo {
    uint32_t id { 0 };
    uint32_t width { 0 };
    uint32_t height { 0 };
    bool viewable { false };    // the graph leaves it shader readable at the end of the frame, so the viewport can show it
};

struct RGPassInfo {
    std::string name;
    uint32_t readCount { 0 };
    uint32_t writeCount { 0 };
    bool enabled { true };
    const void* settings { nullptr };   // the settings object of the renderer that added it, if it has one
    std::vector<RGTextureInfo> outputs;
};

// api independent side of the graph: the sizes passes render at and which passes run. the backend graph records
// the passes and resources, a disabled pass keeps its layout transitions and only skips its work
class RenderGraph : public Object {
public:
    virtual ~RenderGraph() = default;

    virtual void reset() = 0;
    virtual std::vector<RGPassInfo> getPasses() const = 0;   // the passes recorded for the last frame, in run order

    // passes rebuild on this like on a window resize, the upscaler can change it while the window stays the same
    void setRenderSize(uint32_t width, uint32_t height);
    void setOutputSize(uint32_t width, uint32_t height);
    uint32_t getRenderWidth() const { return m_renderWidth; }
    uint32_t getRenderHeight() const { return m_renderHeight; }
    uint32_t getOutputWidth() const { return m_outputWidth; }
    uint32_t getOutputHeight() const { return m_outputHeight; }

    // by name, the graph is rebuilt every frame so the state outlives the passes
    void setPassEnabled(const std::string& name, bool enabled);
    bool isPassEnabled(const std::string& name) const { return !m_disabledPasses.contains(name); }
    void enableAllPasses() { m_disabledPasses.clear(); }

protected:
    RenderGraph() = default;

    uint32_t m_renderWidth { 0 };
    uint32_t m_renderHeight { 0 };
    uint32_t m_outputWidth { 0 };
    uint32_t m_outputHeight { 0 };
    std::unordered_set<std::string> m_disabledPasses;
};
