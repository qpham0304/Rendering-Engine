#include "DLSSSupport.h"

#ifdef WK_DLSS
#include <nvsdk_ngx_vk.h>
#include <string>
#include <algorithm>

namespace {
	NVSDK_NGX_FeatureDiscoveryInfo discoveryInfo()
	{
		NVSDK_NGX_FeatureDiscoveryInfo info{};
		info.SDKVersion = NVSDK_NGX_Version_API;
		info.FeatureID = NVSDK_NGX_Feature_SuperSampling;
		info.Identifier.IdentifierType = NVSDK_NGX_Application_Identifier_Type_Project_Id;
		info.Identifier.v.ProjectDesc.ProjectId = DLSSSupport::ProjectID;
		info.Identifier.v.ProjectDesc.EngineType = NVSDK_NGX_ENGINE_TYPE_CUSTOM;
		info.Identifier.v.ProjectDesc.EngineVersion = DLSSSupport::EngineVersion;
		info.ApplicationDataPath = L".";
		info.FeatureInfo = DLSSSupport::featureInfo();
		return info;
	}

	// all or nothing, a missing extension means the device can't run DLSS
	std::optional<std::vector<const char*>> filterAvailable(uint32_t count, const VkExtensionProperties* required, const std::vector<VkExtensionProperties>& available)
	{
		std::vector<const char*> names;
		for(uint32_t i = 0; i < count; i++) {
			bool found = std::any_of(available.begin(), available.end(), [&](const VkExtensionProperties& ext) {
				return std::string(ext.extensionName) == required[i].extensionName;
			});
			if(!found) {
				return std::nullopt;
			}
			names.push_back(required[i].extensionName);
		}
		return names;
	}
}

// the dll stays in the sdk folder instead of being copied next to every exe, the exe folder is still searched after it
const NVSDK_NGX_FeatureCommonInfo* DLSSSupport::featureInfo()
{
	static const wchar_t* paths[] = { L"" WK_DLSS_RUNTIME_DIR };
	static NVSDK_NGX_FeatureCommonInfo info = [] {
		NVSDK_NGX_FeatureCommonInfo common{};
		common.PathListInfo.Path = paths;
		common.PathListInfo.Length = 1;
		return common;
	}();
	return &info;
}

std::optional<std::vector<const char*>> DLSSSupport::instanceExtensions()
{
	NVSDK_NGX_FeatureDiscoveryInfo info = discoveryInfo();
	uint32_t count = 0;
	VkExtensionProperties* required = nullptr;
	if(NVSDK_NGX_FAILED(NVSDK_NGX_VULKAN_GetFeatureInstanceExtensionRequirements(&info, &count, &required))) {
		return std::nullopt;
	}

	uint32_t availableCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &availableCount, nullptr);
	std::vector<VkExtensionProperties> available(availableCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &availableCount, available.data());

	return filterAvailable(count, required, available);
}

std::optional<std::vector<const char*>> DLSSSupport::deviceExtensions(VkInstance instance, VkPhysicalDevice physicalDevice)
{
	NVSDK_NGX_FeatureDiscoveryInfo info = discoveryInfo();
	uint32_t count = 0;
	VkExtensionProperties* required = nullptr;
	if(NVSDK_NGX_FAILED(NVSDK_NGX_VULKAN_GetFeatureDeviceExtensionRequirements(instance, physicalDevice, &info, &count, &required))) {
		return std::nullopt;
	}

	uint32_t availableCount = 0;
	vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &availableCount, nullptr);
	std::vector<VkExtensionProperties> available(availableCount);
	vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &availableCount, available.data());

	return filterAvailable(count, required, available);
}

#else

const NVSDK_NGX_FeatureCommonInfo* DLSSSupport::featureInfo()
{
	return nullptr;
}

std::optional<std::vector<const char*>> DLSSSupport::instanceExtensions()
{
	return std::nullopt;
}

std::optional<std::vector<const char*>> DLSSSupport::deviceExtensions(VkInstance, VkPhysicalDevice)
{
	return std::nullopt;
}

#endif
