#pragma once

#include "graphics/framework/vulkan/renderers/RendererVulkan.h"
#include <optional>
#include <unordered_map>

enum class BlurType : int { Box = 0, Gaussian = 1, Bilateral = 2 };

// separable blur other passes request with blur(), it adds its own horizontal and vertical nodes to the graph
// and hands back the blurred texture, same size and format as the input
class BlurPassVulkan : public RendererVulkan
{
public:
	struct PushConstant {
		glm::mat4 invProj;
		int type;
		int radius;
		float sigma;
		float depthScale;
		int vertical;
	};

	struct BlurParams {
		BlurType type { BlurType::Gaussian };
		int radius { 4 };
		float sigma { 2.0f };
		std::optional<RGHandle> depth;
		glm::mat4 invProj { 1.0f };
		float depthScale { 100.0f };
	};

public:
	BlurPassVulkan(std::string serviceName = "BlurPassVulkan");
	virtual ~BlurPassVulkan() override;

	virtual bool init(WindowConfig config) override;
	virtual bool onClose() override;
	virtual void onUpdate() override;
	virtual void render(Camera& camera) override;
	virtual void addToGraph(RenderGraphVulkan& graph) override;

	RGHandle blur(RenderGraphVulkan& graph, const std::string& name, RGHandle input, const BlurParams& params);

private:
	struct Request {
		uint32_t poolID { 0 };
		uint32_t setsHorizontalID { 0 };
		uint32_t setsVerticalID { 0 };
		uint32_t inputID { 0 };
		uint32_t depthID { 0 };
		uint32_t tempID { 0 };
		uint32_t outputID { 0 };
		PushConstant push {};
	};

	void _createPipelines();
	Request& _getRequest(const std::string& name);
	void _updateDescriptors(Request& request, TextureVulkan* input, TextureVulkan* depth, TextureVulkan* temp, TextureVulkan* output);
	void _writeBlur(VkCommandBuffer cmd, const Request& request, uint32_t setsID, TextureVulkan* target, bool vertical, uint32_t currentFrame);
	void _recreateResources() override;
	void _cleanupResources() override;

	uint32_t layoutID { 0 };
	std::unique_ptr<VulkanPipeline> pipeline;
	std::unordered_map<std::string, Request> m_requests;
};
