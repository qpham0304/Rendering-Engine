#include "MaterialManagerVulkan.h"
#include "graphics/framework/Vulkan/renderers/RenderDeviceVulkan.h"
#include "graphics/framework/Vulkan/resources/textures/TextureManagerVulkan.h"
#include "graphics/framework/Vulkan/resources/descriptors/DescriptorManagerVulkan.h"
#include "core/features/ServiceLocator.h"
#include "core/features/Material.h"
#include "logging/Logger.h"
#include <graphics/framework/Vulkan/resources/buffers/DeviceAddressBufferVulkan.h>

MaterialManagerVulkan::MaterialManagerVulkan(std::string serviceName)
	: MaterialManager(serviceName)
{

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

	fallback_albedoID = textureManagerVulkan->loadTexture("assets/Textures/default/32x32/albedo.png", 1, false);
	fallback_normalID = textureManagerVulkan->loadTexture("assets/Textures/default/32x32/normal.png", 1, true);
	// fallback_metallicID = textureManagerVulkan->loadTexture("assets/Textures/default/32x32/metallic.png", 1, true);
	fallback_metallicID = textureManagerVulkan->loadTexture("assets/Textures/default/32x32/roughness-2.png", 1, true);
	fallback_roughnessID = textureManagerVulkan->loadTexture("assets/Textures/default/32x32/roughness.png", 1, true);
	fallback_aoID = textureManagerVulkan->loadTexture("assets/Textures/default/32x32/ao.png", 1, true);
	fallback_emissiveID = textureManagerVulkan->loadTexture("assets/Textures/default/32x32/emissive.png", 1, false);

	_createMaterialDescriptorSet();
	
	materialsGPU.resize(10000);
	BufferManager& bufferManager = ServiceLocator::GetService<BufferManager>("BufferManagerVulkan");
    auto bufferManagerVulkan = &dynamic_cast<BufferManagerVulkan&>(bufferManager);

	
	//TODO: might abstract this behind the bufferManager for resue
    VkDeviceSize bufferSize = sizeof(GPUMaterialData) * materialsGPU.size();
	uint32_t bufferID = bufferManagerVulkan->createBufferDeviceAddress(bufferSize);
	materialDeviceAddress = (DeviceAddressBufferVulkan*)bufferManagerVulkan->getBuffer(bufferID);

    return true;
}

void MaterialManagerVulkan::onUpdate()
{
	//TODO: conditional update would be better in case of thousands materials
	_buildMaterialCache();
	_updateGPUBuffer();
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
	for (const auto& [id, material] : materials) {
		list.emplace_back(id);
	}
	return list;
}

uint32_t MaterialManagerVulkan::createMaterial(const MaterialDesc &materialDesc)
{
	materials[m_ids] = MaterialVulkan();
	MaterialVulkan& material = materials[m_ids];

	updateMaterial(m_ids, materialDesc);

	GPUMaterialData materialGPU {};
	materialGPU.albedoIdx = material.albedoID;
	materialGPU.normalIdx = material.normalID;
	materialGPU.metalnessIdx = material.metallicID;
	materialGPU.roughnessIdx = material.roughnessID;
	materialGPU.aoIdx = material.aoID;
	materialGPU.emissiveIdx = material.emissiveID;
	materialGPU.uv = material.uv;
	materialGPU.albedo = material.albedo;
	materialGPU.normal = material.normal;
	materialGPU.metallic = material.metallic;
	materialGPU.roughness = material.roughness;
	materialGPU.ao = material.ao;
	materialGPU.emissive = material.emissive;
	materialsGPU[m_ids] = materialGPU;

    return _assignID();
}

void MaterialManagerVulkan::bindMaterial(void* cmdBuffer, void* p)
{
	assert(p && "pipeline required");

	VulkanPipeline* pipeline = static_cast<VulkanPipeline*>(p);
	
	// note: the interface only support binding compute right now
	vkCmdBindDescriptorSets(
		reinterpret_cast<VkCommandBuffer>(cmdBuffer),
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		pipeline->pipelineLayout,
		1,	// note: this always bind set 1 so always reserve set 1 if use this bind function
		1,
		&descriptorManagerVulkan->getDescriptorSet(textureManagerVulkan->getBindlessSet())[0],
		0,
		nullptr
	);

}

