#include "MaterialManagerVulkan.h"
#include "graphics/framework/Vulkan/renderers/RenderDeviceVulkan.h"
#include "graphics/framework/Vulkan/resources/textures/TextureManagerVulkan.h"
#include "graphics/framework/Vulkan/resources/descriptors/DescriptorManagerVulkan.h"
#include "graphics/framework/Vulkan/core/CommonStructs.h"
#include "core/features/ServiceLocator.h"
#include "core/features/Material.h"
#include "logging/Logger.h"
#include <graphics/framework/Vulkan/resources/buffers/DeviceAddressBufferVulkan.h>
#include <algorithm>
#include <stdexcept>
#include <string>

MaterialManagerVulkan::MaterialManagerVulkan(std::string serviceName)
	: MaterialManager(serviceName)
{
	setFootprint("MaterialManagerVulkan", sizeof(MaterialManagerVulkan), MemorySystem::Graphics);

}

MaterialManagerVulkan::~MaterialManagerVulkan()
{

}

bool MaterialManagerVulkan::init(WindowConfig config)
{
    Service::init(config);

	RenderDevice& device = ServiceLocator::GetService<RenderDevice>("RenderDeviceVulkan");
	renderDeviceVulkan = dynamic_cast<RenderDeviceVulkan*>(&device);

	TextureManager& textureManager = ServiceLocator::GetService<TextureManager>("TextureManagerVulkan");
	textureManagerVulkan = dynamic_cast<TextureManagerVulkan*>(&textureManager);

	DescriptorManager& descriptorManager = ServiceLocator::GetService<DescriptorManager>("DescriptorManagerVulkan");
	descriptorManagerVulkan = dynamic_cast<DescriptorManagerVulkan*>(&descriptorManager);

	if (!(renderDeviceVulkan && textureManagerVulkan && descriptorManagerVulkan)) {
		return false;
	}

	fallback_albedoID = _loadFallbackTexture("assets/Textures/default/32x32/albedo.png", false, { 255, 255, 255, 255 });
	fallback_normalID = _loadFallbackTexture("assets/Textures/default/32x32/normal.png", true, { 128, 128, 255, 255 });
	// fallback_metallicID = textureManagerVulkan->loadTexture("assets/Textures/default/32x32/metallic.png", 1, true);
	fallback_metallicID = _loadFallbackTexture("assets/Textures/default/32x32/roughness-2.png", true, { 0, 0, 0, 255 });
	fallback_roughnessID = _loadFallbackTexture("assets/Textures/default/32x32/roughness.png", true, { 255, 255, 255, 255 });
	fallback_aoID = _loadFallbackTexture("assets/Textures/default/32x32/ao.png", true, { 255, 255, 255, 255 });
	fallback_emissiveID = _loadFallbackTexture("assets/Textures/default/32x32/emissive.png", false, { 0, 0, 0, 255 });

	_createMaterialDescriptorSet();
	
	//TODO: hard coded the max number of material indices
	MaterialDesc unused {};
	unused.albedoID = fallback_albedoID;
	unused.normalID = fallback_normalID;
	unused.metallicID = fallback_metallicID;
	unused.roughnessID = fallback_roughnessID;
	unused.aoID = fallback_aoID;
	unused.emissiveID = fallback_emissiveID;
	materials.resize(Common::MAX_INSTANCES, unused);

	BufferManager& bufferManager = ServiceLocator::GetService<BufferManager>("BufferManagerVulkan");
    auto bufferManagerVulkan = &dynamic_cast<BufferManagerVulkan&>(bufferManager);

    VkDeviceSize bufferSize = sizeof(MaterialDesc) * materials.size();
	uint32_t bufferID = bufferManagerVulkan->createBufferDeviceAddress(bufferSize);
	materialDeviceAddress = (DeviceAddressBufferVulkan*)bufferManagerVulkan->getBuffer(bufferID);

    return true;
}

void MaterialManagerVulkan::onUpdate(const FrameTime& frameTime)
{
	//TODO: conditional update would be better in case of thousands materials
	_updateGPUBuffer();
}

uint32_t MaterialManagerVulkan::_loadFallbackTexture(const std::string& path, bool isDataTexture, std::array<uint8_t, 4> color)
{
	uint32_t id = textureManagerVulkan->loadTexture(path, 1, isDataTexture);
	if (id != 0) {
		return id;
	}

	// a magenta checker as the default normal or roughness map would break every material, use a flat color
	VkFormat format = isDataTexture ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_R8G8B8A8_SRGB;
	return textureManagerVulkan->createTextureFromPixels(color.data(), 1, 1, format, "$fallback$" + path, isDataTexture);
}

bool MaterialManagerVulkan::onClose()
{
    return true;
}

