#pragma once

#include "graphics/framework/vulkan/renderers/RendererVulkan.h"
#include <stddef.h>

class BufferVulkan;

// draws debug lines and collider selection outlines on top of the current display image
// outlines follow box3d samples: selected/hovered shapes are rasterized into a
// mask (0.5 = hover, 1.0 = select), then the combine pass decode the mask and mark highlight boundary
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

	bool hasOutlineMesh(uint32_t key) const;
	void uploadOutlineMesh(uint32_t key, const std::vector<glm::vec3>& localTriangles);
	void drawOutline(uint32_t key, const glm::mat4& model, bool selected);

	glm::vec4 hoverColor { 0.3f, 0.7f, 1.0f, 1.0f };
	glm::vec4 selectColor { 1.0f, 0.55f, 0.1f, 1.0f };
	int hoverThickness { 1 };
	int selectThickness { 2 };

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

	struct MaskPushConstant {
		glm::mat4 model;
		float kind;		// 0.5 = hover, 1.0 = select
	};

	struct OutlineMesh {
		BufferVulkan* buffer { nullptr };
		uint32_t vertexCount { 0 };
	};

	struct OutlineDraw {
		uint32_t key;
		MaskPushConstant push;
	};

	struct CombinePushConstant {
		glm::vec4 hoverColor;
		glm::vec4 selectColor;
		int sceneIndex;
		int debugSceneIndex;
		int maskIndex;
		int hoverThickness;
		int selectThickness;
	};

	const uint32_t MAX_DEBUG_VERTICES = 10000;
	std::unique_ptr<VulkanPipeline> pipeline;
	std::unique_ptr<VulkanPipeline> maskPipeline;
	std::unique_ptr<VulkanPipeline> combinePipeline;
	TextureVulkan* debugImage;
	TextureVulkan* maskImage;
	TextureVulkan* displayImage;

	DebugDrawPushConstant pushConstant;
	CombinePushConstant combinePush;
	std::vector<DebugVertex> m_lineVertices;
	std::vector<BufferVulkan*> m_vertexBuffers;
	std::unordered_map<uint32_t, OutlineMesh> m_outlineMeshes;
	std::vector<OutlineDraw> m_outlineDraws;

	uint32_t layoutID { 0 };
	uint32_t poolID { 0 };
	uint32_t setsID { 0 };

	uint32_t displayLayoutID { 0 };
	uint32_t displayPoolID { 0 };
	uint32_t displaySetsID { 0 };

	uint32_t width { 0 };
	uint32_t height { 0 };

	BufferVulkan* _createMappedBuffer(size_t bufferSize);
	void _createBuffers();
	void _createResources();
	void _createPipelines();
	void _createDescriptors();
	void _updateDescriptors();
	void _clearDrawRequests();

	virtual void _recreateResources() override;
	virtual void _cleanupResources() override;
};
