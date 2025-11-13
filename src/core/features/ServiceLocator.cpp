#include "ServiceLocator.h"
#include "../../logging/Logger.h"

ServiceLocator* ServiceLocator::instance = nullptr;


void ServiceLocator::supportingServices()
{
	instance->listServices();
}


void ServiceLocator::setContext(ServiceLocator* other)
{
	instance = other;
}


void ServiceLocator::listServices()
{
	Logger& logger = Get<Logger>("Engine_LoggerPSD");
	for (const auto& [name, service] : services) {
		logger.info("Service: [{}, Adress: {}]", name, service);
	}
}


bool ServiceLocator::hasService(std::string_view serviceName) const
{
	if (services.find(serviceName.data()) == services.end()) {
		return false;
	}
	return true;
}
