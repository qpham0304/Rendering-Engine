#pragma once

#include "Core/resources/managers/RendererManager.h"
#include "graphics/framework/vulkan/core/CommonStructs.h"
#include "RendererVulkan.h"

class RenderDeviceVulkan;
class TextureVulkan;

class RendererManagerVulkan : public RendererManager
{
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
	const Common::UniformBufferObject& getUBO() const;
	const std::vector<UniformBufferVulkan*>& getBufferUBO() const;


protected:
	uint32_t currentRenderMode { 0 };
	TextureVulkan* displayImage { nullptr };

	std::vector<UniformBufferVulkan*> uniformbuffersList{};
	Common::UniformBufferObject ubo {};
	glm::mat4 lastViewProj{};
	bool firstFrame { true };
	
	//Common::PushBDA pushBDA;
	//uint32_t objDeviceAddressBufferID;
	//uint64_t objDeviceAddress;
	//uint64_t materialsAddress;
	//std::vector<Common::ObjectRefs> objects;

	RenderDeviceVulkan* renderDeviceVulkan{ nullptr };
	Renderer* applicationRenderer { nullptr };
	Renderer* forwardRenderer { nullptr };
	Renderer* deferredRenderer { nullptr };
	Renderer* raytracingRenderer { nullptr };

	Renderer* shadowMapPass { nullptr };
	Renderer* imageBasedRenderer { nullptr };
	Renderer* ddgiPassRenderer { nullptr };
	Renderer* alchemyAORenderer { nullptr };
	Renderer* hiZPassRenderer { nullptr };
	Renderer* SSRGIPassRenderer { nullptr };
	Renderer* bloomRenderer { nullptr };
	Renderer* temporalPassRenderer { nullptr };
	Renderer* deferredCombineRenderer { nullptr };
	Renderer* postProcessRenderer { nullptr };
	Renderer* particleRenderer { nullptr };
	Renderer* debugRenderer { nullptr };

private:
	bool needResize{ false };

	//friend class renderer vulkan should not be able to call these
	virtual bool init(WindowConfig config) override;
	virtual bool onClose() override;
	virtual void destroy(uint32_t id) override;
	virtual void onUpdate() override;
    virtual void render() override;
	virtual void drawDebug(glm::vec3 start, glm::vec3 end) override;
	
	virtual void _cleanupResources();
	virtual void _recreateResources();

};