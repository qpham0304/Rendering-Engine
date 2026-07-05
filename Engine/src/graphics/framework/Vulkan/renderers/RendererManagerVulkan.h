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

	void setRenderMode(uint32_t mode);
	int getRenderMode();
	void beginFrame();
	void endFrame();
	void setDisplayImage(TextureVulkan* image);
	TextureVulkan* getDisplayImage();

protected:
	uint32_t currentRenderMode { 0 };

	RenderDeviceVulkan* renderDeviceVulkan{ nullptr };
	Renderer* applicationRenderer { nullptr };
	Renderer* forwardRenderer { nullptr };
	Renderer* deferredRenderer { nullptr };
	Renderer* raytracingRenderer { nullptr };

	Renderer* shadowMapPass { nullptr };
	Renderer* imageBasedRenderer { nullptr };
	Renderer* ddgiPassRenderer { nullptr };
	Renderer* alchemyAORenderer { nullptr };
	Renderer* hiZPassRenderer { nullptr };
	Renderer* SSRGIPassRenderer { nullptr };
	Renderer* bloomRenderer { nullptr };
	Renderer* temporalPassRenderer { nullptr };
	Renderer* deferredCombineRenderer { nullptr };
	Renderer* postProcessRenderer { nullptr };

	TextureVulkan* displayImage { nullptr };
};