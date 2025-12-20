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
#include <random>

namespace FileReader {

	static std::vector<char> readFileBinary(std::string_view filename) {
		std::ifstream file(filename.data(), std::ios::ate | std::ios::binary);

		if (!file.is_open()) {
			throw std::runtime_error("EngineUtil::readFileBinary::failed to open file!");
		}

		size_t fileSize = (size_t)file.tellg();
		std::vector<char> buffer(fileSize);

		file.seekg(0);
		file.read(buffer.data(), fileSize);

		file.close();

		return buffer;
	}

}

inline uint64_t genUUID() {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dist;
    return dist(gen);
}

namespace EngineUtils
{
	

};

