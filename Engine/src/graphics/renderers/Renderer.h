#pragma once

#include <memory>
#include "RenderDevice.h"
#include "services/Service.h"

class Camera;
class Scene;

class Renderer : public Service
{

public:
	virtual ~Renderer() = default;

	Renderer(const Renderer& other) = delete;
	Renderer(const Renderer&& other) = delete;
	Renderer& operator=(const Renderer& other) = delete;
	Renderer& operator=(const Renderer&& other) = delete;

	virtual bool init(WindowConfig config) = 0;
	virtual bool onClose() = 0;
	virtual void onUpdate() = 0;
	virtual void beginFrame() = 0;
	virtual void endFrame() = 0;
	virtual void render(Camera& camera) = 0;

protected:
	Renderer(std::string name = "Renderer") : Service(name) {};

	std::unique_ptr<RenderDevice> renderDevice;
};

