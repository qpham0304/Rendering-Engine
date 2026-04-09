#include "DeferredRendererVulkan.h"
#include "core/features/ServiceLocator.h"
#include "core/events/EventManager.h"
#include "graphics/renderers/RenderDevice.h"
#include "window/AppWindow.h"

#include <core/resources/managers/TextureManager.h>
#include <core/resources/managers/MeshManager.h>
#include <core/resources/managers/ModelManager.h>
#include <core/resources/managers/DescriptorManager.h>
#include <gui/GuiManager.h>
#include <core/features/Mesh.h>
#include <core/features/Camera.h>
#include <graphics/framework/Vulkan/resources/textures/TextureVulkan.h>
#include <graphics/framework/Vulkan/resources/descriptors/DescriptorManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/materials/MaterialManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/textures/TextureManagerVulkan.h>
#include <graphics/framework/Vulkan/renderers/RendererManagerVulkan.h>
#include <graphics/framework/vulkan/core/VulkanPipeline.h>
#include <graphics/framework/Vulkan/renderers/RenderDeviceVulkan.h>
#include <core/scene/SceneManager.h>
#include <imgui.h>
#include "graphics/framework/vulkan/renderers/renderpasses/AlchemyAORendererVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpasses/HiZPassVulkan.h"
#include "graphics/framework/vulkan/renderers/renderpasses/SSRGIPassVulkan.h"

DeferredRendererVulkan::DeferredRendererVulkan() 
	: RendererVulkan("DeferredRendererVulkan")
{

}

DeferredRendererVulkan::~DeferredRendererVulkan()
{

}

bool DeferredRendererVulkan::init(WindowConfig config)
{
	RendererVulkan::init(config);

	_createRenderPasses();
	_createFrameBuffers();

	RendererVulkan* renderer = nullptr;
	renderer = rendererManagerVulkan->getRenderer("ShadowMapRendererVulkan");
	shadowMapRenderer = dynamic_cast<ShadowMapRendererVulkan*>(renderer);
	renderer = rendererManagerVulkan->getRenderer("ImageBasedRendererVulkan");
	imageBasedRenderer = dynamic_cast<ImageBasedRendererVulkan*>(renderer);
	renderer = rendererManagerVulkan->getRenderer("AlchemyAORendererVulkan");
	alchemyAORendererVulkan = dynamic_cast<AlchemyAORendererVulkan*>(renderer);
	renderer = rendererManagerVulkan->getRenderer("HiZPassVulkan");
	hiZPassRenderer = dynamic_cast<HiZPassVulkan*>(renderer);
	renderer = rendererManagerVulkan->getRenderer("SSRGIPassVulkan");
	SSRGIPassRenderer = dynamic_cast<SSRGIPassVulkan*>(renderer);

	assert(shadowMapRenderer 
		&& imageBasedRenderer 
		&& alchemyAORendererVulkan 
		&& hiZPassRenderer 
		&& "failed to retrieve renderer"
	);

	alchemyAORendererVulkan->init(config);
	hiZPassRenderer->init(config);
	SSRGIPassRenderer->init(config);

	bufferManagerVulkan->createUniformBuffers(uniformbuffersList, sizeof(UniformBufferObject));
	
	instanceData.resize(10000);
	size_t bufferSize = 10000 * sizeof(StorageBufferObject);
	bufferManagerVulkan->createStorageBuffers(storagebuffersList, bufferSize);

	lights.reserve(numLights);
	size_t lightBufferSize = numLights * sizeof(LightSSBO);
	bufferManagerVulkan->createStorageBuffers(lightStoragebuffers, lightBufferSize);

	pushConstantLight.skyboxDetail = 0.0f;
	pushConstantLight.color = sunColor * sunIntensity;
	pushConstantLight.bias = 0.001f;
	pushConstantLight.alpha = 0.0001f;
    pushConstantLight.lintstepLow = 0.2f;
    pushConstantLight.linstepHigh = 1.0f;
    pushConstantLight.litBias = 0.0005f;
	pushConstantLight.aoOn = 1;
	pushConstantLight.G = 0.7f;
	pushConstantLight.scatteringScale = 0.2f;

	_createDescriptor();
	_createPipelines();

	_createLightDescriptor();
	_createLightPipeline();


	return true;
}

bool DeferredRendererVulkan::onClose()
{
	renderDeviceVulkan->waitIdle();
	_cleanupResources();
	
	return true;
}

void DeferredRendererVulkan::onUpdate()
{
	
}

