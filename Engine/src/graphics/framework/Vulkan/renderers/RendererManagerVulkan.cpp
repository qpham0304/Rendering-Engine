#include "RendererManagerVulkan.h"
#include "RendererManagerVulkan.h"


RendererManagerVulkan::RendererManagerVulkan(std::string serviceName)
    : RendererManager(serviceName) 
{
}

RendererManagerVulkan::~RendererManagerVulkan()
{
}

bool RendererManagerVulkan::init(WindowConfig config)
{
    Service::init(config);

    return true;
}

bool RendererManagerVulkan::onClose()
{
    Service::onClose();

    return true;
}

void RendererManagerVulkan::destroy(uint32_t id)
{

}

std::vector<uint32_t> RendererManagerVulkan::listIDs() const
{
    return std::vector<uint32_t>();
}

void RendererManagerVulkan::onUpdate()
{
    // m_logger->warn("updating...");
}

void RendererManagerVulkan::render()
{
    // m_logger->warn("rendering...");
}
