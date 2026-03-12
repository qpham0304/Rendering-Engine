#include "DeferredRendererVulkan.h"
#include "core/features/ServiceLocator.h"
#include "graphics/renderers/RenderDevice.h"
#include "logging/Logger.h"
#include "window/AppWindow.h"
#include "core/events/EventManager.h"

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
#include <graphics/framework/vulkan/core/VulkanPipeline.h>
#include "graphics/framework/Vulkan/renderers/RenderDeviceVulkan.h"
#include <core/scene/SceneManager.h>
#include <imgui.h>

DeferredRendererVulkan::DeferredRendererVulkan()
{

}

DeferredRendererVulkan::~DeferredRendererVulkan()
{

}

bool DeferredRendererVulkan::init(WindowConfig config)
{
	Service::init(config);

	RenderDevice& renderDevice = ServiceLocator::GetService<RenderDevice>("RenderDeviceVulkan");
	renderDeviceVulkan = dynamic_cast<RenderDeviceVulkan*>(&renderDevice);

	BufferManager& bufferManager = ServiceLocator::GetService<BufferManager>("BufferManagerVulkan");
	bufferManagerVulkan = &static_cast<BufferManagerVulkan&>(bufferManager);
	DescriptorManager& descriptorManager = ServiceLocator::GetService<DescriptorManager>("DescriptorManagerVulkan");
	descriptorManagerVulkan = &static_cast<DescriptorManagerVulkan&>(descriptorManager);
	
	textureManager = &ServiceLocator::GetService<TextureManager>("TextureManagerVulkan");
	meshManager = &ServiceLocator::GetService<MeshManager>("MeshManager");
    materialManager = &ServiceLocator::GetService<MaterialManager>("MaterialManagerVulkan");
	modelManager = &ServiceLocator::GetService<ModelManager>("ModelManager");
	guiManager = &ServiceLocator::GetService<GuiManager>("ImGuiManager");

	if(!(
		renderDeviceVulkan &&
		bufferManagerVulkan &&
		descriptorManagerVulkan &&
		textureManager &&
		meshManager &&
		materialManager &&
		modelManager &&
		guiManager
	)) {
		return false;
	}

	shadowMapRenderer.init(config);
	imageBasedRenderer.init(config);
	_createRenderPasses();
	_createFrameBuffers();


	bufferManagerVulkan->createUniformBuffers(uniformbuffersList, sizeof(UniformBufferObject));
	
	instanceData.resize(10000);
	size_t bufferSize = 10000 * sizeof(StorageBufferObject);
	bufferManagerVulkan->createStorageBuffers(storagebuffersList, bufferSize);

	lights.reserve(numLights);
	size_t lightBufferSize = numLights * sizeof(LightSSBO);
	bufferManagerVulkan->createStorageBuffers(lightStoragebuffers, lightBufferSize);

	pushConstantLight.skyboxDetail = 1.0f;
	pushConstantLight.color = sunColor * sunIntensity;
	pushConstantLight.bias = 0.001f;
	pushConstantLight.alpha = 0.0001f;
    pushConstantLight.lintstepLow = 0.2f;
    pushConstantLight.linstepHigh = 1.0f;
    pushConstantLight.litBias = 0.0005f;

	_createDescriptor();
	_createPipelines();
	_createViewDescriptorSets();

	_createLightDescriptor();
	_createLightPipeline();


	return true;
}

bool DeferredRendererVulkan::onClose()
{
	renderDeviceVulkan->waitIdle();
	gPassPipeline->destroy();
	lightingPipeline->destroy();
	renderTarget.destroy(renderDeviceVulkan->device);
	
	shadowMapRenderer.onClose();
	imageBasedRenderer.onClose();

	return true;
}

void DeferredRendererVulkan::onUpdate()
{
	shadowMapRenderer.onUpdate();
}

void DeferredRendererVulkan::beginFrame()
{
	renderDeviceVulkan->beginFrame();
}

void DeferredRendererVulkan::endFrame()
{
	renderDeviceVulkan->endFrame();
}

