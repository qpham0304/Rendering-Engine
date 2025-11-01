#pragma once

#include <vector>
#include <cstring>
#include <cassert>
#include <string>
#include <stdexcept>
#include <chrono>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cstdlib>
#include <cstdint>
#include <limits>
#include <optional>
#include <set>
#include <glm/glm.hpp>
#include <array>

namespace FileReader {

	static std::vector<char> readFileBinary(std::string_view filename) {
		std::ifstream file(filename.data(), std::ios::ate | std::ios::binary);

		if (!file.is_open()) {
			throw std::runtime_error("failed to open file!");
		}

		size_t fileSize = (size_t)file.tellg();
		std::vector<char> buffer(fileSize);

		file.seekg(0);
		file.read(buffer.data(), fileSize);

		file.close();

		return buffer;
	}

}

class EngineUtils
{

};

