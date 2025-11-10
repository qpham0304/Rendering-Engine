#include "ServiceLocator.h"
#include "../../logging/Logger.h"

void ServiceLocator::ListServices()
{
	Logger& logger = Get<Logger>("Engine_LoggerPSD");
	for (const auto& [name, service] : services) {
		logger.info("Service: {} at {}", name, (void*)service);
	}
}

bool ServiceLocator::hasService(std::string_view serviceName) const
{
	if (services.find(serviceName.data()) == services.end()) {
		return false;
	}
	return true;
}
