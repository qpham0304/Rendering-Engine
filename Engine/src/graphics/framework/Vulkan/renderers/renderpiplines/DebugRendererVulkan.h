#pragma once

#include "graphics/framework/vulkan/renderers/RendererVulkan.h"
#include <stddef.h>

class BufferVulkan;

class DebugRendererVulkan : public RendererVulkan
{
public:
    DebugRendererVulkan(std::string serviceName = "DebugRendererVulkan");

	virtual ~DebugRendererVulkan() override;

	virtual bool init(WindowConfig config) override;
	virtual bool onClose() override;
	virtual void onUpdate() override;
	virtual void render(Camera& camera) override;

	void drawLine(const glm::vec3& start, const glm::vec3& end);

private:
	struct DebugVertex {
		glm::vec3 pos;

		static VkVertexInputBindingDescription getBindingDescription() {
			VkVertexInputBindingDescription bindingDescription{};
			bindingDescription.binding = 0;
			bindingDescription.stride = sizeof(DebugVertex);
			bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
			return bindingDescription;
		}

		static std::array<VkVertexInputAttributeDescription, 1> getAttributeDescriptions() {
			std::array<VkVertexInputAttributeDescription, 1> attributeDescriptions{};
			attributeDescriptions[0].binding = 0;
			attributeDescriptions[0].location = 0;
			attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributeDescriptions[0].offset = offsetof(DebugVertex, pos);
			return attributeDescriptions;
		}
	};

	struct DebugDrawPushConstant {
		int imageIndex;
	};

	struct CombinePushConstant {
		int sceneIndex;
		int debugSceneIndex;
	};

	const uint32_t MAX_DEBUG_VERTICES = 10000;
	std::unique_ptr<VulkanPipeline> pipeline;
	std::unique_ptr<VulkanPipeline> combinePipeline;
	TextureVulkan* debugImage;
	TextureVulkan* displayImage;

	DebugDrawPushConstant pushConstant;
	CombinePushConstant combinePush;
	std::vector<DebugVertex> m_lineVertices;
	BufferVulkan* m_vertexBuffer;

	uint32_t layoutID { 0 };
	uint32_t poolID { 0 };
	uint32_t setsID { 0 };

	uint32_t displayLayoutID { 0 };
	uint32_t displayPoolID { 0 };
	uint32_t displaySetsID { 0 };

	uint32_t width { 0 };
	uint32_t height { 0 };

	void _createResources();
	void _createPipelines();
	void _createDescriptors();
	void _updateDescriptors();

	virtual void _recreateResources() override;
	virtual void _cleanupResources() override;
};