#include "Serializable.h"
#include "logging/Logger.h"

#include <fstream>

bool Serializable::loadJson(const std::string& path, Logger& logger, nlohmann::json& json)
{
	std::ifstream file(path);
	if (!file.is_open()) {
		logger.error("Failed to open json: {}", path);
		return false;
	}

	// noexcept, returns a discarded json object on error
	json = nlohmann::json::parse(file, nullptr, false);
	if (json.is_discarded()) {
		logger.error("Failed to parse json: {}", path);
		json = nlohmann::json();
		return false;
	}

	return true;
}
