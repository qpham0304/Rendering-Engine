#pragma once

#include "core/resources/managers/Manager.h"
#include "graphics/renderers/Renderer.h"
#include <glm/glm.hpp>

class RendererManager : public Manager
{
public:
	struct StorageBuffer {
		glm::mat4 model;
	};

	virtual ~RendererManager() override = default;

	virtual bool init(WindowConfig config) override = 0;
	virtual bool onClose() override = 0;
	virtual void destroy(uint32_t id) override = 0;
	virtual void onUpdate() override = 0;
	virtual std::vector<uint32_t> listIDs() const override = 0;
    virtual void render() = 0;

	template<typename T> requires std::derived_from<T, Renderer>
	Renderer* addRenderer(std::string_view name) {
		auto it = m_renderers.find(name.data());
		if(it != m_renderers.end()) {
			return it->second.get();
		}

		m_renderers[name.data()] = std::make_shared<T>();
		return m_renderers[name.data()].get();
	}

    virtual Renderer* getRenderer(std::string_view name) = 0;

protected:
    RendererManager(std::string serviceName = "RendererManager") : Manager(serviceName) {};

	std::unordered_map<std::string, std::shared_ptr<Renderer>> m_renderers;
	std::vector<StorageBuffer> instanceData;

};