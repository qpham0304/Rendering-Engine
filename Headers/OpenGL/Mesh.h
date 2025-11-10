#pragma once
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include "Texture.h"

#define MAX_BONE_INFLUENCE 4

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

class Shader;

class Mesh
{
public:
	std::vector<Vertex> vertices;
	std::vector<unsigned int> indices;
	std::vector<Texture> textures;

public:
	int getNumVertices();
	Mesh(std::vector<Vertex> vertices, std::vector<unsigned int> indices, std::vector<Texture> textures);
	~Mesh();
	void Delete();
	void Draw(Shader& shader);

private:
	unsigned int VAO, VBO, EBO;
	int numVertices = 0;

private:
	void setup();

};

