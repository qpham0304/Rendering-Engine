#pragma once

#include <vector>
#include <glm/glm.hpp>

#define MAX_BONE_INFLUENCE 4

//TODO: no way to get attribute descriptions here yet
// use the one mirrored in vulkan device for now

struct Vertex {
	glm::vec3 positions;
	glm::vec3 color;
	glm::vec2 texCoords;
	glm::vec3 normal;
	glm::vec3 tangent;
	glm::vec3 bitangent;

	int m_BoneIDs[MAX_BONE_INFLUENCE];		//bone indexes which will influence this vertex
	float m_Weights[MAX_BONE_INFLUENCE];	//weights from each bone
};


struct Mesh {
	std::vector<Vertex> vertices;
	std::vector<uint16_t> indices;
	uint32_t materialID;
};