void MaterialManagerVulkan::destroy(uint32_t id)
{
    
}

std::vector<uint32_t> MaterialManagerVulkan::listIDs() const
{
	std::vector<uint32_t> list;
	uint32_t count = std::min<uint32_t>(m_ids.load(std::memory_order_relaxed), static_cast<uint32_t>(materials.size()));
	for (uint32_t id = 1; id < count; id++) {
		list.emplace_back(id);
	}
	return list;
}

uint32_t MaterialManagerVulkan::createMaterial(const MaterialDesc &materialDesc)
{
	if (m_ids >= materials.size()) {
		m_logger->error("MaterialManagerVulkan::createMaterial: material limit {} reached", materials.size());
		return 0;
	}
	_setMaterial(m_ids, materialDesc);
    return _assignID();
}

bool MaterialManagerVulkan::_exists(uint32_t id) const
{
	return id != 0 && id < m_ids.load(std::memory_order_relaxed) && id < materials.size();
}

void MaterialManagerVulkan::bindMaterial(void* cmdBuffer, void* p)
{
	assert(p && "pipeline required");

	VulkanPipeline* pipeline = static_cast<VulkanPipeline*>(p);
	
	//TODO: read **Note**
	// **Note**: the interface only support binding compute right now
	vkCmdBindDescriptorSets(
		reinterpret_cast<VkCommandBuffer>(cmdBuffer),
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		pipeline->pipelineLayout,
		1,	// **Note**: this always bind set 1 so always reserve set 1 if use this bind function
		1,
		&descriptorManagerVulkan->getDescriptorSet(textureManagerVulkan->getBindlessSet())[0],
		0,
		nullptr
	);
}

MaterialDesc MaterialManagerVulkan::getMaterial(const uint32_t &id)
{
	if (!_exists(id)) {
		throw std::out_of_range("MaterialManagerVulkan::getMaterial: invalid material id " + std::to_string(id));
	}
	return materials[id];
}

uint64_t MaterialManagerVulkan::getMaterialAddress()
{
	return materialDeviceAddress->getAddress();
}

void MaterialManagerVulkan::_updateGPUBuffer()
{
	materialDeviceAddress->update(materials.data(), sizeof(MaterialDesc) * materials.size());
}

bool MaterialManagerVulkan::updateMaterial(uint32_t id, const MaterialDesc &materialDesc)
{
    if (!_exists(id)) {
		return false;
	}

    _setMaterial(id, materialDesc);
    return true;
}

void MaterialManagerVulkan::_setMaterial(uint32_t id, const MaterialDesc &materialDesc)
{
    MaterialDesc& material = materials[id];
    material = materialDesc;
    material.albedoID = _checkMaterial(materialDesc.albedoID, fallback_albedoID);
    material.normalID = _checkMaterial(materialDesc.normalID, fallback_normalID);
    material.metallicID = _checkMaterial(materialDesc.metallicID, fallback_metallicID);
    material.roughnessID = _checkMaterial(materialDesc.roughnessID, fallback_roughnessID);
    material.aoID = _checkMaterial(materialDesc.aoID, fallback_aoID);
    material.emissiveID = _checkMaterial(materialDesc.emissiveID, fallback_emissiveID);

	textureManagerVulkan->registerBindlessTexture(material.albedoID);
	textureManagerVulkan->registerBindlessTexture(material.normalID);
	textureManagerVulkan->registerBindlessTexture(material.metallicID);
	textureManagerVulkan->registerBindlessTexture(material.roughnessID);
	textureManagerVulkan->registerBindlessTexture(material.aoID);
	textureManagerVulkan->registerBindlessTexture(material.emissiveID);
}

void* MaterialManagerVulkan::getMaterialLayout()
{
	VkDescriptorSetLayout layout = descriptorManagerVulkan->getDescriptorLayout(materialLayoutID);
	return reinterpret_cast<void*>(layout);
}

uint32_t MaterialManagerVulkan::_checkMaterial(uint32_t textureID, uint32_t fallbackID) const
{
    return textureID == 0 ? fallbackID : textureID;
}



void MaterialManagerVulkan::_createMaterialDescriptorSet()
{
	std::vector<VkDescriptorSetLayoutBinding> bindings = {
		{ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}
	};
	materialLayoutID = descriptorManagerVulkan->createLayout(bindings);

	uint32_t frameCount = VulkanUtils::numFrames();
	uint32_t maxMaterial = Common::MAX_INSTANCES;
	std::vector<VkDescriptorPoolSize> poolSizes = {
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frameCount * maxMaterial }
	};
	materialPoolID = descriptorManagerVulkan->createPool(poolSizes, maxMaterial);
}