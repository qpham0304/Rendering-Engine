#pragma once

#include <iostream>
#include <cstring>
#include <cassert>
#include <cstdlib>
#include <glm/glm.hpp>
#include <cstdint>
#include <vector>
#include <stdexcept>
#include <fstream>
#include "Mesh.h"
#include "Random.h"


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

namespace EngineUtils
{
    inline Mesh drawSphere(float radius, int sectorCount, int stackCount) 
    {
        Mesh data;
        const float M_PI = 3.14159265359f;

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

                glm::vec3 pos(x, y, z);
                glm::vec3 normal = glm::normalize(pos);

                // Tangent is the derivative with respect to sectorAngle (longitude)
                // Bitangent is the derivative with respect to stackAngle (latitude)
                glm::vec3 tangent(-sinf(sectorAngle), cosf(sectorAngle), 0.0f);
                glm::vec3 bitangent = glm::cross(normal, tangent);

                Vertex v{};
                v.positions = pos;
                v.normal = normal;
                v.texCoords = {s, t};
                v.tangent = tangent;
                v.bitangent = bitangent;
                v.color = glm::vec3(1.0f); // Default white

                data.vertices.push_back(v);
            }
        }

        for (int i = 0; i < stackCount; ++i) {
            int k1 = i * (sectorCount + 1);
            int k2 = k1 + sectorCount + 1;
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
        
        // For a flat quad facing +Z: 
        // Tangent follows +X (U direction), Bitangent follows +Y (V direction)
        glm::vec3 n = {0.0f, 0.0f, 1.0f};
        glm::vec3 tan = {1.0f, 0.0f, 0.0f};
        glm::vec3 bitan = {0.0f, 1.0f, 0.0f};

        data.vertices = {
            {{-h, -h, 0.0f}, {1,1,1}, {0.0f, 0.0f}, n, tan, bitan},
            {{ h, -h, 0.0f}, {1,1,1}, {1.0f, 0.0f}, n, tan, bitan},
            {{ h,  h, 0.0f}, {1,1,1}, {1.0f, 1.0f}, n, tan, bitan},
            {{-h,  h, 0.0f}, {1,1,1}, {0.0f, 1.0f}, n, tan, bitan}
        };

        data.indices = { 0, 1, 2, 2, 3, 0 };
        return data;
    }

    inline Mesh drawCube(float size) 
    {
        Mesh data;
        float h = size * 0.5f;

        struct Face {
            float v[12];
            glm::vec3 n;
            glm::vec3 tan;
            glm::vec3 bitan;
        };

        // Pre-calculating tangents/bitangents for each cardinal face
        Face faces[6] = {
            { {-h,-h, h,  h,-h, h,  h, h, h, -h, h, h}, { 0, 0, 1}, { 1, 0, 0}, { 0, 1, 0} }, // Front
            { { h,-h,-h, -h,-h,-h, -h, h,-h,  h, h,-h}, { 0, 0,-1}, {-1, 0, 0}, { 0, 1, 0} }, // Back
            { {-h, h, h,  h, h, h,  h, h,-h, -h, h,-h}, { 0, 1, 0}, { 1, 0, 0}, { 0, 0,-1} }, // Top
            { {-h,-h,-h,  h,-h,-h,  h,-h, h, -h,-h, h}, { 0,-1, 0}, { 1, 0, 0}, { 0, 0, 1} }, // Bottom
            { { h,-h, h,  h,-h,-h,  h, h,-h,  h, h, h}, { 1, 0, 0}, { 0, 0,-1}, { 0, 1, 0} }, // Right
            { {-h,-h,-h, -h,-h, h, -h, h, h, -h, h,-h}, {-1, 0, 0}, { 0, 0, 1}, { 0, 1, 0} }  // Left
        };

        for (int i = 0; i < 6; ++i) {
            for (int j = 0; j < 4; ++j) {
                Vertex v{};
                v.positions = { faces[i].v[j * 3 + 0], faces[i].v[j * 3 + 1], faces[i].v[j * 3 + 2] };
                v.normal    = faces[i].n;
                v.tangent   = faces[i].tan;
                v.bitangent = faces[i].bitan;
                v.texCoords = { (j == 1 || j == 2) ? 1.0f : 0.0f, (j == 2 || j == 3) ? 1.0f : 0.0f };
                v.color     = glm::vec3(1.0f);
                data.vertices.push_back(v);
            }

            int start = i * 4;
            data.indices.insert(data.indices.end(), { (uint16_t)start, (uint16_t)(start+1), (uint16_t)(start+2), 
                                                    (uint16_t)start, (uint16_t)(start+2), (uint16_t)(start+3) });
        }

        return data;
    }
};

