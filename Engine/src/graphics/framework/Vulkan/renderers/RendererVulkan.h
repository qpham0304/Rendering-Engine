#pragma once

#include "src/graphics/renderers/Renderer.h"
#include <glm/glm.hpp>

class RenderDeviceVulkan;
class RendererVulkan : public Renderer
{
private:
    struct PushConstantData {
        alignas(16) glm::vec3 color;
        alignas(16) glm::vec3 range;
        alignas(4)  bool flag;
        alignas(4)  float data;
    };


public:
	RendererVulkan();
	virtual ~RendererVulkan() override;

	virtual void init() override;
	virtual void beginFrame() override;
	virtual void endFrame() override;
	virtual void render() override;
	virtual void shutdown() override;
	virtual void addMesh() override;
	virtual void addModel(std::string_view path) override;

	void beginRecording(void* cmdBuffer);
	void endRecording(void* cmdBuffer);

protected:
	RenderDeviceVulkan* renderDeviceVulkan{ nullptr };
    PushConstantData pushConstantData {};

};