void DeferredRendererVulkan::render(Camera& camera)
{
	RendererVulkan::render(camera);
	
	if(needResize) {
		_recreateResources();
		needResize = false;
		return;
	}

	instanceData.clear(); 
    lights.clear();

	SceneManager& sceneManager = SceneManager::getInstance();
	Scene* scene = sceneManager.getActiveScene();
	if(!scene){
		m_logger->error("No scene to render");
	}
    auto entities = scene->getEntitiesWith<TransformComponent>();
    for (auto& entity : entities) {
        auto& transform = entity.getComponent<TransformComponent>();
        instanceData.push_back({ transform.getModelMatrix() });

        if (entity.hasComponent<LightComponent>()) {
            auto& light = entity.getComponent<LightComponent>();
			lights.push_back(LightSSBO(
				light.color, 
				glm::vec4(transform.translateVec, 1.0), 
				light.intensity * transform.scaleVec.x
			));
        }
    }

	ubo.view = camera.getViewMatrix();
	ubo.proj = camera.getProjectionMatrix();
	ubo.cameraPos = glm::vec4(camera.getPosition(), 1.0);
	ubo.proj[1][1] *= -1.0;
	ubo.invView = camera.getInViewMatrix();
	ubo.invProj = camera.getInProjectionMatrix();
	ubo.invProj[1][1] *= -1.0;
	ubo.width = renderTarget.width;
	ubo.height = renderTarget.height;
	
	pushConstantLight.color = sunColor * sunIntensity;
	pushConstantLight.direction = glm::vec4(shadowMapRenderer->lightDir, 0.0f);
	pushConstantLight.sunlightMVP = shadowMapRenderer->lightSpaceMatrix;
    pushConstantLight.time = AppWindow::getTime();
	pushConstantLight.numLights = lights.size();
	
	VkCommandBuffer cmdBuffer = renderDeviceVulkan->commandPool.currentBuffer();
	uint32_t currentFrame = renderDeviceVulkan->getCurrentFrameIndex();
	uniformbuffersList[currentFrame]->update(&ubo, sizeof(ubo));

	StorageBufferVulkan* ssbo = storagebuffersList[currentFrame];
	ssbo->update(instanceData.data(), instanceData.size() * sizeof(StorageBufferObject));

	StorageBufferVulkan* lightSSBO = lightStoragebuffers[currentFrame];
	lightSSBO->update(lights.data(), lights.size() * sizeof(LightSSBO));
	
	shadowMapRenderer->render(camera);
	imageBasedRenderer->onUpdate();
	imageBasedRenderer->computeSH(cmdBuffer, currentFrame);
	imageBasedRenderer->computePrefilter(cmdBuffer, currentFrame);
	
	// recordDrawCommand(cmdBuffer, renderDeviceVulkan->getImageIndex());
	recordDrawCommand(cmdBuffer, currentFrame);
	// rendererManagerVulkan->setDisplayImage(renderTarget.colorTextures[currentFrame]);
	renderDeviceVulkan->waitIdle();
	rendererManagerVulkan->setDisplayImage(SSRGIPassRenderer->getOutputImage());
}

