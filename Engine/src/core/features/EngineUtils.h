#pragma once

#include <cstring>
#include <cassert>
#include <string>
#include <chrono>
#include <iostream>
#include <algorithm>
#include <cstdlib>
#include <limits>
#include <optional>
#include <set>
#include <glm/glm.hpp>
#include <array>
#include <random>
#include <cstdint>
#include <vector>
#include <stdexcept>
#include <fstream>
#include "Mesh.h"

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
    inline Mesh drawSphere(float radius, int sectorCount, int stackCount) 
    {
        Mesh data;
        const float M_PI = 3.14159265359;

        for (int i = 0; i <= stackCount; ++i) {
            float stackAngle = M_PI / 2 - i * (M_PI / stackCount);
            float xy = radius * cosf(stackAngle);
            float z = radius * sinf(stackAngle);

            for (int j = 0; j <= sectorCount; ++j) {
                float sectorAngle = j * (2 * M_PI / sectorCount);

                float x = xy * cosf(sectorAngle);
                float y = xy * sinf(sectorAngle);

                float s = (float)j / sectorCount;
                float t = (float)i / stackCount;

                // Normalized vector for color/normals
                float nx = x / radius;
                float ny = y / radius;
                float nz = z / radius;

                data.vertices.push_back({ {x, y, z}, {nx, ny, nz}, {s, t} });
            }
        }

        for (int i = 0; i < stackCount; ++i) {
            int k1 = i * (sectorCount + 1);     // beginning of current stack
            int k2 = k1 + sectorCount + 1;      // beginning of next stack

            for (int j = 0; j < sectorCount; ++j, ++k1, ++k2) {
                if (i != 0) {
                    data.indices.push_back(k1);
                    data.indices.push_back(k2);
                    data.indices.push_back(k1 + 1);
                }

                if (i != (stackCount - 1)) {
                    data.indices.push_back(k1 + 1);
                    data.indices.push_back(k2);
                    data.indices.push_back(k2 + 1);
                }
            }
        }
        return data;
    }

    inline Mesh drawQuad(float size = 1.0f) {
        Mesh data;
        float h = size * 0.5f;
        
        data.vertices = {
            {{-h, -h, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}}, // Bottom Left
            {{ h, -h, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}}, // Bottom Right
            {{ h,  h, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}}, // Top Right
            {{-h,  h, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}}  // Top Left
        };

        data.indices = {
            0, 1, 2, 
            2, 3, 0
        };

        return data;
    }

    inline Mesh drawCube(float size) 
    {
        Mesh data;
        float h = size * 0.5f;

        struct Face {
            float v[12];
            float n[3];
        };

        Face faces[6] = {
            { {-h,-h, h,  h,-h, h,  h, h, h, -h, h, h}, { 0, 0, 1} }, // Front
            { { h,-h,-h, -h,-h,-h, -h, h,-h,  h, h,-h}, { 0, 0,-1} }, // Back
            { {-h, h, h,  h, h, h,  h, h,-h, -h, h,-h}, { 0, 1, 0} }, // Top
            { {-h,-h,-h,  h,-h,-h,  h,-h, h, -h,-h, h}, { 0,-1, 0} }, // Bottom
            { { h,-h, h,  h,-h,-h,  h, h,-h,  h, h, h}, { 1, 0, 0} }, // Right
            { {-h,-h,-h, -h,-h, h, -h, h, h, -h, h,-h}, {-1, 0, 0} }  // Left
        };

        for (int i = 0; i < 6; ++i) {
            for (int j = 0; j < 4; ++j) {
                float x = faces[i].v[j * 3 + 0];
                float y = faces[i].v[j * 3 + 1];
                float z = faces[i].v[j * 3 + 2];

                float s = (j == 1 || j == 2) ? 1.0f : 0.0f;
                float t = (j == 2 || j == 3) ? 1.0f : 0.0f;

                data.vertices.push_back({ {x, y, z}, {faces[i].n[0], faces[i].n[1], faces[i].n[2]}, {s, t} });
            }

            int startIndex = i * 4;
            data.indices.push_back(startIndex + 0);
            data.indices.push_back(startIndex + 1);
            data.indices.push_back(startIndex + 2);
            data.indices.push_back(startIndex + 0);
            data.indices.push_back(startIndex + 2);
            data.indices.push_back(startIndex + 3);
        }

        return data;
    }
};

