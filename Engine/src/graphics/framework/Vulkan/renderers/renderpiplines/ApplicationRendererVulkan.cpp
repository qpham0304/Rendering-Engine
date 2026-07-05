#include "ApplicationRendererVulkan.h"
#include "logging/Logger.h"
#include "core/events/EventManager.h"
#include "gui/GuiManager.h"
#include "core/features/ServiceLocator.h"
#include "core/features/Camera.h"
#include "window/AppWindow.h"
#include "core/features/Mesh.h"

#include <core/resources/managers/TextureManager.h>
#include <core/resources/managers/MeshManager.h>
#include <core/resources/managers/ModelManager.h>
#include <core/resources/managers/DescriptorManager.h>

#include <graphics/renderers/RenderDevice.h>
#include <graphics/framework/Vulkan/renderers/RenderDeviceVulkan.h>
#include <graphics/framework/Vulkan/resources/textures/TextureVulkan.h>
#include <graphics/framework/Vulkan/resources/descriptors/DescriptorManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/materials/MaterialManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/textures/TextureManagerVulkan.h>
#include <graphics/framework/Vulkan/renderers/RendererManagerVulkan.h>
#include <graphics/framework/vulkan/core/VulkanPipeline.h>
#include <graphics/framework/Vulkan/renderers/renderpiplines/ForwardRendererVulkan.h>
#include <graphics/framework/vulkan/renderers/renderpiplines/DeferredRendererVulkan.h>
#include <graphics/framework/vulkan/renderers/renderpasses/BloomPassVulkan.h>
#include <core/scene/SceneManager.h>
#include "imgui.h" // TODO: remove it once done

ApplicationRendererVulkan::ApplicationRendererVulkan(std::string serviceName) 
	:	RendererVulkan(serviceName)
{

}

ApplicationRendererVulkan::~ApplicationRendererVulkan()
{

}

bool ApplicationRendererVulkan::init(WindowConfig config)
{
	RendererVulkan::init(config);

	EventManager::getInstance().subscribe(EventType::KeyPressed, [&](Event& event) {
		KeyPressedEvent& keyPressedEvent = static_cast<KeyPressedEvent&>(event);
		if (keyPressedEvent.keyCode == KEY_F11) {
			showGui = !showGui;
			
			GuiManager* guiManager = &ServiceLocator::GetService<GuiManager>("ImGuiManager");
			if(guiManager) {
				guiManager->setActive(showGui);
			}
		}
	});

	_createPipeline();

	return true;
}

bool ApplicationRendererVulkan::onClose()
{	
	appPipeline->destroy();

	return true;
}

void ApplicationRendererVulkan::onUpdate()
{
}

void ApplicationRendererVulkan::render(Camera& camera)
{
	// stop rendering as we can't record begin/endRecording because the manager's 
	// command Buffer recording state is likely corrupted by the destruction inside
	//  _recreateResources By returning, we let the manager call endFrame on an empty buffer
    RendererVulkan::_resize();
	
	if(rendererManagerVulkan->getDisplayImage()) {
		pushConstantData.index = rendererManagerVulkan->getDisplayImage()->id();
	} else {
		m_logger->warn("Application has no image to render");
	}
	
    VkCommandBuffer cmd = renderDeviceVulkan->commandPool.currentBuffer();
    renderDeviceVulkan->beginLabel(cmd, "Application Pass");
	recordDrawCommand(cmd, renderDeviceVulkan->getImageIndex());
    renderDeviceVulkan->endLabel(cmd);
}