void DeferredRendererVulkan::renderGui()
{
	SceneManager& sceneManager = SceneManager::getInstance();
	Scene* scene = sceneManager.getActiveScene();
	if (!scene) {
		m_logger->error("No scene to render");
	}

	auto entities = scene->getEntitiesWith<LightComponent>();

	
	ImGui::Begin("Lights Control");
	if(ImGui::Button("Add Pipeline")) {
		AsyncEvent e;
		EventManager::getInstance().queue(e, [this] (AsyncEvent& event) {
			rendererManagerVulkan->addRenderer<DeferredRendererVulkan>("DeferredRendererVulkanTemp");

			PipelineConfigInfo gBufferConfig = VulkanPipeline::defaultPipelineConfigInfo(5);
			gBufferConfig.renderPass = renderTarget.renderPass;

			auto bindingDescription = VulkanDevice::VertexVulkan::getBindingDescription();
			auto attributeDescriptions = VulkanDevice::VertexVulkan::getAttributeDescriptions();
			VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
			vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
			vertexInputInfo.vertexBindingDescriptionCount = 1;
			vertexInputInfo.vertexAttributeDescriptionCount = attributeDescriptions.size();
			vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
			vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

			VkDescriptorSetLayout descriptorSetLayout = descriptorManagerVulkan->getDescriptorLayout(layoutID);
			VkDescriptorPool descriptorPool = descriptorManagerVulkan->getDescriptorPool(poolID);
			
			void* handle = materialManager->getMaterialLayout();
			auto materialLayout = reinterpret_cast<VkDescriptorSetLayout>(handle);
			std::vector<VkDescriptorSetLayout> layouts = { descriptorSetLayout, materialLayout };
			
			tempPipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
			tempPipeline->createGraphicsPipeline(
				"assets/shaders/spv/gBuffer.vert.spv", 
				"assets/shaders/spv/gBuffer.frag.spv", 
				gBufferConfig, 
				vertexInputInfo, 
				layouts, 
				0
			);
		});
	}
	ImGui::SameLine();
	ImGui::Text(tempPipeline ? "loaded pipelien" : "loading...");
	
	if(ImGui::Button("Change Environment")) {
		std::string path;
		path = Utils::fileDialog();
		if(!path.empty()) {
			imageBasedRenderer->loadTexture(path);
		}
	}
	ImGui::SliderFloat("skybox detail", &pushConstantLight.skyboxDetail, 0.0f, 1.0f);
	ImGui::SliderFloat3("Base Light Dir", &shadowMapRenderer->lightDir[0], -1.0f, 1.0f);
	ImGui::SliderFloat("Sun Azimuth", &shadowMapRenderer->sunAzimuth, 0.0f, 6.28f);
	ImGui::SliderFloat("Sun Elevation", &shadowMapRenderer->sunElevation, -3.14f, 3.14f);
	ImGui::SliderFloat("Sun view Area", &shadowMapRenderer->s, 1.0f, 25.0f);
	ImGui::SliderFloat("Sun zNear", &shadowMapRenderer->zNear, 0.01f, 15.0f);
	ImGui::SliderFloat("Sun zFar", &shadowMapRenderer->zFar, 50.0f, 200.0f);

	// ImGui::SliderFloat("Sun Azimuth", &shadowMapRenderer->sunAzimuthDeg, 0.0f, 360.0f);
	// ImGui::SliderFloat("Sun Elevation", &shadowMapRenderer->sunElevationDeg, 0.0f, 90.0f);
	ImGui::DragFloat("G phase function", &pushConstantLight.G, 0.01f, 0.0f, 1.0f);
	ImGui::DragFloat("scattering Scale", &pushConstantLight.scatteringScale, 0.01f, 0.0f, 5.0f);
	
	ImGui::Checkbox("Light Ortho", &shadowMapRenderer->useOrtho);
	ImGui::SameLine();
	bool aoChecked = (pushConstantLight.aoOn != 0);
	if (ImGui::Checkbox("aoOn", &aoChecked)) {
		pushConstantLight.aoOn = aoChecked ? 1 : 0;
	}
	ImGui::SliderInt("aoBlurRadius", &alchemyAORendererVulkan->blurrPushConstant.blurRadius, 1.0f, 16.0f);
	ImGui::SliderFloat("aoBlurScale", &alchemyAORendererVulkan->blurrPushConstant.scale, 1.0f, 100.0f);
	ImGui::ColorEdit4("color", &sunColor[0]);
	ImGui::SliderFloat("intensity", &sunIntensity, 1.0f, 15.0f);
	ImGui::SliderFloat("Bias", &pushConstantLight.bias, 0.001f, 0.1f);
	ImGui::SliderFloat("Alpha", &pushConstantLight.alpha, 0.0001f, 0.01f);
	ImGui::SliderFloat("Lintstep Low", &pushConstantLight.lintstepLow, 0.01f, 1.0f);
	ImGui::SliderFloat("Lintstep High", &pushConstantLight.linstepHigh, 0.01f, 2.0f);
	ImGui::SliderFloat("Lit Bias", &pushConstantLight.litBias, 0.0001f, 0.01f, "%.4f", ImGuiSliderFlags_Logarithmic);
	uint32_t min_r = 1;
	uint32_t max_r = 64;
	ImGui::SliderScalar("radius", ImGuiDataType_U32, &shadowMapRenderer->pushconstant.radius, &min_r, &max_r);
	ImGui::SliderFloat("sigma", &shadowMapRenderer->pushconstant.sigma, 1.0f, 30.0f, "%.4f", ImGuiSliderFlags_Logarithmic);


	int i = 0;
	for (auto& entity : entities) {
		TransformComponent& transform = entity.getComponent<TransformComponent>();
		LightComponent& light = entity.getComponent<LightComponent>();

		std::string name = entity.getComponent<NameComponent>().name + std::to_string(i);
		if (ImGui::CollapsingHeader(name.c_str())) {
			ImGui::SliderFloat4("Position", &transform.translateVec[0], -50.0, 50.0);
			ImGui::ColorEdit4("Color", &light.color[0]);
		}

		lights[i].color = light.color;
		lights[i].position = glm::vec4(transform.translateVec, 1.0f);

		i++;
	}
	ImGui::End();

	ImGui::Begin("G-Buffer Debug");
	
	uint32_t currentFrame = renderDeviceVulkan->getCurrentFrameIndex();
	ImVec2 size(256, 144);
	ImGui::Image((ImTextureID)textureManagerVulkan->inspectTexture(renderTarget.colorTextures[currentFrame]->id()), ImVec2(256, 144));
	ImGui::Image((ImTextureID)textureManagerVulkan->inspectTexture(renderTarget.gBufferPos[currentFrame]->id()), ImVec2(256, 144));
	ImGui::Image((ImTextureID)textureManagerVulkan->inspectTexture(renderTarget.gBufferNorm[currentFrame]->id()), ImVec2(256, 144));
	ImGui::Image((ImTextureID)textureManagerVulkan->inspectTexture(renderTarget.gBufferAlbedo[currentFrame]->id()), ImVec2(256, 144));
	ImGui::Image((ImTextureID)textureManagerVulkan->inspectTexture(renderTarget.gPBR[currentFrame]->id()), ImVec2(256, 144));
	ImGui::Image((ImTextureID)textureManagerVulkan->inspectTexture(renderTarget.depthTextures[currentFrame]->id()), ImVec2(256, 144));
	ImGui::Image((ImTextureID)(textureManagerVulkan->inspectTexture(shadowMapRenderer->depthID)), ImVec2(256, 144));
	ImGui::Image((ImTextureID)(textureManagerVulkan->inspectTexture(SSRGIPassRenderer->getOutputImage()->id())), ImVec2(256, 144));

	ImGui::End();
}

void DeferredRendererVulkan::recordDrawCommand(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
	beginRecording(commandBuffer,renderTarget.renderPass,renderTarget.framebuffers[imageIndex]);
	
	uint32_t currentFrame = renderDeviceVulkan->getCurrentFrameIndex();
	_renderGeometryPass(commandBuffer, currentFrame);
    vkCmdNextSubpass(commandBuffer, VK_SUBPASS_CONTENTS_INLINE);	// transition to next pass
	_renderLightPass(commandBuffer, currentFrame);

	endRecording(commandBuffer);
}