void DeferredRendererVulkan::render(Camera& camera)
{
	cam = &camera;
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
			lights.push_back(LightSSBO(light.color, glm::vec4(transform.translateVec, 1.0), light.intensity));
        }
    }
	
    pushConstantLight.time = AppWindow::getTime();
	pushConstantLight.numLights = lights.size();
	
	shadowMapRenderer.render(camera);

	VkCommandBuffer cmdBuffer = renderDeviceVulkan->commandPool.currentBuffer();
	uint32_t currentFrame = renderDeviceVulkan->getCurrentFrameIndex();
	
	imageBasedRenderer.onUpdate();
	imageBasedRenderer.computeSH(cmdBuffer, currentFrame);
	imageBasedRenderer.computePrefilter(cmdBuffer, currentFrame);
	// VkImageMemoryBarrier globalBarrier = {};
	// vkCmdPipelineBarrier(
	// 	cmdBuffer,
	// 	VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
	// 	VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
	// 	0, 0, nullptr, 0, nullptr, 1, nullptr
	// );

	ubo.view = camera.getViewMatrix();
	ubo.proj = camera.getProjectionMatrix();
	ubo.cameraPos = glm::vec4(camera.getPosition(), 1.0);
	ubo.proj[1][1] *= -1;
	ubo.invView = camera.getInViewMatrix();
	ubo.invProj = camera.getInProjectionMatrix();
	ubo.invProj[1][1] *= -1;
	ubo.width = AppWindow::getWidth();
	ubo.height = AppWindow::getHeight();
	
	pushConstantLight.color = sunColor * sunIntensity;
	pushConstantLight.direction = glm::vec4(shadowMapRenderer.lightDir, 0.0f);
	pushConstantLight.sunlightMVP = shadowMapRenderer.lightSpaceMatrix;

	uint32_t frame = renderDeviceVulkan->getCurrentFrameIndex();
	uniformbuffersList[frame]->update(&ubo, sizeof(ubo));

	StorageBufferVulkan* ssbo = storagebuffersList[frame];
	ssbo->update(instanceData.data(), instanceData.size() * sizeof(StorageBufferObject));

	StorageBufferVulkan* lightSSBO = lightStoragebuffers[frame];
	lightSSBO->update(lights.data(), lights.size() * sizeof(LightSSBO));

	recordDrawCommand(cmdBuffer, renderDeviceVulkan->getImageIndex());

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
	if(ImGui::Button("Chane Environment")) {
		std::string path;
		path = Utils::fileDialog();
		imageBasedRenderer.loadTexture(path);
	}
	ImGui::SliderFloat("skybox detail", &pushConstantLight.skyboxDetail, 0.0f, 1.0f);
	ImGui::Checkbox("Light Perspective", &shadowMapRenderer.useOrtho);
	ImGui::ColorEdit4("color", &sunColor[0]);
	ImGui::SliderFloat("intensity", &sunIntensity, 1.0f, 15.0f);
	ImGui::SliderFloat("Bias", &pushConstantLight.bias, 0.001f, 0.1f);
	ImGui::SliderFloat("Alpha", &pushConstantLight.alpha, 0.0001f, 0.01f);
	ImGui::SliderFloat("Lintstep Low", &pushConstantLight.lintstepLow, 0.01f, 1.0f);
	ImGui::SliderFloat("Lintstep High", &pushConstantLight.linstepHigh, 0.01f, 2.0f);
	ImGui::SliderFloat("Lit Bias", &pushConstantLight.litBias, 0.0001f, 0.01f, "%.4f", ImGuiSliderFlags_Logarithmic);

	int i = 0;
	for (auto& entity : entities) {
		TransformComponent& transform = entity.getComponent<TransformComponent>();
		LightComponent& light = entity.getComponent<LightComponent>();

		if (ImGui::CollapsingHeader(entity.getComponent<NameComponent>().name.c_str())) {
			ImGui::SliderFloat4("Position", &transform.translateVec[0], -50.0, 50.0);
			ImGui::ColorEdit4("Color", &light.color[0]);
			ImGui::SliderFloat("Intensity Multiplier", &light.intensity, 1.0f, 1000.0f);
		}

		lights[i].color = light.color;
		lights[i].position = glm::vec4(transform.translateVec, 1.0f);
		lights[i].intensity = light.intensity * transform.scaleVec.x;

		i++;
	}
	ImGui::End();

	ImGui::Begin("G-Buffer Debug");
	uint32_t frameIdx = renderDeviceVulkan->getImageIndex();
	int numFrames = 3; 
	int numGroups = imGuisetIDs.size() / numFrames;


	const char* names[] = { "Final", "Position", "Albedo", "PBR", "Normals" };

	for (int i = 0; i < numGroups; i++) {
		int index = (i * numFrames) + frameIdx;

		// ensure we don't exceed the vector size
		if (index < imGuisetIDs.size()) {
			VkDescriptorSet descSet = descriptorManagerVulkan->getDescriptorSet(imGuisetIDs[index])[0];

			ImGui::Text("%s", names[i]);
			ImGui::Image(reinterpret_cast<ImTextureID>(descSet), ImVec2(256, 144));
		}
	}

	ImGui::Text("DEPTH MAP", names[i]);
	ImGui::Image(reinterpret_cast<ImTextureID>(shadowMapRenderer.imGuiDescriptorSet), ImVec2(256, 256));
	
	// void* irradianceImage = textureManager->inspectTexture(imageBasedRenderer.hdrImageID);
	// ImGui::Image(reinterpret_cast<ImTextureID>(irradianceImage), ImVec2(256, 256));

	//ImGui::SliderFloat4("Sunlight Direction", &pushConstantLight.direction[0], -1.0f, 1.0f);
	//ImGui::SliderFloat4("Sunlight Color", &pushConstantLight.color[0], 0.0f, 1.0f);


	ImGui::End();
}

