#pragma once

#include "graphics/framework/vulkan/renderers/RendererVulkan.h"
#include <glm/glm.hpp>
#include <memory>
#include <vector>

class TextureVulkan;
class VulkanPipeline;

class AtrousPassVulkan : public RendererVulkan
{
public:
	struct PushConstant {
		glm::mat4 invProj;
		glm::vec2 screenRes;
		int stepSize;
		float depthSigma;
		float normalPower;
		float luminanceSigma;
		float specularRoughnessStart;
	};

public:
	AtrousPassVulkan(std::string serviceName = "AtrousPassVulkan");
	virtual ~AtrousPassVulkan() override;

	virtual bool init(WindowConfig config) override;
	virtual bool onClose() override;
	virtual void addToGraph(RenderGraphVulkan& graph) override;

	bool isFiltering() const;
	TextureVulkan* getDiffuseOutputImage() const;
	TextureVulkan* getSpecularOutputImage() const;

protected:
	virtual void _recreateResources() override;
	virtual void _cleanupResources() override;

	void _createTextures(RenderGraphVulkan& graph);
	void _createDescriptors();
	void _updateDescriptors(uint32_t frameIndex);
	void _execute(VkCommandBuffer cmd, uint32_t frameIndex);

	AtrousSettings* settings { nullptr };
	std::unique_ptr<VulkanPipeline> pipeline;
	PushConstant pushConstant {};

	TextureVulkan* inputDiffuse { nullptr };
	TextureVulkan* inputSpecular { nullptr };
	TextureVulkan* historyLength { nullptr };
	TextureVulkan* depthImage { nullptr };
	TextureVulkan* normalImage { nullptr };
	TextureVulkan* pbrImage { nullptr };

	TextureVulkan* diffuse[2] { nullptr, nullptr };
	TextureVulkan* specular[2] { nullptr, nullptr };
	RGHandle diffuseHandles[2] {};
	RGHandle specularHandles[2] {};
	int iterations { 0 };
	uint64_t activeFrame { 0 };
	uint32_t renderWidth { 0 };
	uint32_t renderHeight { 0 };

	uint32_t layoutID { 0 };
	uint32_t poolID { 0 };
	uint32_t setsFromInputID { 0 };
	uint32_t setsAToBID { 0 };
	uint32_t setsBToAID { 0 };
};
