#pragma once

#include <vector>
#include <unordered_map>
#include <string>
#include <atomic>
#include <memory>
#include <stdexcept>

struct Model {
	std::vector<uint32_t> meshIDs;
	std::vector<uint32_t> materialsID;
	std::vector<uint32_t> textureIDs;
};

class ModelsManager
{
public:
	ModelsManager();
	~ModelsManager();

	void loadModel(std::string m_path);

private:
	std::unordered_map<uint32_t, std::shared_ptr<Model>> models;
	std::atomic<uint32_t> m_id;
};

