#pragma once

#include "graphics/framework/Vulkan/core/WrapperStructs.h"
#include "core/resources/managers/MaterialManager.h"
#include "graphics/framework/vulkan/resources/buffers/BufferManagerVulkan.h"
#include <volk.h>
#include <glm/glm.hpp>
#include <cstdint>
#include <array>

class RenderDeviceVulkan;
class TextureManagerVulkan;
class DescriptorManagerVulkan;
class DeviceAddressBufferVulkan;

class MaterialManagerVulkan : public MaterialManager
{
public:
	MaterialManagerVulkan(std::string serviceName = "MaterialManagerVulkan");
	~MaterialManagerVulkan();

	virtual bool init(WindowConfig config) override;
	virtual void onUpdate(const FrameTime& frameTime) override;
	virtual bool onClose() override;
	virtual void destroy(uint32_t id) override;
	virtual std::vector<uint32_t> listIDs() const override;
	virtual uint32_t createMaterial(const MaterialDesc& materialDesc) override;
    virtual void bindMaterial(void* cmdBuffer, void* pipeline) override;
    virtual MaterialDesc getMaterial(const uint32_t& id) override;
	virtual void* getMaterialLayout() override;
	virtual bool updateMaterial(uint32_t id, const MaterialDesc& materialDesc) override;
	
	uint64_t getMaterialAddress();
	uint32_t getFallbackAlbedoID() const { return fallback_albedoID; }
	

private:
	bool _exists(uint32_t id) const;
	void _setMaterial(uint32_t id, const MaterialDesc& materialDesc);
	uint32_t _checkMaterial(uint32_t textureID, uint32_t fallbackID) const;
	uint32_t _loadFallbackTexture(const std::string& path, bool isDataTexture, std::array<uint8_t, 4> color);
	void _createMaterialDescriptorSet();
	void _updateGPUBuffer();

private:
	RenderDeviceVulkan* renderDeviceVulkan;
	TextureManagerVulkan* textureManagerVulkan;
	DescriptorManagerVulkan* descriptorManagerVulkan;


	std::vector<MaterialDesc> materials;
	DeviceAddressBufferVulkan* materialDeviceAddress;
	
	uint32_t materialLayoutID;
	uint32_t materialPoolID;

	uint32_t fallback_albedoID;
	uint32_t fallback_normalID;
	uint32_t fallback_metallicID;
	uint32_t fallback_roughnessID;
	uint32_t fallback_aoID;
	uint32_t fallback_emissiveID;
};

