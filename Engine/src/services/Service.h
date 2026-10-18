#pragma once

#include <string>
#include "core/features/Configs.h"
#include "Logging/Logger.h"
#include "core/features/FrameTime.h"

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
	virtual void onFixedUpdate(float fixedDeltaTime);	//on fixed update simulation, i.e: frame update 144 while fixed update 60
	virtual void onUpdate(const FrameTime& frameTime);	//on frame update, blame unity for confusing name
	virtual bool onClose();

	std::string m_serviceName;
	WindowConfig m_config;
	std::unique_ptr<Logger> m_logger;

private:
	friend class Engine;


};

