#pragma once

#include "services/Service.h"
#include "core/features/Object.h"

class Camera;
class Scene;

class Renderer : public Object
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
	virtual void render(Camera& camera) = 0;

protected:
	Renderer(std::string name = "Renderer") : m_name(name) { setFootprint("Renderer", sizeof(Renderer), MemorySystem::Graphics); };

	std::string m_name;
};

