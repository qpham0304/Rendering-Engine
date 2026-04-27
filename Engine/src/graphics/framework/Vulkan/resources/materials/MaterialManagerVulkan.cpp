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
	materials[m_ids] = { MaterialVulkan(), MaterialUniform() };
	MaterialVulkan& material = materials[m_ids].first;


	//TODO: each mesh owns a set now, hash to prevent duplicate material set
	uint32_t frameCount = VulkanUtils::numFrames();
	material.descriptorSetID = descriptorManagerVulkan->createSets(materialLayoutID, materialPoolID, frameCount);

	BufferManager& bufferManager = ServiceLocator::GetService<BufferManager>("BufferManagerVulkan");
	auto bufferManagerVulkan = &dynamic_cast<BufferManagerVulkan&>(bufferManager);
	
	bufferManagerVulkan->createUniformBuffers(material.uniformbuffersList, sizeof(MaterialUniform));
	
	for(int i = 0; i < VulkanUtils::numFrames(); i++) {
		updateMaterial(m_ids, materialDesc, i);
	}

	
	GPUMaterialData materialGPU {};
	materialGPU.albedoIdx = material.albedoID;
	materialGPU.normalIdx = material.normalID;
	materialGPU.metalnessIdx = material.metallicID;
	materialGPU.roughnessIdx = material.roughnessID;
	materialGPU.aoIdx = material.aoID;
	materialGPU.emissiveIdx = material.emissiveID;
	materialsGPU[m_ids] = materialGPU;

    return _assignID();
}

void MaterialManagerVulkan::bindMaterial(const uint32_t &id, void* cmdBuffer, void* p)
{
	assert(p && "pipeline required");

	uint32_t frame = renderDeviceVulkan->getCurrentFrameIndex();
	const MaterialVulkan& material = materials.at(id).first;
	VkDescriptorSet materialSet = descriptorManagerVulkan->getDescriptorSet(material.descriptorSetID)[frame];

	VulkanPipeline* pipeline = static_cast<VulkanPipeline*>(p);
	
	vkCmdBindDescriptorSets(
		reinterpret_cast<VkCommandBuffer>(cmdBuffer),
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		pipeline->pipelineLayout,
		1,
		1,
		&descriptorManagerVulkan->getDescriptorSet(textureManagerVulkan->getBindlessSet())[0],
		0,
		nullptr
	);

	vkCmdBindDescriptorSets(
		reinterpret_cast<VkCommandBuffer>(cmdBuffer),
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		pipeline->pipelineLayout,
		2,
		1,
		&materialSet,
		0,
		nullptr
	);

}

MaterialDesc MaterialManagerVulkan::getMaterial(const uint32_t &id)
{
	const MaterialVulkan& material = materials.at(id).first;
	const MaterialUniform& materialControl = materials.at(id).second;

    return MaterialDesc {
		{ material.albedoID },
		{ material.normalID },
		{ material.metallicID },
		{ material.roughnessID },
		{ material.aoID },
		{ material.emissiveID },
		{ materialControl.materialIdx }, // this is not currently used or set
		{ materialControl.uv },
		{ materialControl.albedo },
		{ materialControl.normal },
		{ materialControl.metallic },
		{ materialControl.roughness },
		{ materialControl.ao },
		{ materialControl.emissive }
	};
}

uint64_t MaterialManagerVulkan::getMaterialAddress(uint32_t id)
{
	return materialDeviceAddress->getReference() + (id * sizeof(GPUMaterialData));
}

void MaterialManagerVulkan::_buildMaterialCache()
{
	for (auto const& [id, pair] : materials) {
		MaterialVulkan material = pair.first;
        GPUMaterialData gpuMaterial {};
        gpuMaterial.albedoIdx    = material.albedoID;
        gpuMaterial.normalIdx    = material.normalID;
        gpuMaterial.metalnessIdx = material.metallicID;
        gpuMaterial.roughnessIdx = material.roughnessID;
        gpuMaterial.aoIdx        = material.aoID;
        gpuMaterial.emissiveIdx  = material.emissiveID;
        
        materialsGPU[id] = gpuMaterial;
    }
}

void MaterialManagerVulkan::_updateGPUBuffer()
{
	materialDeviceAddress->update(materialsGPU.data(), sizeof(GPUMaterialData) * materialsGPU.size());
}

bool MaterialManagerVulkan::updateMaterial(uint32_t id, const MaterialDesc &materialDesc, uint32_t frameIndex)
{
    auto it = materials.find(id);
    if(it == materials.end()) return false;
    
    MaterialVulkan& material = it->second.first;
    material.albedoID = _checkMaterial(materialDesc.albedoIDs, fallback_albedoID);
    material.normalID = _checkMaterial(materialDesc.normalIDs, fallback_normalID);
    material.metallicID = _checkMaterial(materialDesc.metallicIDs, fallback_metallicID);
    material.roughnessID = _checkMaterial(materialDesc.roughnessIDs, fallback_roughnessID);
    material.aoID = _checkMaterial(materialDesc.aoIDs, fallback_aoID);
    material.emissiveID = _checkMaterial(materialDesc.emissiveIDs, fallback_emissiveID);

    MaterialUniform& materialUniform = it->second.second;
    materialUniform.materialIdx = materialDesc.materialIdx;
    materialUniform.uv = materialDesc.uv;
    materialUniform.albedo = materialDesc.albedo;
    materialUniform.normal = materialDesc.normal;
    materialUniform.metallic  = materialDesc.metallic;
    materialUniform.roughness = materialDesc.roughness;
    materialUniform.ao         = materialDesc.ao;
    materialUniform.emissive   = materialDesc.emissive;

    auto materialSets = descriptorManagerVulkan->getDescriptorSet(material.descriptorSetID);

    auto updateDescriptor = [&](uint32_t frame) {
        std::vector<VkWriteDescriptorSet> writes;
        
        auto writeMaterial = [&](uint32_t binding, uint32_t textureID) {
			textureManagerVulkan->registerTextureSampler(textureID);
        };
		
		textureManagerVulkan->registerTextureSampler(material.albedoID);
		textureManagerVulkan->registerTextureSampler(material.normalID);
		textureManagerVulkan->registerTextureSampler(material.metallicID);
		textureManagerVulkan->registerTextureSampler(material.roughnessID);
		textureManagerVulkan->registerTextureSampler(material.aoID);
		textureManagerVulkan->registerTextureSampler(material.emissiveID);

        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = static_cast<VkBuffer>(*material.uniformbuffersList[frame]);
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(MaterialUniform);

        descriptorManagerVulkan->writeUniform(&writes, materialSets[frame], 0, bufferInfo);
        descriptorManagerVulkan->updateDescriptorSets(&writes);
        material.uniformbuffersList[frame]->update(&materialUniform, sizeof(materialUniform));
    };

    if(frameIndex == -1) {	// if no frameIndex provided use default value and update all in flight descriptors
        renderDeviceVulkan->waitIdle();
        for(uint32_t i = 0; i < VulkanUtils::numFrames(); i++) {
            updateDescriptor(i);
        }
    } else {				// if frameIndex provided, only update in flight frame
        updateDescriptor(frameIndex);
    }

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