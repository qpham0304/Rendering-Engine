#pragma once

#include "Core/resources/managers/RendererManager.h"
#include "RendererVulkan.h"

class RendererManagerVulkan : public RendererManager
{
public:
	RendererManagerVulkan(std::string serviceName = "RendererManagerVulkan");
	virtual ~RendererManagerVulkan() override;

	virtual bool init(WindowConfig config) override;
	virtual bool onClose() override;
	virtual void destroy(uint32_t id) override;
	virtual std::vector<uint32_t> listIDs() const override;
	virtual void onUpdate() override;
    virtual void render() override;

protected:
	std::unordered_map<std::string, std::unique_ptr<Renderer>> m_renderers;
	
};