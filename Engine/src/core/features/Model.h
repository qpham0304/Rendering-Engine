#pragma once

#include <vector>

struct Model {
	std::vector<uint32_t> meshIDs {};
	uint32_t skeletonID { 0 };
	uint32_t animationID { 0 };
	std::string path { "None" };
};
