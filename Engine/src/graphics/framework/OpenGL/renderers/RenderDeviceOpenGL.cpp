#include "RenderDeviceOpenGL.h"
#include "core/features/ServiceLocator.h"
#include "Logging/Logger.h"
#include <stdexcept>

RenderDeviceOpenGL::RenderDeviceOpenGL()
	: RenderDevice("RenderDeviceOpenGL")
{

}

int RenderDeviceOpenGL::init(WindowConfig config)
{
	Service::init(config);

	m_logger = &ServiceLocator::GetService<Logger>("Client_LoggerPSD");
	Log().info("[{}] {}", m_ServiceName, "Render Device initialized");

	return 0;
}

int RenderDeviceOpenGL::onClose()
{
	throw std::runtime_error("RenderDeviceOpenGL beginFrame not implemented");
}

void RenderDeviceOpenGL::onUpdate()
{
	throw std::runtime_error("RenderDeviceOpenGL beginFrame not implemented");
}

void RenderDeviceOpenGL::beginFrame()
{
	throw std::runtime_error("RenderDeviceOpenGL beginFrame not implemented");
}

void RenderDeviceOpenGL::endFrame()
{
	throw std::runtime_error("RenderDeviceOpenGL beginFrame not implemented");
}

void RenderDeviceOpenGL::render()
{
	throw std::runtime_error("RenderDeviceOpenGL render not implemented");
}

void RenderDeviceOpenGL::draw(uint32_t numIndicies, uint32_t numInstances, uint32_t offset)
{
	throw std::runtime_error("RenderDeviceOpenGL draw not implemented");
}

Logger& RenderDeviceOpenGL::Log() const
{
	return *m_logger;
}