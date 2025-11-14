#include "RenderDeviceVulkan.h"
#include "../../src/core/features/ServiceLocator.h"
#include "../../src/Logging/Logger.h"

RenderDeviceVulkan::RenderDeviceVulkan()
	: RenderDevice("RenderDeviceVulkan"),
	swapchain(device),
	VulkanFrameBuffer(),
	pipeline(device)
{

}

int RenderDeviceVulkan::init(WindowConfig config)
{
	Service::init(config);

	m_logger = &ServiceLocator::GetService<Logger>("Engine_LoggerPSD");
	m_logger->info("Vulkan Render Device initialized");

	return 0;
}

int RenderDeviceVulkan::onClose()
{
	throw std::runtime_error("beginFrame not implemented");
}

void RenderDeviceVulkan::onUpdate()
{
	throw std::runtime_error("beginFrame not implemented");
}

void RenderDeviceVulkan::beginFrame()
{
	throw std::runtime_error("beginFrame not implemented");
}

void RenderDeviceVulkan::endFrame()
{
	throw std::runtime_error("beginFrame not implemented");
}

void RenderDeviceVulkan::render()
{

}

Logger& RenderDeviceVulkan::Log() const
{
	return *m_logger;
}