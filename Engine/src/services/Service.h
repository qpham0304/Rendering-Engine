#pragma once

#include <string>
#include "core/features/Configs.h"
#include "Logging/Logger.h"

class Engine;
class Service
{
public:
	virtual ~Service();

	const std::string& getServiceName() const;

protected:
	Service();
	Service(std::string_view name = "untitled");
	
	virtual bool init(WindowConfig config);
	virtual void onUpdate();
	virtual bool onClose();

	std::string m_serviceName;
	WindowConfig m_config;
	std::unique_ptr<Logger> m_logger;

private:
	friend class Engine;


};

