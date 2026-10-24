#include "GraphicsSettingsSerializer.h"

#include "GraphicsSettings.h"
#include "core/features/ServiceLocator.h"
#include "logging/Logger.h"

#include <filesystem>
#include <fstream>

#define REGISTER_SETTINGS(Type) \
	settings_handlers.emplace(std::type_index(typeid(Type)), SettingsHandler{ \
		#Type, \
		&GraphicsSettingsSerializerInternal<Type>::Serialize, \
		&GraphicsSettingsSerializerInternal<Type>::Deserialize \
	})

GraphicsSettingsSerializer::GraphicsSettingsSerializer()
	: Serializable()
{
	setFootprint("GraphicsSettingsSerializer", sizeof(GraphicsSettingsSerializer), MemorySystem::Graphics);
	RegisterSettings();
}

void GraphicsSettingsSerializer::RegisterSettings()
{
	if (!settings_handlers.empty()) {
		return;
	}

	REGISTER_SETTINGS(BloomSettings);
	REGISTER_SETTINGS(ShadowMapSettings);
	REGISTER_SETTINGS(LightingSettings);
	REGISTER_SETTINGS(AmbientOcclusionSettings);
	REGISTER_SETTINGS(ImageBasedSettings);
	REGISTER_SETTINGS(DeferredSettings);
	REGISTER_SETTINGS(DDGISettings);
}

bool GraphicsSettingsSerializer::saveSettings(std::string_view filePath) const
{
	Logger& logger = ServiceLocator::GetService<Logger>("Engine_LoggerSPD");

	std::filesystem::path path(filePath);
	if (path.has_parent_path()) {
		std::filesystem::create_directories(path.parent_path());
	}

	nlohmann::json settingsJson;
	serialize(settingsJson);

	std::ofstream out(path, std::ios::out | std::ios::trunc);
	if (!out) {
		logger.error("Stream error: Could not open file at {}", filePath);
		return false;
	}

	try {
		out << settingsJson.dump(2) << std::endl;
		out.flush();
		if (out.fail()) {
			logger.error("Failed writing data to disk for {}", filePath);
			return false;
		}
	}
	catch (const std::exception& e) {
		logger.error("JSON/File error: {}", e.what());
		return false;
	}

	logger.info("Graphics settings saved: {} settings written to {}", settingsJson["graphics_settings"].size(), filePath);
	return true;
}

bool GraphicsSettingsSerializer::loadSettings(std::string_view filePath)
{
	Logger& logger = ServiceLocator::GetService<Logger>("Engine_LoggerSPD");

	nlohmann::json settingsJson;
	if (!loadJson(std::string(filePath), logger, settingsJson) || !settingsJson.contains("graphics_settings")) {
		logger.error("missing graphics settings in {}", filePath);
		return false;
	}

	deserialize(settingsJson);
	logger.info("Graphics settings loaded from {}", filePath);
	return true;
}

void GraphicsSettingsSerializer::serialize(nlohmann::json& j) const
{
	Logger& logger = ServiceLocator::GetService<Logger>("Engine_LoggerSPD");

	nlohmann::json settings = nlohmann::json::object();
	for (const Entry& entry : m_entries) {
		const SettingsHandler* handler = _findHandler(entry.type);
		if (!handler) {
			logger.warn("Settings '{}' are not saved, register their type in GraphicsSettingsSerializer::RegisterSettings", entry.name);
			continue;
		}
		settings[handler->typeName] = handler->save(entry.data.get());
	}
	j["graphics_settings"] = std::move(settings);
}

void GraphicsSettingsSerializer::deserialize(const nlohmann::json& j)
{
	if (!j.contains("graphics_settings") || !j["graphics_settings"].is_object()) {
		ServiceLocator::GetService<Logger>("Engine_LoggerSPD").error("GraphicsSettingsSerializer: expected a graphics_settings object");
		return;
	}

	m_loaded = j["graphics_settings"];
	for (Entry& entry : m_entries) {
		_applyLoaded(entry);
	}
}

const GraphicsSettingsSerializer::SettingsHandler* GraphicsSettingsSerializer::_findHandler(std::type_index type)
{
	RegisterSettings();
	auto it = settings_handlers.find(type);
	return it != settings_handlers.end() ? &it->second : nullptr;
}

void GraphicsSettingsSerializer::_applyLoaded(Entry& entry) const
{
	const SettingsHandler* handler = _findHandler(entry.type);
	if (!handler || !m_loaded.contains(handler->typeName)) {
		return;
	}

	// a field of the wrong type stops this entry only, the rest of the file still loads
	try {
		handler->load(entry.data.get(), m_loaded.at(handler->typeName));
	} catch (const nlohmann::json::exception& e) {
		ServiceLocator::GetService<Logger>("Engine_LoggerSPD").warn("Failed to load settings '{}': {}", entry.name, e.what());
	}
}
