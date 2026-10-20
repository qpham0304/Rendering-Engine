#pragma once

#include "graphics/framework/vulkan/renderers/RendererVulkan.h"
#include "graphics/framework/vulkan/core/CommonStructs.h"

#include <glm/glm.hpp>
#include <vector>

// writes the g-buffer (position, normal, albedo, pbr, emissive, motion, depth) with dynamic rendering
class GeometryPassVulkan : public RendererVulkan
{
public:
	struct StorageBufferObject {
		glm::mat4 model;
	};

	// single copy, gpu only images, the frame boundary barrier protects them instead of per frame copies
	struct RenderTarget {
		uint32_t width { 0 };
		uint32_t height { 0 };
		VkFormat depthFormat { VK_FORMAT_UNDEFINED };
		TextureVulkan* gBufferPos { nullptr };
		TextureVulkan* gBufferNorm { nullptr };
		TextureVulkan* gBufferAlbedo { nullptr };
		TextureVulkan* gPBR { nullptr };
		TextureVulkan* gBufferEmissive { nullptr };
		TextureVulkan* gBufferMotion { nullptr };
		TextureVulkan* depthTexture { nullptr };
	};

public:
	GeometryPassVulkan(std::string serviceName = "GeometryPassVulkan");
	virtual ~GeometryPassVulkan() override;

	virtual bool init(WindowConfig config) override;
	virtual bool onClose() override;
	virtual void onUpdate() override;
	virtual void render(Camera& camera) override;
	virtual void addToGraph(RenderGraphVulkan& graph) override;

	//TODO: make private once done testing
	RenderTarget renderTarget;

protected:
	const int numInstances = 1;

	std::vector<StorageBufferVulkan*> storagebuffersList;
	std::vector<StorageBufferVulkan*> prevStoragebufferList;
	std::vector<StorageBufferObject> instanceData;
	std::vector<StorageBufferObject> instanceDataPrev;
	Common::PushBDA pushConstant;

	uint32_t layoutID;
	uint32_t poolID;
	uint32_t setsID;
	std::unique_ptr<VulkanPipeline> gPassPipeline;

	// one objects table per frame in flight, the cpu rewrites it every frame while the gpu may still read the previous one
	std::vector<uint32_t> objDeviceAddressBufferIDs;
	std::vector<uint64_t> objDeviceAddresses;
	uint64_t materialsAddress;
	std::vector<Common::ObjectRefs> objects;

	void _updateSceneData(uint32_t currentFrame);
	void _renderGeometryPass(VkCommandBuffer cmd, uint32_t currentFrame);
	void _createRenderTargets();
	void _createDescriptor();
	void _updateDescriptor();
	void _createPipelines();
	void _recreateResources();
	void _cleanupResources();
};
