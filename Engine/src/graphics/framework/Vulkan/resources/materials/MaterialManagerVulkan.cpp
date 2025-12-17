#include "MaterialManagerVulkan.h"
#include "graphics/framework/Vulkan/renderers/RenderDeviceVulkan.h"
#include "graphics/framework/Vulkan/resources/textures/TextureManagerVulkan.h"
#include "graphics/framework/Vulkan/resources/descriptors/DescriptorManagerVulkan.h"
#include "core/features/ServiceLocator.h"
#include "core/features/Material.h"
#include "logging/Logger.h"

MaterialManagerVulkan::MaterialManagerVulkan(std::string serviceName)
	: MaterialManager(serviceName)
{

}

MaterialManagerVulkan::~MaterialManagerVulkan()
{

}

int MaterialManagerVulkan::init(WindowConfig config)
{
    Service::init(config);

    m_logger = &ServiceLocator::GetService<Logger>("Engine_LoggerPSD");

	RenderDevice& device = ServiceLocator::GetService<RenderDevice>("RenderDeviceVulkan");
	renderDeviceVulkan = dynamic_cast<RenderDeviceVulkan*>(&device);

	TextureManager& textureManager = ServiceLocator::GetService<TextureManager>("TextureManagerVulkan");
	textureManagerVulkan = dynamic_cast<TextureManagerVulkan*>(&textureManager);

	DescriptorManager& descriptorManager = ServiceLocator::GetService<DescriptorManager>("DescriptorManagerVulkan");
	descriptorManagerVulkan = dynamic_cast<DescriptorManagerVulkan*>(&descriptorManager);

	if (!(renderDeviceVulkan && textureManagerVulkan && descriptorManagerVulkan)) {
		return -1;
	}

	fallback_albedoID = textureManagerVulkan->loadTexture("assets/Textures/default/32x32/albedo.png");
	fallback_normalID = textureManagerVulkan->loadTexture("assets/Textures/default/32x32/normal.png");
	fallback_metallicID = textureManagerVulkan->loadTexture("assets/Textures/default/32x32/metallic.png");
	fallback_roughnessID = textureManagerVulkan->loadTexture("assets/Textures/default/32x32/roughness.png");
	fallback_aoID = textureManagerVulkan->loadTexture("assets/Textures/default/32x32/ao.png");
	fallback_emissiveID = textureManagerVulkan->loadTexture("assets/Textures/default/32x32/emissive.png");

	_createMaterialDescriptorSet();

    return 0;
}

int MaterialManagerVulkan::onClose()
{
    return 0;
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

	material.albedoID = _checkMaterial(materialDesc.albedoIDs, fallback_albedoID);
	material.normalID = _checkMaterial(materialDesc.normalIDs, fallback_normalID);
	material.metallicID = _checkMaterial(materialDesc.metallicIDs, fallback_metallicID);
	material.roughnessID = _checkMaterial(materialDesc.roughnessIDs, fallback_roughnessID);
	material.aoID = _checkMaterial(materialDesc.aoIDs, fallback_aoID);
	material.emissiveID = _checkMaterial(materialDesc.emissiveIDs, fallback_emissiveID);

	//TODO: each mesh own a set now, hash to prevent duplicate material set
	material.descriptorSetID = descriptorManagerVulkan->createSets(materialLayoutID, materialPoolID, 1);
	VkDescriptorSet materialSet = descriptorManagerVulkan->getDescriptorSet(material.descriptorSetID)[0];

	std::vector<VkWriteDescriptorSet> writes;
	std::vector<VkDescriptorImageInfo> imageInfos;
	writes.reserve(6);
	imageInfos.reserve(6);

	//std::vector<TextureVulkan*> textures;
	//textures.push_back(textureManagerVulkan->getTexture(material.albedoID));
	//textures.push_back(textureManagerVulkan->getTexture(material.normalID));
	//textures.push_back(textureManagerVulkan->getTexture(material.metallicID));
	//textures.push_back(textureManagerVulkan->getTexture(material.roughnessID));
	//textures.push_back(textureManagerVulkan->getTexture(material.aoID));
	//textures.push_back(textureManagerVulkan->getTexture(material.emissiveID));

	//int i = 0;
	//for(TextureVulkan* texture : textures) {
	//	imageInfos.push_back(VkDescriptorImageInfo());
	//	imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	//	imageInfos[i].imageView = texture->textureImageView;
	//	imageInfos[i].sampler = texture->textureSampler;

	//	descriptorManagerVulkan->writeImage(&writes, materialSet, writes.size(), imageInfos[i++]);
	//}

	auto writeMaterial = [&](uint32_t binding, uint32_t materialID) {
		TextureVulkan* texture = textureManagerVulkan->getTexture(materialID);

		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = texture->textureImageView;
		imageInfo.sampler = texture->textureSampler;
		imageInfos.push_back(imageInfo);
		
		descriptorManagerVulkan->writeImage(&writes, materialSet, binding, imageInfos.back());
	};

	writeMaterial(0, material.albedoID);
	writeMaterial(1, material.normalID);
	writeMaterial(2, material.metallicID);
	writeMaterial(3, material.roughnessID);
	writeMaterial(4, material.aoID);
	writeMaterial(5, material.emissiveID);

	descriptorManagerVulkan->updateDescriptorSets(&writes);

    return _assignID();
}

void MaterialManagerVulkan::bindMaterial(const uint32_t &id, void* cmdBuffer)
{
	MaterialVulkan material = materials.at(id);
	VkDescriptorSet materialSet = descriptorManagerVulkan->getDescriptorSet(material.descriptorSetID)[0];

	vkCmdBindDescriptorSets(
		reinterpret_cast<VkCommandBuffer>(cmdBuffer),
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		renderDeviceVulkan->pipeline.pipelineLayout,
		1,
		1,
		&materialSet,
		0,
		nullptr
	);
}

MaterialDesc MaterialManagerVulkan::getMaterial(const uint32_t &id)
{
	MaterialVulkan material = materials.at(id);

    return MaterialDesc {
		{ material.albedoID },
		{ material.normalID },
		{ material.metallicID },
		{ material.roughnessID },
		{ material.aoID },
		{ material.emissiveID },
	};
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
		{ 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
		{ 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
		{ 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
		{ 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
		{ 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
		{ 5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
	};
	materialLayoutID = descriptorManagerVulkan->createLayout(bindings);
	m_logger->warn("material layout ID: {}", materialLayoutID);

	uint32_t maxMaterial = 1024;
	std::vector<VkDescriptorPoolSize> poolSizes = {
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 6 * maxMaterial },
	};
	materialPoolID = descriptorManagerVulkan->createPool(poolSizes, maxMaterial);
	m_logger->warn("material pool ID: {}", materialPoolID);
}