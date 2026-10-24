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
class DepthPrepassVulkan;
class GeometryPassVulkan;
class LightingPassVulkan;
class RayTraceRendererVulkan;
class ShadowMapPassVulkan;
class ImageBasedRendererVulkan;
class SkyAtmosphereVulkan;
class DDGIBuilderVulkan;
class AmbientOcclusionPassVulkan;
class HiZPassVulkan;
class SSRGIPassVulkan;
class BloomPassVulkan;
class TemporalPassVulkan;
class DeferredCombinePassVulkan;
class PostProcessRendererVulkan;
class ParticleRendererVulkan;
class RibbonRendererVulkan;
class DebugRendererVulkan;

class MeshManager;
class ModelManager;
class BufferManagerVulkan;
class MaterialManagerVulkan;

class RendererManagerVulkan : public RendererManager
{
public:
	struct MeshDraw {
		glm::mat4 model;
		uint32_t meshID;
		uint32_t objectIdx;
	};

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
	RenderGraphVulkan& getRenderGraph() { return m_renderGraph; }
	const Common::UniformBufferObject& getUBO() const;
	const std::vector<UniformBufferVulkan*>& getBufferUBO() const;

	//mostly for ray tracing only
	void collectMeshDraws();
	const std::vector<MeshDraw>& getMeshDraws() const { return m_meshDraws; }

	void drawObjects(VkCommandBuffer cmd);
	uint64_t getObjectsAddress() const;
	uint64_t getFrameNumber() const { return m_frameNumber; }
	void collectLights();
	uint64_t getLightsAddress() const;
	uint32_t getLightCount() const { return static_cast<uint32_t>(m_lights.size()); }


protected:
	uint32_t currentRenderMode { 1 };
	TextureVulkan* displayImage { nullptr };

	std::vector<UniformBufferVulkan*> uniformbuffersList{};
	Common::UniformBufferObject ubo {};
	glm::mat4 lastViewProj{};
	bool firstFrame { true };

	RenderDeviceVulkan* renderDeviceVulkan{ nullptr };
	MeshManager* meshManager { nullptr };
	ModelManager* modelManager { nullptr };
	BufferManagerVulkan* bufferManagerVulkan { nullptr };
	MaterialManagerVulkan* materialManagerVulkan { nullptr };
	std::vector<MeshDraw> m_meshDraws;
	std::vector<Common::Object> m_objects;
	std::vector<uint32_t> m_objectsBufferIDs;
	std::vector<uint64_t> m_objectsAddresses;
	std::vector<glm::mat4> m_prevModels;
	std::vector<VkDrawIndirectCommand> m_drawCommands;
	std::vector<uint32_t> m_drawCommandsBufferIDs;
	std::vector<Common::PointLight> m_lights;
	std::vector<uint32_t> m_lightsBufferIDs;
	std::vector<uint64_t> m_lightsAddresses;

	ApplicationRendererVulkan* applicationRenderer { nullptr };
	ForwardRendererVulkan* forwardRenderer { nullptr };
	DeferredRendererVulkan* deferredRenderer { nullptr };
	DepthPrepassVulkan* depthPrepassRenderer { nullptr };
	GeometryPassVulkan* geometryPassRenderer { nullptr };
	LightingPassVulkan* lightingPassRenderer { nullptr };
	RayTraceRendererVulkan* raytracingRenderer { nullptr };

	ShadowMapPassVulkan* shadowMapPass { nullptr };
	SkyAtmosphereVulkan* skyAtmosphereRenderer { nullptr };
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
	RibbonRendererVulkan* ribbonRenderer { nullptr };
	DebugRendererVulkan* debugRenderer { nullptr };

private:
	RenderGraphVulkan m_renderGraph;
	bool needResize{ false };
	uint64_t m_frameNumber { 0 };
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