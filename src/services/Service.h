#pragma once

#include <string>
#include "../../src/core/features/Configs.h"

class Service
{
public:
	virtual ~Service() = default;

	const std::string& getServiceName() const;

	virtual int init(WindowConfig platform) = 0;
	virtual int onClose() = 0;
	virtual void onUpdate() = 0;

protected:
	Service() = default;
	Service(std::string_view name = "untitled") : serviceName(name) {}

protected:
	std::string serviceName;

};

