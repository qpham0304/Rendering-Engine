#pragma once

#include "../../../src/services/Service.h"

class RenderDevice : public Service
{
public:


public:
	virtual ~RenderDevice() = default;

	RenderDevice(const RenderDevice& other) = delete;
	RenderDevice(const RenderDevice&& other) = delete;
	RenderDevice& operator=(const RenderDevice& other) = delete;
	RenderDevice& operator=(const RenderDevice&& other) = delete;

	virtual int init(WindowConfig platform) override = 0;
	virtual int onClose() override = 0;
	virtual void onUpdate() override = 0;
	virtual void beginFrame() = 0;
	virtual void endFrame() = 0;


protected:
	RenderDevice(std::string name = "RenderDevice") : Service(name) {};
	

private:
};

