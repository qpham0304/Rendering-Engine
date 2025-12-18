#pragma once

#include "graphics/framework/Vulkan/core/WrapperStructs.h"
#include "core/resources/managers/MaterialManager.h"
#include <vulkan/vulkan.h>

class RenderDeviceVulkan;
class TextureManagerVulkan;
class DescriptorManagerVulkan;

class MaterialManagerVulkan : public MaterialManager
{
public:
    struct MaterialVulkan {
		uint32_t descriptorSetID;
		uint32_t albedoID;
		uint32_t normalID;
		uint32_t metallicID;
		uint32_t roughnessID;
		uint32_t aoID;
		uint32_t emissiveID;
    };

public:
	MaterialManagerVulkan(std::string serviceName = "MaterialManagerVulkan");
	~MaterialManagerVulkan();

	virtual bool init(WindowConfig config) override;
	virtual bool onClose() override;
	virtual void destroy(uint32_t id) override;
	virtual std::vector<uint32_t> listIDs() const override;
	virtual uint32_t createMaterial(const MaterialDesc& materialDesc) override;
    virtual void bindMaterial(const uint32_t& id, void* cmdBuffer) override;
    virtual MaterialDesc getMaterial(const uint32_t& id) override;
	virtual void* getMaterialLayout() override;

private:
	uint32_t _checkMaterial(const std::vector<uint32_t>& textures, uint32_t fallbackID) const;
	void _createMaterialDescriptorSet();

private:
	RenderDeviceVulkan* renderDeviceVulkan;
	TextureManagerVulkan* textureManagerVulkan;
	DescriptorManagerVulkan* descriptorManagerVulkan;


    std::unordered_map<uint32_t, MaterialVulkan> materials;
	uint32_t materialLayoutID;
	uint32_t materialPoolID;

	uint32_t fallback_albedoID;
	uint32_t fallback_normalID;
	uint32_t fallback_metallicID;
	uint32_t fallback_roughnessID;
	uint32_t fallback_aoID;
	uint32_t fallback_emissiveID;
};

