
#include "ImGuiConsoleLogWidget.h"
#include "core/features/Profiler.h"
#include <imgui.h>

#include "logging/Logger.h"
#include "core/features/Timer.h"
#include "core/features/Mesh.h"
#include "core/features/Material.h"
#include "core/features/ServiceLocator.h"
#include "core/resources/managers/Manager.h"
#include "core/resources/managers/TextureManager.h"
#include "core/resources/managers/MeshManager.h"
#include "core/resources/managers/ModelManager.h"
#include "core/resources/managers/BufferManager.h"
#include "core/resources/managers/DescriptorManager.h"
#include "core/resources/managers/MaterialManager.h" 
#include "vulkan/vulkan.h" //TODO: remove dependency
#include "graphics/framework/Vulkan/core/VulkanUtils.h"
#include "graphics/framework/Vulkan/resources/descriptors/DescriptorManagerVulkan.h"
#include "graphics/framework/Vulkan/resources/textures/TextureVulkan.h"
#include "window/AppWindow.h"

bool ButtonCenteredOnLine(const char* label, float alignment = 0.5f)
{
	ImGuiStyle& style = ImGui::GetStyle();

	float size = ImGui::CalcTextSize(label).x + style.FramePadding.x * 2.0f;
	float avail = ImGui::GetContentRegionAvail().x;
	float off = (avail - size) * alignment;

	if (off > 0.0f) {
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + off);
	}

	return ImGui::Button(label);
}

ImGuiConsoleLogWidget::ImGuiConsoleLogWidget() : ConsoleLogWidget()
{
	textureManager = &ServiceLocator::GetService<TextureManager>("TextureManagerVulkan");
	meshManager = &ServiceLocator::GetService<MeshManager>("MeshManager");
	modelManager = &ServiceLocator::GetService<ModelManager>("ModelManager");
	bufferManager = &ServiceLocator::GetService<BufferManager>("BufferManagerVulkan");
	DescriptorManager& descriptorManager = ServiceLocator::GetService<DescriptorManager>("DescriptorManagerVulkan");
	descriptorManagerVulkan = &static_cast<DescriptorManagerVulkan&>(descriptorManager);
	materialManager = &ServiceLocator::GetService<MaterialManager>("MaterialManagerVulkan");

	if(AppWindow::getWindowConfig().renderPlatform == RenderPlatform::VULKAN) {
		_createViewDescriptorBind();	// texture view layout and pool
	}
}

void ImGuiConsoleLogWidget::render()
{
	ImGui::BeginGroup();
	//ImGui::SetNextItemAllowOverlap();
	//ImGui::SetCursorPos(ImGui::GetWindowContentRegionMin());
	Profiler::display();

	ImGui::Begin("Assets");
	 _listTextureManager();
	ImGui::End();

	ImGui::Begin("console");
	ImGui::ShowDebugLogWindow();
	ImGui::End();

	//ImGui::ShowDemoWindow();
	ImGui::EndGroup();
}

void ImGuiConsoleLogWidget::_listTextureManager()
{
	ImGui::Begin("Textures");

	if(AppWindow::getWindowConfig().renderPlatform == RenderPlatform::VULKAN) {
		std::vector<uint32_t> ids = textureManager->listIDs();
		for (auto& id : ids) {
			textureIDs[id] = _createViewDescriptorSet(id);
			ImGui::Text(std::to_string(id).c_str());
			ImGui::Begin("Texture View");
			ImGui::BeginChild("Image View");
			VkDescriptorSet VkDescriptorSet = descriptorManagerVulkan->getDescriptorSet(textureIDs[id])[0];
			ImGui::Image((ImTextureID)VkDescriptorSet, ImVec2(250, 250));
			ImGui::EndChild();
			ImGui::End();
		}
	}
	
	//std::vector<uint32_t> ids = materialManager->listIDs();
	//for(auto& id : ids){
	//	ImGui::Text(std::to_string(id).c_str());
	//	MaterialDesc material = materialManager->getMaterial(id);
	//	ImGui::Begin("Texture View");
	//	ImGui::BeginChild("Image View");
	//	ImGui::Image((ImTextureID)material.albedoIDs[0], ImVec2(250, 250));
	//	ImGui::Text(std::to_string(material.albedoIDs[0]).c_str());
	//	ImGui::Image((ImTextureID)material.normalIDs[0], ImVec2(250, 250));
	//	ImGui::Text(std::to_string(material.normalIDs[0]).c_str());
	//	ImGui::Image((ImTextureID)material.metallicIDs[0], ImVec2(250, 250));
	//	ImGui::Text(std::to_string(material.metallicIDs[0]).c_str());
	//	ImGui::Image((ImTextureID)material.roughnessIDs[0], ImVec2(250, 250));
	//	ImGui::Text(std::to_string(material.roughnessIDs[0]).c_str());
	//	ImGui::Image((ImTextureID)material.aoIDs[0], ImVec2(250, 250));
	//	ImGui::Text(std::to_string(material.aoIDs[0]).c_str());
	//	ImGui::Image((ImTextureID)material.emissiveIDs[0], ImVec2(250, 250));
	//	ImGui::Text(std::to_string(material.emissiveIDs[0]).c_str());
	//	ImGui::EndChild();
	//	ImGui::End();
	//}
	ImGui::End();
}

void ImGuiConsoleLogWidget::_createViewDescriptorBind()
{
	const uint32_t MAX_NUM_SETS = 1000;				// add more if requires more
	
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
}

uint32_t ImGuiConsoleLogWidget::_createViewDescriptorSet(uint32_t id)
{
	if(textureIDs.find(id) != textureIDs.end()) {
		return textureIDs[id];
	}

	Texture* texture = textureManager->getTexture(id);
	TextureVulkan* textureVulkan = dynamic_cast<TextureVulkan*>(texture);
	if(!textureVulkan){
		throw std::runtime_error("ImGui createViewDescriptorSet: failed to retrieve texture");
	}

	uint32_t imguiSetID = descriptorManagerVulkan->createSets(imGuilayoutID, imGuipoolID, 1);
	VkDescriptorSet imguiTextureDescriptorSet = descriptorManagerVulkan->getDescriptorSet(imguiSetID)[0];

	VkDescriptorImageInfo imageInfo{};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo.imageView = textureVulkan->textureImageView;
	imageInfo.sampler = textureVulkan->textureSampler;

	std::vector<VkWriteDescriptorSet> writes{};
	descriptorManagerVulkan->writeImage(&writes, imguiTextureDescriptorSet, writes.size(), imageInfo);
	descriptorManagerVulkan->updateDescriptorSets(&writes);

	return imguiSetID;
}
