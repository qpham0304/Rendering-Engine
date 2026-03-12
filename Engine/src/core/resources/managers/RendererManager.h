#pragma once

#include "core/resources/managers/Manager.h"

class RendererManager : public Manager
{
public:
	virtual ~RendererManager() override = default;

	virtual bool init(WindowConfig config) override = 0;
	virtual bool onClose() override = 0;
	virtual void destroy(uint32_t id) override = 0;
	virtual void onUpdate() override = 0;
	virtual std::vector<uint32_t> listIDs() const override = 0;
    virtual void render() = 0;

    
protected:
    RendererManager(std::string serviceName = "RendererManager") : Manager(serviceName) {};

protected:

};