void ApplicationRendererVulkan::recordDrawCommand(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
	Timer timer("CPU render submission time", true);

    TextureVulkan* texture = rendererManagerVulkan->getDisplayImage();
    if (!showGui && !texture) {
        beginRecording(commandBuffer, renderDeviceVulkan->swapchain.renderPass, 
                       renderDeviceVulkan->swapchain.currentFrameBuffer());
        endRecording(commandBuffer);
        return; 
    }

    if (texture && texture->textureImageView != lastView) {
		renderDeviceVulkan->waitIdle(); 
    
        lastView = texture->textureImageView;
    }

	beginRecording(commandBuffer, renderDeviceVulkan->swapchain.renderPass, renderDeviceVulkan->swapchain.currentFrameBuffer());
	{
		if(showGui){
			renderDeviceVulkan->beginLabel(commandBuffer, "Render to Gui Image Mode");
			renderGui(commandBuffer);
			renderDeviceVulkan->endLabel(commandBuffer);
		} 
		else {
			renderDeviceVulkan->beginLabel(commandBuffer, "Render To Swap Chain Mode");
			// vkCmdBindDescriptorSets(
			// 	commandBuffer,
			// 	VK_PIPELINE_BIND_POINT_GRAPHICS,
			// 	appPipeline->pipelineLayout,
			// 	0,
			// 	1,
			// 	&descriptorSets[renderDeviceVulkan->getCurrentFrameIndex()],
			// 	0,
			// 	nullptr
			// );

			vkCmdBindDescriptorSets(
				commandBuffer,
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				appPipeline->pipelineLayout,
				0, 
				1, 
				&descriptorManagerVulkan->getDescriptorSet(textureManagerVulkan->getBindlessSet())[0],
				0, 
				nullptr
			);

			vkCmdPushConstants(
				commandBuffer,
				appPipeline->pipelineLayout,
				VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
				0,
				sizeof(PushConstantData),
				&pushConstantData
			);

			vkCmdDraw(commandBuffer, 3, 1, 0, 0);
			renderDeviceVulkan->endLabel(commandBuffer);
		}
	}
	endRecording(commandBuffer);
}

void ApplicationRendererVulkan::beginRecording(void* cmdBuffer, void* renderPass, void* frameBuffer)
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
	// renderPassInfo.renderArea.extent = { AppWindow::getWidth(), AppWindow::getHeight() };


	std::array<VkClearValue, 2> clearValues{};
	clearValues[0].color = { 0.15f, 0.15f, 0.15f, 1.0f };
	clearValues[1].depthStencil = { 1.0f, 0 };

	renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	renderPassInfo.pClearValues = clearValues.data();

	vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

	appPipeline->bind(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS);

	renderDeviceVulkan->setViewport();
	renderDeviceVulkan->setScissor();
}

void ApplicationRendererVulkan::endRecording(void* cmdBuffer)
{
	vkCmdEndRenderPass(static_cast<VkCommandBuffer>(cmdBuffer));
}