MaterialDesc MaterialManagerVulkan::getMaterial(const uint32_t &id)
{
	const MaterialVulkan& material = materials.at(id);

    return MaterialDesc {
		{ material.albedoID },
		{ material.normalID },
		{ material.metallicID },
		{ material.roughnessID },
		{ material.aoID },
		{ material.emissiveID },
		{ material.uv },
		{ material.albedo },
		{ material.normal },
		{ material.metallic },
		{ material.roughness },
		{ material.ao },
		{ material.emissive }
	};
}

uint64_t MaterialManagerVulkan::getMaterialAddress()
{
	return materialDeviceAddress->getReference();
}

void MaterialManagerVulkan::_buildMaterialCache()
{
	for (auto const& [id, pair] : materials) {
		MaterialVulkan material = pair;
        GPUMaterialData gpuMaterial {};
        gpuMaterial.albedoIdx    = material.albedoID;
        gpuMaterial.normalIdx    = material.normalID;
        gpuMaterial.metalnessIdx = material.metallicID;
        gpuMaterial.roughnessIdx = material.roughnessID;
        gpuMaterial.aoIdx        = material.aoID;
        gpuMaterial.emissiveIdx  = material.emissiveID;

		gpuMaterial.uv = material.uv;
		gpuMaterial.albedo = material.albedo;
		gpuMaterial.normal = material.normal;
		gpuMaterial.metallic = material.metallic;
		gpuMaterial.roughness = material.roughness;
		gpuMaterial.ao = material.ao;
		gpuMaterial.emissive = material.emissive;
        materialsGPU[id] = gpuMaterial;
    }
}

void MaterialManagerVulkan::_updateGPUBuffer()
{
	materialDeviceAddress->update(materialsGPU.data(), sizeof(GPUMaterialData) * materialsGPU.size());
}

bool MaterialManagerVulkan::updateMaterial(uint32_t id, const MaterialDesc &materialDesc)
{
    auto it = materials.find(id);
    if(it == materials.end()) return false;
    
    MaterialVulkan& material = it->second;
    material.albedoID = _checkMaterial(materialDesc.albedoIDs, fallback_albedoID);
    material.normalID = _checkMaterial(materialDesc.normalIDs, fallback_normalID);
    material.metallicID = _checkMaterial(materialDesc.metallicIDs, fallback_metallicID);
    material.roughnessID = _checkMaterial(materialDesc.roughnessIDs, fallback_roughnessID);
    material.aoID = _checkMaterial(materialDesc.aoIDs, fallback_aoID);
    material.emissiveID = _checkMaterial(materialDesc.emissiveIDs, fallback_emissiveID);
	material.uv = materialDesc.uv;
	material.albedo = materialDesc.albedo;
	material.normal = materialDesc.normal;
	material.metallic  = materialDesc.metallic ;
	material.roughness = materialDesc.roughness;
	material.ao        = materialDesc.ao       ;
	material.emissive  = materialDesc.emissive ;

	textureManagerVulkan->registerTextureSampler(material.albedoID);
	textureManagerVulkan->registerTextureSampler(material.normalID);
	textureManagerVulkan->registerTextureSampler(material.metallicID);
	textureManagerVulkan->registerTextureSampler(material.roughnessID);
	textureManagerVulkan->registerTextureSampler(material.aoID);
	textureManagerVulkan->registerTextureSampler(material.emissiveID);

    return true;
}

void* MaterialManagerVulkan::getMaterialLayout()
{
	VkDescriptorSetLayout layout = descriptorManagerVulkan->getDescriptorLayout(materialLayoutID);
	return reinterpret_cast<void*>(layout);
}

uint32_t MaterialManagerVulkan::_checkMaterial(const std::vector<uint32_t> &textures, uint32_t fallbackID) const
{
	// only use the first material imported from assimp
    return textures.empty() ? fallbackID : textures[0];
}



void MaterialManagerVulkan::_createMaterialDescriptorSet()
{
	std::vector<VkDescriptorSetLayoutBinding> bindings = {
		{ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}
	};
	materialLayoutID = descriptorManagerVulkan->createLayout(bindings);

	uint32_t frameCount = VulkanUtils::numFrames();
	uint32_t maxMaterial = 1024 * 8;
	std::vector<VkDescriptorPoolSize> poolSizes = {
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frameCount * maxMaterial }
	};
	materialPoolID = descriptorManagerVulkan->createPool(poolSizes, maxMaterial);
}