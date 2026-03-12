#pragma once
#include "graphics/renderers/Renderer.h"
#include "graphics/framework/vulkan/core/VulkanRenderTarget.h"
#include "graphics/framework/vulkan/resources/buffers/BufferManagerVulkan.h"
 

class Logger;
class TextureManager;
class MeshManager;
class ModelManager;
class GuiManager;
class BufferManager;
class RenderDeviceVulkan;
class TextureVulkan;
class DescriptorManagerVulkan;
class MaterialManager;
class VulkanPipeline;
class Camera;

class ImageBasedRendererVulkan : public Renderer
{
public:
    ImageBasedRendererVulkan();
    virtual ~ImageBasedRendererVulkan() override;

	virtual bool init(WindowConfig config) override;
	virtual bool onClose() override;
	virtual void onUpdate() override;
	virtual void beginFrame() override {};
	virtual void endFrame() override {};
	virtual void render(Camera& camera) override;

	void computeSH(VkCommandBuffer cmd, uint32_t currentFrame);
	void writeBRDF();
	void computePrefilter(VkCommandBuffer cmd, uint32_t currentFrame);
	void loadTexture(std::string_view path);

	uint32_t hdrImageID{0};
	TextureVulkan* hdrImage{ nullptr };

	uint32_t hdrImageID_temp{0};
	TextureVulkan* hdrImage_temp{ nullptr };

	std::vector<StorageBufferVulkan*> finalSumBuffers;
	TextureVulkan* brdfLUT;
	TextureVulkan* prefilterMap;
	
protected:
	Logger* m_logger{ nullptr };
	RenderDeviceVulkan* renderDeviceVulkan{ nullptr };
	MeshManager* meshManager{ nullptr };
	ModelManager* modelManager{ nullptr };
	GuiManager* guiManager{ nullptr };
	TextureManager* textureManager{ nullptr };
	MaterialManager* materialManager{ nullptr };
    BufferManager* bufferManager{ nullptr };
	BufferManagerVulkan* bufferManagerVulkan{ nullptr };
	DescriptorManagerVulkan* descriptorManagerVulkan{ nullptr };


	struct PartialSum {
		glm::vec4 coefficients;
	};
	std::vector<StorageBufferVulkan*> partialSumBuffers;
	std::unique_ptr<VulkanPipeline> projectionSH_pipeline;
	uint32_t projectionSH_descriptorLayoutID;
	uint32_t projectionSH_descriptorPoolID;
	uint32_t projectionSH_descriptorSetID;
	
	struct FinalSH {
		glm::vec4 finalCoeffs[9];
	};
	std::unique_ptr<VulkanPipeline> sumSH_pipeline;
	uint32_t sumSH_descriptorLayoutID;
	uint32_t sumSH_descriptorPoolID;
	uint32_t sumSH_descriptorSetID;
	
	uint32_t groupCountX;
	uint32_t groupCountY;
	uint32_t totalWorkgroups;

	
	std::unique_ptr<VulkanPipeline> brdfLUT_pipeline;
	uint32_t lutDescriptorLayoutID;
	uint32_t lutDescriptorPoolID;
	uint32_t lutDescriptorSetID;

	struct PrefilterPushConstants {
		float roughness;
		int mipSize;
	};
	PrefilterPushConstants push{};
	std::unique_ptr<VulkanPipeline> prefilter_pipeline;
	uint32_t prefilterLayoutID;
	uint32_t prefilterPoolID;
	uint32_t prefilterSetID;

	std::vector<VkImageView> prefilterMipViews;
	std::vector<VkDescriptorSet> prefilterSets;
	uint32_t mipLevels;
	uint32_t mapSize{ 128 };

private:
	void _createDescriptorSetProjection();
	void _createDescriptorSetGlobalSum();
	void _createResourceLUT();
	void _createResourcePrefilteredMap();
};