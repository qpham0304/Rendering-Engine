#include "RenderGraph.h"
#include "core/events/EventManager.h"

void RenderGraph::setRenderSize(uint32_t width, uint32_t height)
{
    bool changed = m_renderWidth != 0 && (m_renderWidth != width || m_renderHeight != height);
    m_renderWidth = width;
    m_renderHeight = height;

    if(changed) {
        RenderResizeEvent event(width, height);
        EventManager::getInstance().publish(event);
    }
}

void RenderGraph::setOutputSize(uint32_t width, uint32_t height)
{
    m_outputWidth = width;
    m_outputHeight = height;
}

void RenderGraph::setPassEnabled(const std::string& name, bool enabled)
{
    if(enabled) {
        m_disabledPasses.erase(name);
    } else {
        m_disabledPasses.insert(name);
    }
}
