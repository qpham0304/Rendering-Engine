#pragma once

#include <volk.h>
#include <vector>
#include <optional>

struct NVSDK_NGX_FeatureCommonInfo;

// what NGX needs from the instance and device before DLSS can be created on them,
// nullopt when the engine is built without DLSS or the driver / gpu can't run it
namespace DLSSSupport {
	inline constexpr const char* ProjectID = "6c0f1e2a-8d3b-4f57-9a61-2b7d4e9c3f10";
	inline constexpr const char* EngineVersion = "1.0";

	const NVSDK_NGX_FeatureCommonInfo* featureInfo();
	std::optional<std::vector<const char*>> instanceExtensions();
	std::optional<std::vector<const char*>> deviceExtensions(VkInstance instance, VkPhysicalDevice physicalDevice);
}
