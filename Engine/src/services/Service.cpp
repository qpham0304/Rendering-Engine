#include "Service.h"

bool Service::init(WindowConfig config)
{
	m_config = config;
	
	return true;
}

const std::string& Service::getServiceName() const
{
	return m_ServiceName;
}