void DeferredRendererVulkan::beginRecording(void* cmdBuffer, void* renderPass, void* frameBuffer)
{
	VkCommandBuffer commandBuffer = static_cast<VkCommandBuffer>(cmdBuffer);
	VkRenderPass vulkanRenderPass = static_cast<VkRenderPass>(renderPass);
	VkFramebuffer vulkanFrameBuffer = static_cast<VkFramebuffer>(frameBuffer);

	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = vulkanRenderPass;
	renderPassInfo.framebuffer = vulkanFrameBuffer;
	renderPassInfo.renderArea.offset = { 0, 0 };
	// renderPassInfo.renderArea.extent = renderDeviceVulkan->swapchain.swapChainExtent;
	// renderPassInfo.renderArea.extent = { AppWindow::getWidth(), AppWindow::getHeight() };
	renderPassInfo.renderArea.extent = { renderTarget.width, renderTarget.height };


	std::array<VkClearValue, 7> clearValues{};
	clearValues[0].color = { 0.15f, 0.15f, 0.15f, 1.0f }; 	// Final Swapchain
	clearValues[1].color = { 0.0f, 0.0f, 0.0f, 1.0f };    	// Position
	clearValues[2].color = { 0.0f, 0.0f, 0.0f, 1.0f };    	// Normal
	clearValues[3].color = { 0.0f, 0.0f, 0.0f, 1.0f };    	// Albedo
	clearValues[4].color = { 1.0f, 1.0f, 0.0f, 1.0f };    	// PBR (ORM)
	clearValues[5].color = { 1.0f, 1.0f, 0.0f, 1.0f };    	// Emissive
	clearValues[6].depthStencil = { 1.0f, 0 };            								// Depth (Index 5)

	renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	renderPassInfo.pClearValues = clearValues.data();

	renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	renderPassInfo.pClearValues = clearValues.data();

	vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void DeferredRendererVulkan::endRecording(void* cmdBuffer)
{
	vkCmdEndRenderPass(static_cast<VkCommandBuffer>(cmdBuffer));
}

void DeferredRendererVulkan::_renderGeometryPass(VkCommandBuffer cmd, uint32_t currentFrame)
{
	gPassPipeline->bind(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS);

	renderDeviceVulkan->setViewport(renderTarget.width, renderTarget.height);
	renderDeviceVulkan->setScissor(renderTarget.width, renderTarget.height);

	auto descriptorSets = descriptorManagerVulkan->getDescriptorSet(setsID);
	vkCmdBindDescriptorSets(
		cmd,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		gPassPipeline->pipelineLayout,
		0,
		1,
		&descriptorSets[currentFrame],
		0,
		nullptr
	);


	SceneManager& sceneManager = SceneManager::getInstance();
	Scene* scene = sceneManager.getActiveScene();
	if (!scene) {
		m_logger->error("No scene to render");
	}

	int index = 0;
	int lightIndex = 0;
	for (auto& entity : scene->getEntitiesWith<TransformComponent>()) {
		TransformComponent& transform = entity.getComponent<TransformComponent>();
		const glm::mat4& entityTransform = transform.getModelMatrix();
		glm::vec3& translation = transform.translateVec;
		
		if(index >= instanceData.size()) {
			instanceData.push_back({entityTransform});
			continue;
		}

		// TODO: copy the multiple all transforms to ssbo would be slow
		if (instanceData[index].model != entityTransform) {
			instanceData[index].model = entityTransform;
		}

		if (entity.hasComponent<LightComponent>()) {
			lightIndex++;
		}

		if(entity.hasComponent<ModelComponent>()) {
			uint32_t modelID = entity.getComponent<ModelComponent>().modelID;
			const Model* model = modelManager->getModel(modelID);

			if (!model) {
				continue;
			}
			for (uint32_t meshID : model->meshIDs) {
				const Mesh* mesh = meshManager->getMesh(meshID);
				materialManager->bindMaterial(mesh->materialID, cmd, (void*)gPassPipeline.get());
				meshManager->bindMesh(meshID);

				uint32_t indexCount = static_cast<uint32_t>(mesh->indices.size());
				renderDeviceVulkan->draw(indexCount, numInstances, index);
			}
		} 
		
		if (entity.hasComponent<MeshComponent>()) {
			MeshComponent meshComponent = entity.getComponent<MeshComponent>();
			for (uint32_t meshID : meshComponent.meshIDs) {
				const Mesh* mesh = meshManager->getMesh(meshID);
				materialManager->bindMaterial(mesh->materialID, cmd, (void*)gPassPipeline.get());
				meshManager->bindMesh(meshID);

				uint32_t indexCount = static_cast<uint32_t>(mesh->indices.size());
				renderDeviceVulkan->draw(indexCount, numInstances, index);
			}
		}
		
		index++;
	}
}

void DeferredRendererVulkan::_renderLightPass(VkCommandBuffer cmd, uint32_t currentFrame)
{
	lightingPipeline->bind(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS);


    auto lightSets = descriptorManagerVulkan->getDescriptorSet(lightSetsID);
    vkCmdBindDescriptorSets(
        cmd,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        lightingPipeline->pipelineLayout,
        0, 
		1, 
		&lightSets[currentFrame],
        0, nullptr
    );

	auto lightDescriptorSets_1 = descriptorManagerVulkan->getDescriptorSet(lightSetsID_1);
	vkCmdBindDescriptorSets(
		cmd,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		lightingPipeline->pipelineLayout,
		1,
		1,
		&lightDescriptorSets_1[currentFrame],
		0,
		nullptr
	);

	vkCmdPushConstants(
		cmd,
		lightingPipeline->pipelineLayout,
		VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		0,
		sizeof(PushConstantLight),
		&pushConstantLight
	);

    vkCmdDraw(cmd, 3, 1, 0, 0);
}

#pragma region setup
void DeferredRendererVulkan::_createRenderPasses()
{
	VulkanSwapChain& swapchain = renderDeviceVulkan->swapchain;
	VkDevice device = renderDeviceVulkan->device;
	renderTarget.width = swapchain.swapChainExtent.width;
	renderTarget.height = swapchain.swapChainExtent.height;

	VkAttachmentDescription colorAttachment{};
	colorAttachment.format = swapchain.swapChainImageFormat;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // transition automatically

	VkAttachmentDescription gBufferPos{};
	gBufferPos.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	gBufferPos.samples = VK_SAMPLE_COUNT_1_BIT;
	gBufferPos.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	gBufferPos.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	gBufferPos.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	gBufferPos.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkAttachmentDescription gBufferNorm{};
	gBufferNorm.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	gBufferNorm.samples = VK_SAMPLE_COUNT_1_BIT;
	gBufferNorm.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	gBufferNorm.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	gBufferNorm.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	gBufferNorm.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkAttachmentDescription gBufferAlbedo{};
	gBufferAlbedo.format = VK_FORMAT_R8G8B8A8_UNORM;
	gBufferAlbedo.samples = VK_SAMPLE_COUNT_1_BIT;
	gBufferAlbedo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	gBufferAlbedo.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	gBufferAlbedo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	gBufferAlbedo.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkAttachmentDescription gBufferPBR{};
	gBufferPBR.format = VK_FORMAT_R8G8B8A8_UNORM;
	gBufferPBR.samples = VK_SAMPLE_COUNT_1_BIT;
	gBufferPBR.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	gBufferPBR.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	gBufferPBR.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	gBufferPBR.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	gBufferPBR.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	gBufferPBR.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	
	VkAttachmentDescription gBufferEmissive{};
	gBufferEmissive.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	gBufferEmissive.samples = VK_SAMPLE_COUNT_1_BIT;
	gBufferEmissive.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	gBufferEmissive.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	gBufferEmissive.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	gBufferEmissive.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	gBufferEmissive.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	gBufferEmissive.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkAttachmentDescription depthAttachment{};
	depthAttachment.format = TextureManagerVulkan::findDepthFormat(renderDeviceVulkan->device);
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	// gbuffer pass
	std::vector<VkAttachmentReference> gBufferReferences = {
		{1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},	// Position
		{2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},	// Normal
		{3, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},	// Albedo
		{4, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},	// PBR
		{5, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}	// Emissive
	};

	VkAttachmentReference depthReference = {6, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

	VkSubpassDescription subpass0{};
	subpass0.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass0.colorAttachmentCount = gBufferReferences.size();
	subpass0.pColorAttachments = gBufferReferences.data();
	subpass0.pDepthStencilAttachment = &depthReference;

	// set the light pass shader to use subpassInput from gbuffer pass
	std::vector<VkAttachmentReference> inputReferences = {
		{1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
		{2, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
		{3, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
		{4, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
		{5, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}
	};

	// light pass 
	VkAttachmentReference colorAttachmentRef{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

	VkSubpassDescription subpass1{};
	subpass1.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass1.colorAttachmentCount = 1;
	subpass1.pColorAttachments = &colorAttachmentRef;
	subpass1.inputAttachmentCount = inputReferences.size();
	subpass1.pInputAttachments = inputReferences.data();
	subpass1.pDepthStencilAttachment = &depthReference;

	// validation layer complains if dependencies is not zero initialized?
	std::array<VkSubpassDependency, 3> dependencies{};

	// wait for swapchain, basically barrier but set up as subpass dependency
	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[0].srcAccessMask = 0;
	dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	// wait for gbuffer subpass before lighitng subpass reads
	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = 1;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	dependencies[1].dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT | VK_ACCESS_SHADER_READ_BIT;
	dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	dependencies[2].srcSubpass = 1;
	dependencies[2].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[2].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependencies[2].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
	dependencies[2].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependencies[2].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	dependencies[2].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	std::array<VkAttachmentDescription, 7> allAttachments = { 
		colorAttachment,
		gBufferPos,
		gBufferNorm,
		gBufferAlbedo,
		gBufferPBR,
		gBufferEmissive,
		depthAttachment
	};

	std::array<VkSubpassDescription, 2> subpasses = { subpass0, subpass1 };

	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = allAttachments.size();
	renderPassInfo.pAttachments = allAttachments.data();
	renderPassInfo.subpassCount = subpasses.size();
	renderPassInfo.pSubpasses = subpasses.data();
	renderPassInfo.dependencyCount = dependencies.size();
	renderPassInfo.pDependencies = dependencies.data();

	if(vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderTarget.renderPass) != VK_SUCCESS) {
		throw std::runtime_error("failed to create offscreen render pass!");
	}

}

void DeferredRendererVulkan::_createFrameBuffers()
{
	VulkanSwapChain& swapchain = renderDeviceVulkan->swapchain;
	VkDevice device = renderDeviceVulkan->device;
	
	uint32_t numFrames = VulkanUtils::numFrames();
	renderTarget.colorTextures.resize(numFrames);
	renderTarget.gBufferPos.resize(numFrames);
	renderTarget.gBufferNorm.resize(numFrames);
	renderTarget.gBufferAlbedo.resize(numFrames);
	renderTarget.gPBR.resize(numFrames);
	renderTarget.gBufferEmissive.resize(numFrames);
	renderTarget.depthTextures.resize(numFrames);
	renderTarget.framebuffers.resize(numFrames);

	for(size_t i = 0; i < renderTarget.colorTextures.size(); i++) {
		auto createTexture = [&] (VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspect) -> TextureVulkan* {
			uint32_t id = textureManagerVulkan->createTexture();
			auto* texture = static_cast<TextureVulkan*>(textureManagerVulkan->getTexture(id));

			TextureManagerVulkan::createImage(
				swapchain.swapChainExtent.width,
				swapchain.swapChainExtent.height,
				format,
				VK_IMAGE_TILING_OPTIMAL,
				usage,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				texture->textureImage,
				texture->textureImageMemory,
				1,
				renderDeviceVulkan->device
			);

			TextureManagerVulkan::createImageView(
				texture->textureImage,
				texture->textureImageView,
				format,
				aspect,
				1,
				renderDeviceVulkan->device
			);

			VkSamplerCreateInfo samplerInfo{};
			samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			samplerInfo.magFilter = VK_FILTER_LINEAR;
			samplerInfo.minFilter = VK_FILTER_LINEAR;
			samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
			samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
			samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
			samplerInfo.anisotropyEnable = VK_TRUE;
			samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
			samplerInfo.unnormalizedCoordinates = VK_FALSE;
			samplerInfo.compareEnable = VK_FALSE;
			samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
			samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			samplerInfo.mipLodBias = 0.0f;
			samplerInfo.minLod = 0.0f;
			samplerInfo.maxLod = 0.0f;

			TextureManagerVulkan::createTextureSampler(
				texture->textureSampler, 
				renderDeviceVulkan->device,
				samplerInfo
			);

			TextureManagerVulkan::transitionImageLayout(
				texture->textureImage, 
				format, 
				VK_IMAGE_LAYOUT_UNDEFINED, 
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
				1,
				renderDeviceVulkan
			);

			return texture;
		};

		renderTarget.colorTextures[i] = createTexture(
			swapchain.swapChainImageFormat, 
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 
			VK_IMAGE_ASPECT_COLOR_BIT
		);

		renderTarget.gBufferPos[i] = createTexture(
			VK_FORMAT_R16G16B16A16_SFLOAT, 
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT, 
			VK_IMAGE_ASPECT_COLOR_BIT
		);

		renderTarget.gBufferNorm[i] = createTexture(
			VK_FORMAT_R16G16B16A16_SFLOAT,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT, 
			VK_IMAGE_ASPECT_COLOR_BIT
		);

		renderTarget.gBufferAlbedo[i] = createTexture(
			VK_FORMAT_R8G8B8A8_UNORM, 
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, 
			VK_IMAGE_ASPECT_COLOR_BIT
		);

		renderTarget.gPBR[i] = createTexture(
			VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
			VK_IMAGE_ASPECT_COLOR_BIT
		);

		renderTarget.gBufferEmissive[i] = createTexture(
			VK_FORMAT_R16G16B16A16_SFLOAT, 
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT, 
			VK_IMAGE_ASPECT_COLOR_BIT
		);

		VkFormat depthFormat = TextureManagerVulkan::findDepthFormat(renderDeviceVulkan->device);

		uint32_t depthId = textureManagerVulkan->createTexture();
		renderTarget.depthTextures[i] = static_cast<TextureVulkan*>(textureManagerVulkan->getTexture(depthId));

		TextureManagerVulkan::createImage(
			renderDeviceVulkan->swapchain.swapChainExtent.width,
			renderDeviceVulkan->swapchain.swapChainExtent.height,
			depthFormat,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			renderTarget.depthTextures[i]->textureImage,
			renderTarget.depthTextures[i]->textureImageMemory,
			1,
			renderDeviceVulkan->device
		);
		
		TextureManagerVulkan::createImageView(
			renderTarget.depthTextures[i]->textureImage, 
			renderTarget.depthTextures[i]->textureImageView, 
			depthFormat, 
			VK_IMAGE_ASPECT_DEPTH_BIT,
			1,
			renderDeviceVulkan->device
		);

		TextureManagerVulkan::createTextureSampler(renderTarget.depthTextures[i]->textureSampler, renderDeviceVulkan->device);

		std::array<VkImageView, 7> attachments = {
			renderTarget.colorTextures[i]->textureImageView,
			renderTarget.gBufferPos[i]->textureImageView,
			renderTarget.gBufferNorm[i]->textureImageView,
			renderTarget.gBufferAlbedo[i]->textureImageView,
			renderTarget.gPBR[i]->textureImageView,
			renderTarget.gBufferEmissive[i]->textureImageView,
			renderTarget.depthTextures[i]->textureImageView
		};

		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = renderTarget.renderPass;
		framebufferInfo.attachmentCount = attachments.size();
		framebufferInfo.pAttachments = attachments.data();
		framebufferInfo.width = renderDeviceVulkan->swapchain.swapChainExtent.width;
		framebufferInfo.height = renderDeviceVulkan->swapchain.swapChainExtent.height;
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &renderTarget.framebuffers[i]) != VK_SUCCESS) {
			throw std::runtime_error("failed to create framebuffer!");
		}
	}
}

void DeferredRendererVulkan::_createDescriptor()
{
	std::vector<VkDescriptorSetLayoutBinding> bindings = { 
		{ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
		{ 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
	};
	layoutID = descriptorManagerVulkan->createLayout(bindings);
	
	uint32_t frameCount = VulkanUtils::numFrames();
	std::vector<VkDescriptorPoolSize> poolSizes = {
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frameCount },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, frameCount },
	};

	poolID = descriptorManagerVulkan->createPool(poolSizes, frameCount);

	setsID = descriptorManagerVulkan->createSets(layoutID, poolID, VulkanUtils::numFrames());
	_updateDescriptor();
}

void DeferredRendererVulkan::_updateDescriptor()
{
	auto descriptorSets = descriptorManagerVulkan->getDescriptorSet(setsID);

	for (size_t i = 0; i < VulkanUtils::numFrames(); i++) {
		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = static_cast<VkBuffer>(*uniformbuffersList[i]);
		bufferInfo.offset = 0;
		bufferInfo.range = VK_WHOLE_SIZE;

		VkDescriptorBufferInfo ssboInfo{};
		ssboInfo.buffer = static_cast<VkBuffer>(*storagebuffersList[i]);
		ssboInfo.offset = 0;
		ssboInfo.range = VK_WHOLE_SIZE;

		std::vector<VkWriteDescriptorSet> writes = {};
		descriptorManagerVulkan->writeUniform(&writes, descriptorSets[i], 0, bufferInfo);
		descriptorManagerVulkan->writeStorage(&writes, descriptorSets[i], 1, ssboInfo);
		descriptorManagerVulkan->updateDescriptorSets(&writes);
	}
}

void DeferredRendererVulkan::_createPipelines()
{
	PipelineConfigInfo gBufferConfig = VulkanPipeline::defaultPipelineConfigInfo(5);
	gBufferConfig.renderPass = renderTarget.renderPass;

	auto bindingDescription = VulkanDevice::VertexVulkan::getBindingDescription();
	auto attributeDescriptions = VulkanDevice::VertexVulkan::getAttributeDescriptions();
	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.vertexAttributeDescriptionCount = attributeDescriptions.size();
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

	VkDescriptorSetLayout descriptorSetLayout = descriptorManagerVulkan->getDescriptorLayout(layoutID);
	VkDescriptorPool descriptorPool = descriptorManagerVulkan->getDescriptorPool(poolID);
	
	void* handle = materialManager->getMaterialLayout();
	auto materialLayout = reinterpret_cast<VkDescriptorSetLayout>(handle);
	std::vector<VkDescriptorSetLayout> layouts = { descriptorSetLayout, materialLayout };
	
	gPassPipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
	gPassPipeline->createGraphicsPipeline(
		"assets/shaders/spv/gBuffer.vert.spv", 
		"assets/shaders/spv/gBuffer.frag.spv", 
		gBufferConfig, 
		vertexInputInfo, 
		layouts, 
		0
	);
}

void DeferredRendererVulkan::_createLightPipeline()
{
	auto lightDescriptorLayout = descriptorManagerVulkan->getDescriptorLayout(lightLayoutID);
	auto lightDescriptorLayout_1 = descriptorManagerVulkan->getDescriptorLayout(lightLayoutID_1);
	std::vector<VkDescriptorSetLayout> lightLayouts = { lightDescriptorLayout, lightDescriptorLayout_1 };

	// Prepare empty Vertex Input (No vertex buffer used)
	VkPipelineVertexInputStateCreateInfo emptyVertexInput{};
	emptyVertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	emptyVertexInput.vertexAttributeDescriptionCount = 0;
	emptyVertexInput.pVertexAttributeDescriptions = nullptr;
	emptyVertexInput.vertexBindingDescriptionCount = 0;
	emptyVertexInput.pVertexBindingDescriptions = nullptr;

	// Configure the PipelineInfo (Subpass 1, 1 Attachment, No Depth)
	PipelineConfigInfo lightConfig = VulkanPipeline::defaultPipelineConfigInfo(1);
	lightConfig.renderPass = renderTarget.renderPass;
	lightConfig.subpass = 1;
	lightConfig.depthStencilInfo.depthTestEnable = VK_FALSE;
	lightConfig.depthStencilInfo.depthWriteEnable = VK_FALSE;

	lightingPipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
	lightingPipeline->createGraphicsPipeline(
		"assets/shaders/spv/lightPass.vert.spv",
		"assets/shaders/spv/lightPass.frag.spv",
		lightConfig,
		emptyVertexInput,
		lightLayouts,
		sizeof(PushConstantLight)
	);
}

void DeferredRendererVulkan::_createLightDescriptor()
{
    VkDevice device = renderDeviceVulkan->device;

    std::vector<VkDescriptorSetLayoutBinding> bindings = {
        {0, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
		{2, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
		{3, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
		{4, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
    };
    
    uint32_t frameCount = VulkanUtils::numFrames();
    std::vector<VkDescriptorPoolSize> poolSizes{
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, frameCount * 5} 
    };

    lightLayoutID = descriptorManagerVulkan->createLayout(bindings);
    uint32_t lightPoolID = descriptorManagerVulkan->createPool(poolSizes, frameCount * 4);
    lightSetsID = descriptorManagerVulkan->createSets(lightLayoutID, lightPoolID, frameCount);

	lightLayoutID_1 = descriptorManagerVulkan->createLayout({
		{ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
		{ 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
		{ 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
		{ 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
		{ 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
		{ 5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
		{ 6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
		{ 7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
		{ 8, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
	});

	uint32_t poolIDLayout_1 = descriptorManagerVulkan->createPool(
		{
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frameCount },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, frameCount * 2},
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, frameCount * 6 },
		},
		frameCount
	);

	lightSetsID_1 = descriptorManagerVulkan->createSets(lightLayoutID_1, poolIDLayout_1, VulkanUtils::numFrames());
	_updateLightDescriptor();
}

void DeferredRendererVulkan::_updateLightDescriptor()
{
	auto lightingDescriptorSets = descriptorManagerVulkan->getDescriptorSet(lightSetsID);
    for(int i = 0; i < VulkanUtils::numFrames(); i++) {
		VkDescriptorImageInfo posInfo = { VK_NULL_HANDLE, renderTarget.gBufferPos[i]->textureImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
		VkDescriptorImageInfo normInfo = { VK_NULL_HANDLE, renderTarget.gBufferNorm[i]->textureImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
		VkDescriptorImageInfo albedoInfo = { VK_NULL_HANDLE, renderTarget.gBufferAlbedo[i]->textureImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
		VkDescriptorImageInfo pbrInfo = { VK_NULL_HANDLE, renderTarget.gPBR[i]->textureImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
		VkDescriptorImageInfo emissiveInfo = { VK_NULL_HANDLE, renderTarget.gBufferEmissive[i]->textureImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };

		std::vector<VkWriteDescriptorSet> writes;
		descriptorManagerVulkan->writeAttachment(&writes, lightingDescriptorSets[i], 0, posInfo);
		descriptorManagerVulkan->writeAttachment(&writes, lightingDescriptorSets[i], 1, normInfo);
		descriptorManagerVulkan->writeAttachment(&writes, lightingDescriptorSets[i], 2, albedoInfo);
		descriptorManagerVulkan->writeAttachment(&writes, lightingDescriptorSets[i], 3, pbrInfo);
		descriptorManagerVulkan->writeAttachment(&writes, lightingDescriptorSets[i], 4, emissiveInfo);
        descriptorManagerVulkan->updateDescriptorSets(&writes);
    }

	auto descriptorSets = descriptorManagerVulkan->getDescriptorSet(lightSetsID_1);
	for (size_t i = 0; i < VulkanUtils::numFrames(); i++) {
		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = static_cast<VkBuffer>(*uniformbuffersList[i]);
		bufferInfo.offset = 0;
		bufferInfo.range = VK_WHOLE_SIZE;

		VkDescriptorBufferInfo ssboInfo{};
		ssboInfo.buffer = static_cast<VkBuffer>(*lightStoragebuffers[i]);
		ssboInfo.offset = 0;
		ssboInfo.range = VK_WHOLE_SIZE;

		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = shadowMapRenderer->depthMap->textureImageView;
		imageInfo.sampler = shadowMapRenderer->depthMap->textureSampler;
		// imageInfo.imageView = shadowMapRenderer->momentImage->textureImageView;
		// imageInfo.sampler = shadowMapRenderer->momentImage->textureSampler;

		VkDescriptorImageInfo noiseImageInfo{};
		noiseImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		noiseImageInfo.imageView = shadowMapRenderer->blueNoiseImage->textureImageView;
		noiseImageInfo.sampler = shadowMapRenderer->blueNoiseImage->textureSampler;

		VkDescriptorBufferInfo bufferInfoSH{};
		bufferInfoSH.buffer = static_cast<VkBuffer>(*imageBasedRenderer->finalSumBuffers[i]);
		bufferInfoSH.offset = 0;
		bufferInfoSH.range = VK_WHOLE_SIZE;

		VkDescriptorImageInfo brdfLutImageInfo{};
		brdfLutImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		brdfLutImageInfo.imageView = imageBasedRenderer->brdfLUT->textureImageView;
		brdfLutImageInfo.sampler = imageBasedRenderer->brdfLUT->textureSampler;

		VkDescriptorImageInfo prefilterImageInfo{};
		prefilterImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		prefilterImageInfo.imageView = imageBasedRenderer->prefilterMap->textureImageView;
		prefilterImageInfo.sampler = imageBasedRenderer->prefilterMap->textureSampler;

		VkDescriptorImageInfo hdrImageInfo{};
		hdrImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		hdrImageInfo.imageView = imageBasedRenderer->hdrImage->textureImageView;
		hdrImageInfo.sampler = imageBasedRenderer->hdrImage->textureSampler;

		VkDescriptorImageInfo aoImageInfo{};
		aoImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		aoImageInfo.imageView = alchemyAORendererVulkan->getOutputImage()->textureImageView;
		aoImageInfo.sampler = alchemyAORendererVulkan->getOutputImage()->textureSampler;
	
		std::vector<VkWriteDescriptorSet> writes = {};
		descriptorManagerVulkan->writeUniform(&writes, descriptorSets[i], 0, bufferInfo);
		descriptorManagerVulkan->writeStorage(&writes, descriptorSets[i], 1, ssboInfo);
		descriptorManagerVulkan->writeImage(&writes, descriptorSets[i], 2, imageInfo);
		descriptorManagerVulkan->writeImage(&writes, descriptorSets[i], 3, noiseImageInfo);
		descriptorManagerVulkan->writeStorage(&writes, descriptorSets[i], 4, bufferInfoSH);
		descriptorManagerVulkan->writeImage(&writes, descriptorSets[i], 5, brdfLutImageInfo);
		descriptorManagerVulkan->writeImage(&writes, descriptorSets[i], 6, prefilterImageInfo);
		descriptorManagerVulkan->writeImage(&writes, descriptorSets[i], 7, hdrImageInfo);
		descriptorManagerVulkan->writeImage(&writes, descriptorSets[i], 8, aoImageInfo);

		
		descriptorManagerVulkan->updateDescriptorSets(&writes);
	}
}

void DeferredRendererVulkan::_recreateResources()
{
	renderDeviceVulkan->waitIdle();
	rendererManagerVulkan->setDisplayImage(nullptr);
	_cleanupResources();

	_createRenderPasses();
	_createFrameBuffers();

	_updateDescriptor();
	_updateLightDescriptor();

	_createPipelines();
	_createLightPipeline();
}

void DeferredRendererVulkan::_cleanupResources()
{
	renderTarget.destroy(renderDeviceVulkan->device);
	gPassPipeline->destroy();
	lightingPipeline->destroy();
}
#pragma endregion setup