void DeferredRendererVulkan::recordDrawCommand(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
	beginRecording(
		commandBuffer,
		renderTarget.renderPass,
		renderTarget.framebuffers[imageIndex]
	);


	gPassPipeline->bind(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS);

	renderDeviceVulkan->setViewport();
	renderDeviceVulkan->setScissor();

	uint32_t currentFrame = renderDeviceVulkan->getCurrentFrameIndex();
	auto descriptorSets = descriptorManagerVulkan->getDescriptorSet(setsID);
	vkCmdBindDescriptorSets(
		commandBuffer,
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
		// ubo.invNormal = glm::transpose(glm::inverse(glm::mat4(entityTransform)));

		if (entity.hasComponent<LightComponent>()) {
			LightComponent& lightComponent = entity.getComponent<LightComponent>();
			lights[lightIndex].color = lightComponent.color;
			lights[lightIndex].position = glm::vec4(translation, 1.0);
			lights[lightIndex].intensity = transform.scaleVec.x;
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
				materialManager->bindMaterial(mesh->materialID, commandBuffer, (void*)gPassPipeline.get());
				meshManager->bindMesh(meshID);

				uint32_t indexCount = static_cast<uint32_t>(mesh->indices.size());
				renderDeviceVulkan->draw(indexCount, numInstances, index);
			}
		} 
		
		if (entity.hasComponent<MeshComponent>()) {
			MeshComponent meshComponent = entity.getComponent<MeshComponent>();
			for (uint32_t meshID : meshComponent.meshIDs) {
				const Mesh* mesh = meshManager->getMesh(meshID);
				materialManager->bindMaterial(mesh->materialID, commandBuffer, (void*)gPassPipeline.get());
				meshManager->bindMesh(meshID);

				uint32_t indexCount = static_cast<uint32_t>(mesh->indices.size());
				renderDeviceVulkan->draw(indexCount, numInstances, index);
			}
		}
		
		index++;
	}

	// --- TRANSITION ---
    vkCmdNextSubpass(commandBuffer, VK_SUBPASS_CONTENTS_INLINE);

    // --- SUBPASS 1 (Lighting) ---
    lightingPipeline->bind(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS);


    auto lightSets = descriptorManagerVulkan->getDescriptorSet(lightSetsID);
    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        lightingPipeline->pipelineLayout,
        0, 
		1, 
		&lightSets[currentFrame],
        0, nullptr
    );

	auto lightDescriptorSets_1 = descriptorManagerVulkan->getDescriptorSet(lightSetsID_1);
	vkCmdBindDescriptorSets(
		commandBuffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		lightingPipeline->pipelineLayout,
		1,
		1,
		&lightDescriptorSets_1[currentFrame],
		0,
		nullptr
	);

	vkCmdPushConstants(
		commandBuffer,
		lightingPipeline->pipelineLayout,
		VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		0,
		sizeof(PushConstantLight),
		&pushConstantLight
	);

    vkCmdDraw(commandBuffer, 3, 1, 0, 0);

	endRecording(commandBuffer);
}

