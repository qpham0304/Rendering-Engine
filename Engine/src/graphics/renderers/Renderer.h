#pragma once

#include <memory>
#include "../RenderDevice.h"

class Renderer
{

public:
	virtual ~Renderer() = default;

	virtual void init() = 0;
	virtual void beginFrame() = 0;
	virtual void endFrame() = 0;
	virtual void render() = 0;
	virtual void shutdown() = 0;
	virtual void addMesh() = 0;
	virtual void addModel(std:: string_view path) = 0;

protected:
	Renderer() = default;

	std::unique_ptr<RenderDevice> renderDevice;
};

