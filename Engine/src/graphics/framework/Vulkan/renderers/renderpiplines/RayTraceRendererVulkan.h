#pragma once

#include "graphics/framework/vulkan/renderers/RendererVulkan.h"
#include "graphics/framework/vulkan/core/VulkanRenderTarget.h"
#include "graphics/framework/vulkan/resources/buffers/BufferManagerVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpasses/ShadowMapPassVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpasses/ImageBasedRendererVulkan.h"
#include "graphics/framework/vulkan/core/VulkanRayTrace.h"

#include <glm/glm.hpp>
#include <vector>
#include <unordered_map>

class RayTraceRendererVulkan : public RendererVulkan
{

private:
	struct PushConstant {
		uint64_t objectsRef;
		uint32_t objectIdx;
		uint32_t bluenoiseIdx;
		uint32_t explicitPass;
	};

	struct ObjectDesc {
		uint64_t vertexAddress = 0;
		uint64_t indexAddress = 0;
		uint64_t materialsRef = 0;
		uint64_t materialIndicesRef = 0;
	};

	struct UniformBufferObject {
		glm::mat4 view;
		glm::mat4 prevViewProj;
		glm::mat4 proj;
		glm::vec4 cameraPos;
		glm::mat4 invView;
		glm::mat4 invProj;
		float width;
		float height;
		float frameSeed;
		int frameCount;
		bool clear;
	};

	struct StorageBufferObject {
		glm::mat4 model;
	};

	struct alignas(16) LightSSBO {
		glm::vec4 v0;            // 16 bytes (offset 16)
		glm::vec4 v1;            // 16 bytes (offset 32)
		glm::vec4 v2;            // 16 bytes (offset 48)
		uint32_t instanceIdx;    // 4 bytes  (offset 64)
		uint32_t triangleCount;  // 4 bytes  (offset 68)
		uint32_t padding1;       // 4 bytes  (offset 72)
		uint32_t padding2;       // 4 bytes  (offset 76)
	};

public:
    RayTraceRendererVulkan();
    virtual~RayTraceRendererVulkan() override;

    virtual bool init(WindowConfig config) override;
	virtual bool onClose() override;
	virtual void onUpdate() override;
	virtual void render(Camera& camera) override;
	
	void writePostProcess(VkCommandBuffer cmd, uint32_t currentFrame);
	void writeRayTracing(VkCommandBuffer cmd, uint32_t currentFrame);

public:	//TODO: make private once done testing	
	const int MAX_INSTANCES = 3000;
	const int numInstances = 1;
	const int numLights = 1000;
	bool showGui{ true };
	uint32_t frameCounter { 0 };
	bool m_tlasInitialized { false };
	bool clear { false };
	bool explicitPass { false };

	
	std::vector<UniformBufferVulkan*> uniformbuffersList;
	std::vector<StorageBufferVulkan*> storagebuffersList;
	std::vector<StorageBufferVulkan*> prevStoragebufferList;
	std::vector<StorageBufferVulkan*> lightStoragebuffers;
	std::vector<StorageBufferObject> instanceData;
	std::vector<StorageBufferObject> instanceDataPrev;


	PushConstant pushConstant;
	std::vector<LightSSBO> lights;

	Camera* cam;
	UniformBufferObject ubo{};
	glm::mat4 lastViewProj;
	bool firstFrame { true };

	std::unique_ptr<VulkanPipeline> rtPipeline { nullptr };
	uint32_t raytraceLayoutID;
	uint32_t raytracePoolID;
	uint32_t raytraceSetID;
	uint32_t rayTraceImageID;
	TextureVulkan* rayTraceImage;

	std::unique_ptr<VulkanPipeline> postProcessPipeline { nullptr };
	uint32_t postProcessLayoutID;
	uint32_t postProcessPoolID;
	uint32_t postProcessSetID;
	uint32_t postProcessImageID;
	TextureVulkan* postProcessImage;

	uint32_t objDeviceAddressBufferID;
	uint64_t objDeviceAddress;
	uint64_t materialsAddress;
	std::vector<ObjectDesc> objects;

    RaytracingBuilderKHR m_rtBuilder{};

    std::vector<VkDescriptorSetLayoutBinding> rtBindings;
    std::vector<VkDescriptorSetLayoutBinding> postBindings;

	
    uint32_t handleSize{};
    uint32_t handleAlignment{};
    uint32_t baseAlignment{};
	uint32_t m_shaderBindingTableBufferID;
    std::vector<uint8_t>            m_shaderHandles;     	// Storage for shader group handles
    VkStridedDeviceAddressRegionKHR m_rgenRegion{};    		// Ray generation shader region
    VkStridedDeviceAddressRegionKHR m_missRegion{};      	// Miss shader region
    VkStridedDeviceAddressRegionKHR m_hitRegion{};       	// Hit shader region
    VkStridedDeviceAddressRegionKHR m_callRegion{};



	void _renderGeometryPass(VkCommandBuffer cmd, uint32_t currentFrame);
	void _renderLightPass(VkCommandBuffer cmd, uint32_t currentFrame);
	void _createResources();
	void _createPipeline();
	void _createDescriptor();
	void _updateDescriptor(uint32_t index);
	void _recreateResources();
	void _cleanupResources();

	inline VkTransformMatrixKHR toTransformMatrixKHR(glm::mat4 matrix)
	{
		// VkTransformMatrixKHR uses a row-major layout, while glm::mat4's
		// column-major layout. so transpose the matrix to memcpy its data directly.
		glm::mat4        temp = glm::transpose(matrix);
		VkTransformMatrixKHR out_matrix;
		memcpy(&out_matrix, &temp, sizeof(VkTransformMatrixKHR));
		return out_matrix;
	}

	BlasInput _toVkGeometry(uint32_t meshID);
	void _createAccelStructure();
	void _createShaderBindingTable();
	void _updateTlas();
};