void DeferredRendererVulkan::beginRecording(void* cmdBuffer, void* renderPass, void* frameBuffer)
{
	uint32_t imageIndex = renderDeviceVulkan->getImageIndex();
	VkCommandBuffer commandBuffer = static_cast<VkCommandBuffer>(cmdBuffer);
	VkRenderPass vulkanRenderPass = static_cast<VkRenderPass>(renderPass);
	VkFramebuffer vulkanFrameBuffer = static_cast<VkFramebuffer>(frameBuffer);

	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = vulkanRenderPass;
	renderPassInfo.framebuffer = vulkanFrameBuffer;
	renderPassInfo.renderArea.offset = { 0, 0 };
	renderPassInfo.renderArea.extent = renderDeviceVulkan->swapchain.swapChainExtent;


	std::array<VkClearValue, 6> clearValues{};
	clearValues[0].color = { 0.15f, 0.15f, 0.15f, 1.0f }; 	// Final Swapchain
	clearValues[1].color = { 0.0f, 0.0f, 0.0f, 1.0f };    	// Position
	clearValues[2].color = { 0.0f, 0.0f, 0.0f, 1.0f };    	// Normal
	clearValues[3].color = { 0.0f, 0.0f, 0.0f, 1.0f };    	// Albedo
	clearValues[4].color = { 1.0f, 1.0f, 0.0f, 1.0f };    	// PBR (ORM)
	clearValues[5].depthStencil = { 1.0f, 0 };            								// Depth (Index 5)

	renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	renderPassInfo.pClearValues = clearValues.data();

	renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	renderPassInfo.pClearValues = clearValues.data();

	//basic draw commands
	vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void DeferredRendererVulkan::endRecording(void* cmdBuffer)
{
	VkCommandBuffer commandBuffer = static_cast<VkCommandBuffer>(cmdBuffer);

	vkCmdEndRenderPass(commandBuffer);
}

#pragma region setup
void DeferredRendererVulkan::_createRenderPasses()
{
	VulkanSwapChain& swapchain = renderDeviceVulkan->swapchain;
	VkDevice device = renderDeviceVulkan->device;

	VkAttachmentDescription colorAttachment{};
	colorAttachment.format = swapchain.swapChainImageFormat;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;  	// Clear the texture before drawing
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // Save the results
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // transitions the image for use in the NEXT shader automatically

	VkAttachmentDescription gBufferPos{};
	gBufferPos.format = VK_FORMAT_R16G16B16A16_SFLOAT;		// High precision for positions later move to depth reconstruction
	gBufferPos.samples = VK_SAMPLE_COUNT_1_BIT;
	gBufferPos.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	gBufferPos.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; 	// Don't need this after the pass!
	gBufferPos.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	gBufferPos.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkAttachmentDescription gBufferNorm{};
	gBufferNorm.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	gBufferNorm.samples = VK_SAMPLE_COUNT_1_BIT;
	gBufferNorm.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	gBufferNorm.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
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

	VkAttachmentDescription depthAttachment{};
	depthAttachment.format = TextureManagerVulkan::findDepthFormat(renderDeviceVulkan->device);
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	// --- SUBPASS 0: G-Buffer Generation ---
	std::vector<VkAttachmentReference> gBufferReferences = {
		{1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},	// Position
		{2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},	// Normal
		{3, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},	// Albedo
		{4, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}	// PBR
	};

	VkAttachmentReference depthReference = {5, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

	VkSubpassDescription subpass0{};
	subpass0.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass0.colorAttachmentCount = gBufferReferences.size();
	subpass0.pColorAttachments = gBufferReferences.data();
	subpass0.pDepthStencilAttachment = &depthReference;

	// These tell the shader to use 'subpassInput' to read from the G-Buffer
	std::vector<VkAttachmentReference> inputReferences = {
		{1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
		{2, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
		{3, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
		{4, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}
	};

	// --- SUBPASS 1: Lighting ---
	VkAttachmentReference colorAttachmentRef{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

	VkSubpassDescription subpass1{};
	subpass1.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass1.colorAttachmentCount = 1;
	subpass1.pColorAttachments = &colorAttachmentRef;
	subpass1.inputAttachmentCount = inputReferences.size();
	subpass1.pInputAttachments = inputReferences.data();
	subpass1.pDepthStencilAttachment = &depthReference;

	// ZERO INITIALIZE or validation layer will complain
	std::array<VkSubpassDependency, 3> dependencies{};

	// 1. Wait for swapchain to be ready
	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0; // The Geometry Pass
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[0].srcAccessMask = 0;
	dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	// 2. Wait for Subpass 0 (G-Buffer) to finish before Subpass 1 (Lighting) reads
	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = 1;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	dependencies[1].dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
	dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	dependencies[2].srcSubpass = 1; // Lighting Subpass
	dependencies[2].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[2].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[2].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	dependencies[2].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[2].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	dependencies[2].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	std::array<VkAttachmentDescription, 6> allAttachments = { 
		colorAttachment,
		gBufferPos,
		gBufferNorm,
		gBufferAlbedo,
		gBufferPBR,
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
	
	renderTarget.colorTextures.resize(swapchain.swapChainImages.size());
	renderTarget.gBufferPos.resize(swapchain.swapChainImages.size());
	renderTarget.gBufferNorm.resize(swapchain.swapChainImages.size());
	renderTarget.gBufferAlbedo.resize(swapchain.swapChainImages.size());
	renderTarget.gPBR.resize(swapchain.swapChainImages.size());
	renderTarget.depthTextures.resize(swapchain.swapChainImages.size());
	renderTarget.framebuffers.resize(swapchain.swapChainImageViews.size());

	for(size_t i = 0; i < renderTarget.colorTextures.size(); i++) {
		auto createTexture = [&] (VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspect) -> TextureVulkan* {
			uint32_t id = textureManager->createTexture();
			auto* texture = static_cast<TextureVulkan*>(textureManager->getTexture(id));

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

			return texture;
		};

		// for the output image
		renderTarget.colorTextures[i] = createTexture(
			swapchain.swapChainImageFormat, 
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 
			VK_IMAGE_ASPECT_COLOR_BIT
		);

		//TODO: for g-buffer positions image
		renderTarget.gBufferPos[i] = createTexture(
			VK_FORMAT_R16G16B16A16_SFLOAT, 
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, 
			VK_IMAGE_ASPECT_COLOR_BIT
		);

		//TODO: for g-buffer normals image
		renderTarget.gBufferNorm[i] = createTexture(
			VK_FORMAT_R16G16B16A16_SFLOAT,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, 
			VK_IMAGE_ASPECT_COLOR_BIT
		);

		// TODO: for g-buffer albedo image
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

		VkFormat depthFormat = TextureManagerVulkan::findDepthFormat(renderDeviceVulkan->device);

		uint32_t depthId = textureManager->createTexture();
		renderTarget.depthTextures[i] = static_cast<TextureVulkan*>(textureManager->getTexture(depthId));

		TextureManagerVulkan::createImage(
			swapchain.swapChainExtent.width,
			swapchain.swapChainExtent.height,
			depthFormat,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
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

		std::array<VkImageView, 6> attachments = {
			renderTarget.colorTextures[i]->textureImageView,
			renderTarget.gBufferPos[i]->textureImageView,
			renderTarget.gBufferNorm[i]->textureImageView,
			renderTarget.gBufferAlbedo[i]->textureImageView,
			renderTarget.gPBR[i]->textureImageView,
			renderTarget.depthTextures[i]->textureImageView
		};

		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = renderTarget.renderPass;
		framebufferInfo.attachmentCount = attachments.size();
		framebufferInfo.pAttachments = attachments.data();
		framebufferInfo.width = swapchain.swapChainExtent.width;
		framebufferInfo.height = swapchain.swapChainExtent.height;
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &renderTarget.framebuffers[i]) != VK_SUCCESS) {
			throw std::runtime_error("failed to create offscreen framebuffer!");
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
	PipelineConfigInfo gBufferConfig = VulkanPipeline::defaultPipelineConfigInfo(4);
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
		"assets/shaders/gBuffer.vert.spv", 
		"assets/shaders/gBuffer.frag.spv", 
		gBufferConfig, 
		vertexInputInfo, 
		layouts, 
		0
	);
}

void DeferredRendererVulkan::_createViewDescriptorSets()
{
	const uint32_t MAX_NUM_SETS = 24;	// add more if requires more
	
	VkDescriptorSetLayoutBinding samplerLayoutBinding{};
	samplerLayoutBinding.binding = 0;
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	std::vector<VkDescriptorSetLayoutBinding> bindings = { samplerLayoutBinding };
	imGuilayoutID = descriptorManagerVulkan->createLayout(bindings);

	std::vector<VkDescriptorPoolSize> poolSizes = { 
		{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}
	};
	imGuipoolID = descriptorManagerVulkan->createPool(poolSizes, MAX_NUM_SETS);

	for (int i = 0; i < renderTarget.gBufferPos.size(); i++) {
		imGuisetIDs.push_back(descriptorManagerVulkan->createSets(imGuilayoutID, imGuipoolID, 1));

		uint32_t currentID = imGuisetIDs.back();
		VkDescriptorSet imGuiDescriptorSet = descriptorManagerVulkan->getDescriptorSet(currentID)[0];

		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = renderTarget.gBufferPos[i]->textureImageView;
		imageInfo.sampler = renderTarget.gBufferPos[i]->textureSampler;

		std::vector<VkWriteDescriptorSet> writes = {};
		descriptorManagerVulkan->writeImage(&writes, imGuiDescriptorSet, 0, imageInfo);
		descriptorManagerVulkan->updateDescriptorSets(&writes);
	}
		
	for(int i = 0; i < renderTarget.gBufferPos.size(); i++) {
		imGuisetIDs.push_back(descriptorManagerVulkan->createSets(imGuilayoutID, imGuipoolID, 1));
		
		uint32_t currentID = imGuisetIDs.back();
		VkDescriptorSet imGuiDescriptorSet = descriptorManagerVulkan->getDescriptorSet(currentID)[0];

		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = renderTarget.gBufferPos[i]->textureImageView;
		imageInfo.sampler = renderTarget.gBufferPos[i]->textureSampler;

		std::vector<VkWriteDescriptorSet> writes = {};
		descriptorManagerVulkan->writeImage(&writes, imGuiDescriptorSet, 0, imageInfo);
		descriptorManagerVulkan->updateDescriptorSets(&writes);
	}
	
	for(int i = 0; i < renderTarget.gBufferAlbedo.size(); i++) {
		imGuisetIDs.push_back(descriptorManagerVulkan->createSets(imGuilayoutID, imGuipoolID, 1));
		
		uint32_t currentID = imGuisetIDs.back();
		VkDescriptorSet imGuiDescriptorSet = descriptorManagerVulkan->getDescriptorSet(currentID)[0];

		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = renderTarget.gBufferAlbedo[i]->textureImageView;
		imageInfo.sampler = renderTarget.gBufferAlbedo[i]->textureSampler;

		std::vector<VkWriteDescriptorSet> writes = {};
		descriptorManagerVulkan->writeImage(&writes, imGuiDescriptorSet, 0, imageInfo);
		descriptorManagerVulkan->updateDescriptorSets(&writes);
	}

	for (int i = 0; i < renderTarget.gPBR.size(); i++) {
		imGuisetIDs.push_back(descriptorManagerVulkan->createSets(imGuilayoutID, imGuipoolID, 1));
		
		uint32_t currentID = imGuisetIDs.back();
		VkDescriptorSet imGuiDescriptorSet = descriptorManagerVulkan->getDescriptorSet(currentID)[0];

		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = renderTarget.gPBR[i]->textureImageView;
		imageInfo.sampler = renderTarget.gPBR[i]->textureSampler;

		std::vector<VkWriteDescriptorSet> writes = {};
		descriptorManagerVulkan->writeImage(&writes, imGuiDescriptorSet, 0, imageInfo);
		descriptorManagerVulkan->updateDescriptorSets(&writes);
	}

	
	for(int i = 0; i < renderTarget.gBufferNorm.size(); i++) {
		imGuisetIDs.push_back(descriptorManagerVulkan->createSets(imGuilayoutID, imGuipoolID, 1));
		
				uint32_t currentID = imGuisetIDs.back();
		VkDescriptorSet imGuiDescriptorSet = descriptorManagerVulkan->getDescriptorSet(currentID)[0];

		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = renderTarget.gBufferNorm[i]->textureImageView;
		imageInfo.sampler = renderTarget.gBufferNorm[i]->textureSampler;

		std::vector<VkWriteDescriptorSet> writes = {};
		descriptorManagerVulkan->writeImage(&writes, imGuiDescriptorSet, 0, imageInfo);
		descriptorManagerVulkan->updateDescriptorSets(&writes);
	}
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
	lightConfig.subpass = 1; // CRITICAL: Target the second subpass
	lightConfig.depthStencilInfo.depthTestEnable = VK_FALSE;
	lightConfig.depthStencilInfo.depthWriteEnable = VK_FALSE;

	lightingPipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
	lightingPipeline->createGraphicsPipeline(
		"assets/shaders/lightPass.vert.spv",
		"assets/shaders/lightPass.frag.spv",
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
    };
    lightLayoutID = descriptorManagerVulkan->createLayout(bindings);
    
    // Over-allocating to 100 to prevent any "tight fit" crashes
    uint32_t frameCount = VulkanUtils::numFrames();
    std::vector<VkDescriptorPoolSize> poolSizes{
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 100} 
    };

    uint32_t lightPoolID = descriptorManagerVulkan->createPool(poolSizes, 100);    // Create pool with maxSets = 100

    lightSetsID = descriptorManagerVulkan->createSets(lightLayoutID, lightPoolID, frameCount);
    auto lightingDescriptorSets = descriptorManagerVulkan->getDescriptorSet(lightSetsID);
    
    for(int i = 0; i < frameCount; i++) {
		VkDescriptorImageInfo posInfo = { VK_NULL_HANDLE, renderTarget.gBufferPos[i]->textureImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
		VkDescriptorImageInfo normInfo = { VK_NULL_HANDLE, renderTarget.gBufferNorm[i]->textureImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
		VkDescriptorImageInfo albedoInfo = { VK_NULL_HANDLE, renderTarget.gBufferAlbedo[i]->textureImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
		VkDescriptorImageInfo pbrInfo = { VK_NULL_HANDLE, renderTarget.gPBR[i]->textureImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };

		std::array<VkWriteDescriptorSet, 4> writes{};
        
        writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet = lightingDescriptorSets[i];
        writes[0].dstBinding = 0; // Position
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        writes[0].descriptorCount = 1;
        writes[0].pImageInfo = &posInfo;

        writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet = lightingDescriptorSets[i];
        writes[1].dstBinding = 1; // Normal
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        writes[1].descriptorCount = 1;
        writes[1].pImageInfo = &normInfo;

        writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[2].dstSet = lightingDescriptorSets[i];
        writes[2].dstBinding = 2; // Albedo
        writes[2].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        writes[2].descriptorCount = 1;
        writes[2].pImageInfo = &albedoInfo;

		writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[3].dstSet = lightingDescriptorSets[i];
		writes[3].dstBinding = 3;
		writes[3].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		writes[3].descriptorCount = 1;
		writes[3].pImageInfo = &pbrInfo;

        vkUpdateDescriptorSets(device, writes.size(), writes.data(), 0, nullptr);
    }


	lightLayoutID_1 = descriptorManagerVulkan->createLayout({
		{ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
		{ 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
		{ 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
		{ 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
		{ 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
		{ 5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
		{ 6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
		{ 7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
	});

	uint32_t poolIDLayout_1 = descriptorManagerVulkan->createPool(
		{
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frameCount },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, frameCount * 2},
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, frameCount * 5 },
		},
		frameCount
	);

	lightSetsID_1 = descriptorManagerVulkan->createSets(lightLayoutID_1, poolIDLayout_1, VulkanUtils::numFrames());
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
		imageInfo.imageView = shadowMapRenderer.depthMap->textureImageView;
		imageInfo.sampler = shadowMapRenderer.depthMap->textureSampler;
		// imageInfo.imageView = shadowMapRenderer.momentImage->textureImageView;
		// imageInfo.sampler = shadowMapRenderer.momentImage->textureSampler;

		VkDescriptorImageInfo noiseImageInfo{};
		noiseImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		noiseImageInfo.imageView = shadowMapRenderer.blueNoiseImage->textureImageView;
		noiseImageInfo.sampler = shadowMapRenderer.blueNoiseImage->textureSampler;

		VkDescriptorBufferInfo bufferInfoSH{};
		bufferInfoSH.buffer = static_cast<VkBuffer>(*imageBasedRenderer.finalSumBuffers[i]);
		bufferInfoSH.offset = 0;
		bufferInfoSH.range = VK_WHOLE_SIZE;

		VkDescriptorImageInfo brdfLutImageInfo{};
		brdfLutImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		brdfLutImageInfo.imageView = imageBasedRenderer.brdfLUT->textureImageView;
		brdfLutImageInfo.sampler = imageBasedRenderer.brdfLUT->textureSampler;

		VkDescriptorImageInfo prefilterImageInfo{};
		prefilterImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		prefilterImageInfo.imageView = imageBasedRenderer.prefilterMap->textureImageView;
		prefilterImageInfo.sampler = imageBasedRenderer.prefilterMap->textureSampler;

		VkDescriptorImageInfo hdrImageInfo{};
		hdrImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		hdrImageInfo.imageView = imageBasedRenderer.hdrImage->textureImageView;
		hdrImageInfo.sampler = imageBasedRenderer.hdrImage->textureSampler;

		std::vector<VkWriteDescriptorSet> writes = {};
		descriptorManagerVulkan->writeUniform(&writes, descriptorSets[i], 0, bufferInfo);
		descriptorManagerVulkan->writeStorage(&writes, descriptorSets[i], 1, ssboInfo);
		descriptorManagerVulkan->writeImage(&writes, descriptorSets[i], 2, imageInfo);
		descriptorManagerVulkan->writeImage(&writes, descriptorSets[i], 3, noiseImageInfo);
		descriptorManagerVulkan->writeStorage(&writes, descriptorSets[i], 4, bufferInfoSH);
		descriptorManagerVulkan->writeImage(&writes, descriptorSets[i], 5, brdfLutImageInfo);
		descriptorManagerVulkan->writeImage(&writes, descriptorSets[i], 6, prefilterImageInfo);
		descriptorManagerVulkan->writeImage(&writes, descriptorSets[i], 7, hdrImageInfo);
		descriptorManagerVulkan->updateDescriptorSets(&writes);
	}
}
#pragma endregion setup