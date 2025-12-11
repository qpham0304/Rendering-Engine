#pragma once
#include <string>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include "src/graphics/framework/OpenGL/core/TextureOpenGL.h"

#define MAX_BONE_INFLUENCE 4


class ShaderOpenGL;

class MeshOpenGL
{
public:
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

	std::vector<Vertex> vertices;
	std::vector<unsigned int> indices;
	std::vector<TextureOpenGL> textures;

public:
	MeshOpenGL(std::vector<Vertex> vertices, std::vector<unsigned int> indices, std::vector<TextureOpenGL> textures);
	~MeshOpenGL();
	
	void Delete();
	void Draw(ShaderOpenGL& shader);
	int GetNumVertices();

private:
	void setup();

private:
	unsigned int VAO, VBO, EBO;
	int numVertices = 0;

};

