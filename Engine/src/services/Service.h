#pragma once

#include <string>
#include "core/features/Configs.h"

class Service
{
public:
	virtual ~Service() = default;

	const std::string& getServiceName() const;

	virtual int init(WindowConfig config);
	virtual int onClose() { return 0; }
	virtual void onUpdate() {}

protected:
	Service() = default;
	Service(std::string_view name = "untitled") : m_ServiceName(name) {}

protected:
	std::string m_ServiceName;
	WindowConfig m_config;
};

