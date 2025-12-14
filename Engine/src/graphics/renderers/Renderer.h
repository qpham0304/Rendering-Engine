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

	virtual int init(WindowConfig config) = 0;
	virtual int onClose() = 0;
	virtual void onUpdate() = 0;
	virtual void beginFrame() = 0;
	virtual void endFrame() = 0;
	virtual void render(Camera& camera, Scene* scene) = 0;
	virtual void addMesh() = 0;
	virtual void addModel(std:: string_view path) = 0;

protected:
	Renderer(std::string name = "RenderDevice") : Service(name) {};

	std::unique_ptr<RenderDevice> renderDevice;
};