void ApplicationRendererVulkan::renderGui(void* commandBuffer)
{
	// RendererVulkan* renderer = nullptr;
	// renderer = rendererManagerVulkan->getRenderer("ShadowMapPassVulkan");
	// auto shadowMapRenderer = dynamic_cast<ShadowMapPassVulkan*>(renderer);
	// renderer = rendererManagerVulkan->getRenderer("ImageBasedRendererVulkan");
	// auto imageBasedRenderer = dynamic_cast<ImageBasedRendererVulkan*>(renderer);
	// renderer = rendererManagerVulkan->getRenderer("ForwardRendererVulkan");
	// auto forwardRendererVulkan = dynamic_cast<ForwardRendererVulkan*>(renderer);
	// renderer = rendererManagerVulkan->getRenderer("DeferredRendererVulkan");
	// auto deferredRendererVulkan = dynamic_cast<DeferredRendererVulkan*>(renderer);
	// renderer = rendererManagerVulkan->getRenderer("BloomPassVulkan");
	// auto bloomPassRendererVulkan = dynamic_cast<BloomPassVulkan*>(renderer);

	// assert(shadowMapRenderer && imageBasedRenderer && 
	// 	forwardRendererVulkan && deferredRendererVulkan && 
	// 	"failed to retrieve renderer"
	// );

	guiManager->start();
	
	// //TODO: temporarily use imgui renderer, abstract to gui service and remove these
	int currentMode = rendererManagerVulkan->getRenderMode(); 
	if(currentMode == 1) {
	// 	// deferredRendererVulkan->renderGui();
	// 	// bloomPassRendererVulkan->renderGui();
	}

	ImGui::Begin("Application");
	ImGui::BeginChild("Application View");
	uint32_t currentFrame = renderDeviceVulkan->getCurrentFrameIndex();
	ImVec2 size = ImGui::GetContentRegionAvail();
	TextureVulkan* displayImage = rendererManagerVulkan->getDisplayImage();
	if(displayImage) {
		ImGui::Image((ImTextureID)textureManagerVulkan->inspectTexture(displayImage->id()), size);
	} else {
		ImGui::Dummy(size);
	}

	ImVec2 wsize = ImGui::GetWindowSize();
	int wWidth = static_cast<int>(ImGui::GetWindowWidth());
	int wHeight = static_cast<int>(ImGui::GetWindowHeight());
	if (SceneManager::cameraController->getViewWidth() != wWidth || SceneManager::cameraController->getViewHeight() != wHeight) {
		SceneManager::cameraController->updateViewResize(wWidth, wHeight);
	}

	if (ImGui::IsItemHovered() && ImGui::IsWindowFocused()) {
		guiManager->setEditorFocus(true);
		GuiFocusEvent event(true);
		EventManager::getInstance().publish(event);
	} 
	else{
		guiManager->setEditorFocus(false);
		GuiFocusEvent event(false);
		EventManager::getInstance().publish(event);
	}

	SceneManager& sceneManager = SceneManager::getInstance();
	Scene* scene = sceneManager.getActiveScene();
	if(scene){
 		const std::vector<Entity>& entities = scene->getSelectedEntities();
 		if(!entities.empty()) {
 			const Entity& entity = entities[0];
 			TransformComponent& transform = entity.getComponent<TransformComponent>();
			guiManager->renderGuizmo(transform);
 		}
	}

	ImGui::EndChild();
	ImGui::End();

	ImGui::Begin("Render Mode");
	const char* modeNames[] = { "Forward", "Deferred", "Ray Traced" };
	const char* previewValue = modeNames[currentMode];

	if (ImGui::BeginCombo("Current Mode", previewValue)) {
		for (int n = 0; n <= 2; n++) {
			const bool isSelected = (currentMode == n);
			
			if (ImGui::Selectable(modeNames[n], isSelected)) {
				rendererManagerVulkan->setRenderMode(n);
			}

			if (isSelected) {
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}
	ImGui::End();

	guiManager->render(commandBuffer);
	guiManager->end();
}

void ApplicationRendererVulkan::_createPipeline()
{
	void* handle = materialManager->getMaterialLayout();
	VkDescriptorSetLayout materialLayout = reinterpret_cast<VkDescriptorSetLayout>(handle);

	uint32_t bindlessLayoutID = textureManagerVulkan->getBindlessTextureLayout();
	auto bindlessLayout = descriptorManagerVulkan->getDescriptorLayout(bindlessLayoutID);


	VkPipelineVertexInputStateCreateInfo emptyVertexInput{};
	emptyVertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	emptyVertexInput.vertexAttributeDescriptionCount = 0;
	emptyVertexInput.pVertexAttributeDescriptions = nullptr;
	emptyVertexInput.vertexBindingDescriptionCount = 0;
	emptyVertexInput.pVertexBindingDescriptions = nullptr;

	PipelineConfigInfo pipelineConfig = VulkanPipeline::defaultPipelineConfigInfo(1);
	pipelineConfig.renderPass = renderDeviceVulkan->swapchain.renderPass;
	pipelineConfig.subpass = 0;
	pipelineConfig.depthStencilInfo.depthTestEnable = VK_FALSE;
	pipelineConfig.depthStencilInfo.depthWriteEnable = VK_FALSE;
	
	appPipeline = std::make_unique<VulkanPipeline>(renderDeviceVulkan->device);
	appPipeline->createGraphicsPipeline(
		"assets/shaders/spv/default.vert.spv",
		"assets/shaders/spv/default.frag.spv",
		pipelineConfig,
		emptyVertexInput,
		{ bindlessLayout }, 
		sizeof(PushConstantData)
	);
}

void ApplicationRendererVulkan::_recreateResources()
{
	renderDeviceVulkan->waitIdle();

	_cleanupResources();
	_createPipeline();
}

void ApplicationRendererVulkan::_cleanupResources()
{
	appPipeline->destroy();
}