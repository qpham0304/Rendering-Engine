#include "Stream.h"

#include "core/features/EngineUtils.h"

Stream::Stream(std::string_view m_path)
{
	std::vector<char> fileData = FileReader::readFileBinary(m_path);
	std::string jsonStr(fileData.begin(), fileData.end());
	jsonData = json::parse(jsonStr);
}

bool Stream::contains(std::string_view key)
{
	return jsonData.contains(key.data());
}
