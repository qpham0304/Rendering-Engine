#include "Service.h"

int Service::init(WindowConfig config)
{
	m_config = config;
	
	return 0;
}

const std::string& Service::getServiceName() const
{
	return m_ServiceName;
}