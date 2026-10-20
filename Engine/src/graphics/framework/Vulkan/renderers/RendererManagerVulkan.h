#pragma once

#include "Core/resources/managers/RendererManager.h"
#include "graphics/framework/vulkan/core/CommonStructs.h"
#include "RendererVulkan.h"
#include "graphics/framework/vulkan/renderers/RenderGraphVulkan.h"

class RenderDeviceVulkan;
class TextureVulkan;
class ApplicationRendererVulkan;
class ForwardRendererVulkan;
class DeferredRendererVulkan;
class GeometryPassVulkan;
class LightingPassVulkan;
class RayTraceRendererVulkan;
class ShadowMapPassVulkan;
class ImageBasedRendererVulkan;
class DDGIBuilderVulkan;
class AmbientOcclusionPassVulkan;
class HiZPassVulkan;
class SSRGIPassVulkan;
class BloomPassVulkan;
class TemporalPassVulkan;
class DeferredCombinePassVulkan;
class PostProcessRendererVulkan;
class ParticleRendererVulkan;
class DebugRendererVulkan;

class RendererManagerVulkan : public RendererManager
{
public:
	RendererManagerVulkan(std::string serviceName = "RendererManagerVulkan");
	virtual ~RendererManagerVulkan() override;

	virtual std::vector<uint32_t> listIDs() const override;
    virtual RendererVulkan* getRenderer(std::string_view name) override;

	void setRenderMode(uint32_t mode);
	int getRenderMode();
	void beginFrame();
	void endFrame();
	void setDisplayImage(TextureVulkan* image);
	TextureVulkan* getDisplayImage();
	const Common::UniformBufferObject& getUBO() const;
	const std::vector<UniformBufferVulkan*>& getBufferUBO() const;


protected:
	uint32_t currentRenderMode { 1 };
	TextureVulkan* displayImage { nullptr };

	std::vector<UniformBufferVulkan*> uniformbuffersList{};
	Common::UniformBufferObject ubo {};
	glm::mat4 lastViewProj{};
	bool firstFrame { true };
	
	//Common::PushBDA pushBDA;
	//uint32_t objDeviceAddressBufferID;
	//uint64_t objDeviceAddress;
	//uint64_t materialsAddress;
	//std::vector<Common::ObjectRefs> objects;

	RenderDeviceVulkan* renderDeviceVulkan{ nullptr };
	ApplicationRendererVulkan* applicationRenderer { nullptr };
	ForwardRendererVulkan* forwardRenderer { nullptr };
	DeferredRendererVulkan* deferredRenderer { nullptr };
	GeometryPassVulkan* geometryPassRenderer { nullptr };
	LightingPassVulkan* lightingPassRenderer { nullptr };
	RayTraceRendererVulkan* raytracingRenderer { nullptr };

	ShadowMapPassVulkan* shadowMapPass { nullptr };
	ImageBasedRendererVulkan* imageBasedRenderer { nullptr };
	DDGIBuilderVulkan* ddgiPassRenderer { nullptr };
	AmbientOcclusionPassVulkan* alchemyAORenderer { nullptr };
	HiZPassVulkan* hiZPassRenderer { nullptr };
	SSRGIPassVulkan* SSRGIPassRenderer { nullptr };
	BloomPassVulkan* bloomRenderer { nullptr };
	TemporalPassVulkan* temporalPassRenderer { nullptr };
	DeferredCombinePassVulkan* deferredCombineRenderer { nullptr };
	PostProcessRendererVulkan* postProcessRenderer { nullptr };
	ParticleRendererVulkan* particleRenderer { nullptr };
	DebugRendererVulkan* debugRenderer { nullptr };

private:
	RenderGraphVulkan m_renderGraph;
	bool needResize{ false };
	bool swapchainOutdated{ false };

	//friend class renderer vulkan should not be able to call these
	virtual bool init(WindowConfig config) override;
	virtual bool onClose() override;
	virtual void destroy(uint32_t id) override;
	virtual void onUpdate(const FrameTime& frameTime) override;
    virtual void render() override;
	virtual void drawDebug(glm::vec3 start, glm::vec3 end) override;
	
	virtual void _cleanupResources();
	virtual void _recreateResources();

};