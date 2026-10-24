#include "Service.h"
#include "logging/framework/LoggerSpd.h"
#include "Core/events//EventManager.h"

Service::Service()
{
	setFootprint("Service", sizeof(Service), MemorySystem::Core);
	
}

Service::Service(std::string_view name) : m_serviceName(name)
{
	setFootprint("Service", sizeof(Service), MemorySystem::Core);
	
}

Service::~Service()
{

}

bool Service::init(WindowConfig config)
{
	m_config = config;
			
	m_logger = make_unique<LoggerSpd>(m_serviceName);
	m_logger->setLevel(LogLevel::Warn);
	
	m_eventSubscriptions.push_back(EventManager::getInstance().subscribe(EventType::WindowResize, [this] (Event& event) {
		WindowResizeEvent& resizeEvent = static_cast<WindowResizeEvent&>(event);
		m_config.width = resizeEvent.m_width;
		m_config.height = resizeEvent.m_height;
	}));
	
	return true;
}

bool Service::onClose()
{
    return true;
}

void Service::onFixedUpdate(float fixedDeltaTime)
{

}

void Service::onUpdate(const FrameTime& frameTime)
{

}

const std::string& Service::getServiceName() const
{
	return m_serviceName;
}