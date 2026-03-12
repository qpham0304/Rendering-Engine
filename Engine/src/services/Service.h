#pragma once

#include <string>
#include "core/features/Configs.h"
#include "Logging/Logger.h"

class Service
{
public:
	virtual ~Service();

	const std::string& getServiceName() const;

	virtual bool init(WindowConfig config);
	virtual bool onClose();
	virtual void onUpdate() {}

protected:
	Service();
	Service(std::string_view name = "untitled") : m_serviceName(name) {}

protected:
	std::string m_serviceName;
	WindowConfig m_config;
	std::unique_ptr<Logger> m_logger;

};

