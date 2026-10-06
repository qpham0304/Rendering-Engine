#pragma once

#include <vector>

struct Model {
	std::vector<uint32_t> meshIDs;
	uint32_t skeletonID;
	std::string path = "None";
};
