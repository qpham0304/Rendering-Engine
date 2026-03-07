#include "Service.h"
#include "logging/framework/LoggerSpd.h"

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