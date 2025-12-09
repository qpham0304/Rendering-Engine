#pragma once

#include "src/graphics/renderers/Renderer.h"

class RenderDeviceVulkan;
class RendererVulkan : public Renderer
{
private:


public:
	RendererVulkan();
	virtual ~RendererVulkan() override;

	virtual void init() override;
	virtual void beginFrame() override;
	virtual void endFrame() override;
	virtual void render() override;
	virtual void shutdown() override;

	void beginRecording(void* cmdBuffer);
	void endRecording(void* cmdBuffer);

protected:
	RenderDeviceVulkan* renderDeviceVulkan{ nullptr };

};

