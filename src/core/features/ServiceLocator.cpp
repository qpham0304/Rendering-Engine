#include "ServiceLocator.h"


bool ServiceLocator::hasService(std::string_view serviceName) const
{
	if (services.find(serviceName.data()) == services.end()) {
		return false;
	}
	return true;
}
