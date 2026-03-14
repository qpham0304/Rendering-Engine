#include "Service.h"
#include "logging/framework/LoggerSpd.h"
#include "Core/events//EventManager.h"

Service::Service()
{
	
}

Service::~Service()
{

}

bool Service::init(WindowConfig config)
{
	m_config = config;
			
	m_logger = make_unique<LoggerSpd>(m_serviceName);
	m_logger->setLevel(LogLevel::Warn);
	
	EventManager::getInstance().subscribe(EventType::WindowResize, [this] (Event& event) {
		WindowResizeEvent& resizeEvent = static_cast<WindowResizeEvent&>(event);
		m_config.width = resizeEvent.m_width;
		m_config.height = resizeEvent.m_height;
	});
	
	return true;
}

bool Service::onClose()
{
    return true;
}

const std::string& Service::getServiceName() const
{
	return m_serviceName;
}