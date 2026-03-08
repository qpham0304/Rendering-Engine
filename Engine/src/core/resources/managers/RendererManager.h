#pragma once

#include "core/resources/managers/Manager.h"

class RendererManager : public Manager
{
public:
	virtual ~RendererManager() override = default;

	virtual bool init(WindowConfig config) = 0;
	virtual bool onClose() = 0;
	virtual void destroy(uint32_t id) = 0;
	virtual void onUpdate() override = 0;
	virtual std::vector<uint32_t> listIDs() const = 0;
    virtual void render() = 0;

    
protected:
    RendererManager(std::string serviceName = "RendererManager") : Manager(serviceName) {};

protected:

};