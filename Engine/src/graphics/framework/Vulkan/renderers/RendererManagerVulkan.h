#pragma once

#include "Core/resources/managers/RendererManager.h"
#include "RendererVulkan.h"

class RenderDeviceVulkan;
class TextureVulkan;

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

    virtual RendererVulkan* getRenderer(std::string_view name) override;

	void beginFrame();
	void endFrame();
	void setDisplayImage(TextureVulkan* image);
	TextureVulkan* getDisplayImage();

protected:
	RenderDeviceVulkan* renderDeviceVulkan{ nullptr };
	Renderer* applicationRenderer { nullptr };
	Renderer* forwardRenderer { nullptr };
	Renderer* deferredRenderer { nullptr };
	Renderer* shadowMapRenderer { nullptr };
	Renderer* imageBasedRenderer { nullptr };
	Renderer* postProcessRenderer { nullptr };
	Renderer* alchemyAORenderer { nullptr };
	Renderer* hiZPassRenderer { nullptr };
	Renderer* SSRGIPassRenderer { nullptr };

	TextureVulkan* displayImage { nullptr };
};