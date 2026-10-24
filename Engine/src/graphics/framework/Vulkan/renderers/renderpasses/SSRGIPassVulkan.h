#pragma once

#include "PostProcessRendererVulkan.h"
#include <volk.h>

class AccelStructureBufferVulkan;

class SSRGIPassVulkan : public PostProcessRendererVulkan
{
public:
	struct PushConstant {
        alignas(8) glm::vec2 screenRes;
        alignas(4) float maxDistance;
        alignas(4) int mostDetailedMip;
		alignas(4) float thickness;
		alignas(4) float time;
		alignas(4) float frameSeed;
		alignas(4) int maxSteps;
		alignas(4) int useTLAS;
		alignas(8) uint64_t objectsRef;
	};

	struct UniformBufferObject {
		glm::mat4 projection;
		glm::mat4 view;
		glm::mat4 invProj;
		glm::mat4 invView;
		glm::vec4 sunDirection;
		glm::vec4 sunColor;
		float skyIntensity;
	};

public:
	SSRGIPassVulkan(std::string serviceName = "SSRGIPassVulkan");
	virtual ~SSRGIPassVulkan() override;

	virtual bool init(WindowConfig config) override;
	virtual bool onClose() override;
	virtual void onUpdate() override;
	virtual void render(Camera& camera) override;
	virtual void addToGraph(RenderGraphVulkan& graph) override;

    void writeSSRGI(VkCommandBuffer cmd, uint32_t currentFrame, VulkanPipeline* activePipeline);

public: // TODO: consider make private
	TextureVulkan* depthImageHiZ;
	TextureVulkan* depthImageRaw;
	TextureVulkan* normalImage;
	TextureVulkan* albedoImage;
	TextureVulkan* pbrImage;
	TextureVulkan* emissiveImage;
	TextureVulkan* colorImage;
	RGHandle outputHandle {};
	
protected:
	void _createResources();
	void _createPipelines();
	void _createDescriptors();
	void _updateDescriptor(uint32_t index);
	void _updateRayQueryDescriptor(uint32_t index, AccelStructureBufferVulkan* tlas);
	void _recreateResources();
	void _cleanupResources();


	uint32_t SSRGILayoutID;
	uint32_t SSRGIPoolID;
	uint32_t SSRGISetsID;
	UniformBufferObject ubo;
	PushConstant pushConstant;
	std::vector<UniformBufferVulkan*> uniformbuffersList;
	SSRSettings* settings { nullptr };
	bool rtSupported { false };
	std::unique_ptr<VulkanPipeline> rayQueryPipeline;
	std::vector<VkDescriptorSetLayoutBinding> bindings;
	uint32_t hizImageID { 0 };
	uint32_t traceWidth { 0 };
	uint32_t traceHeight { 0 };

};