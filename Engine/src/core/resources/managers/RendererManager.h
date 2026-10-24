#pragma once

#include "core/resources/managers/Manager.h"
#include "graphics/renderers/Renderer.h"
#include "core/features/Texture.h"
#include "graphics/utils/GraphicsSettingsSerializer.h"
#include <glm/glm.hpp>

class MYGRAPHICSENGINE_EXPORT RendererManager : public Manager
{
public:

	virtual ~RendererManager() override = default;

	virtual bool init(WindowConfig config) override = 0;
	virtual bool onClose() override = 0;
	virtual void destroy(uint32_t id) override = 0;
	virtual void onUpdate(const FrameTime& frameTime) override = 0;
	virtual std::vector<uint32_t> listIDs() const override = 0;
    virtual void render() = 0;
	virtual void drawDebug(glm::vec3 start, glm::vec3 end) = 0;

	template<typename T> requires std::derived_from<T, Renderer>
	T* addRenderer(std::string_view name) {
		for(auto& tuple : m_renderers) {
			if(name.data() == std::get<0>(tuple)) {
				return dynamic_cast<T*>(std::get<1>(tuple).get());
			}
		}

		auto renderer = std::make_shared<T>();
		T* rendererPtr = renderer.get();
		m_renderers.push_back(std::tuple(name.data(), std::move(renderer)));
		return rendererPtr;
	}

    virtual Renderer* getRenderer(std::string_view name) = 0;

	GraphicsSettingsSerializer& getGraphicsSettings() { return m_graphicsSettings; }

protected:
	friend class Engine;
	
    RendererManager(std::string serviceName = "RendererManager") : Manager(serviceName) { setFootprint("RendererManager", sizeof(RendererManager), MemorySystem::Resource); };

	std::vector<std::tuple<std::string, std::shared_ptr<Renderer>>> m_renderers;
	GraphicsSettingsSerializer m_graphicsSettings;

};