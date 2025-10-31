#include "Mesh.h"

void Mesh::setup()
{
    // create buffers/arrays
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);
    // load data into vertex buffers
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);

    if (!indices.empty()) {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);
    }

    // set the vertex attribute pointers
    // vertex Positions
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    // vertex color
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    // vertex texture coords
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoords));
    // vertex normals
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal)); // use this vec3 to debug attributes
    // vertex tangent
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, tangent));
    // vertex bitangent
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, bitangent));
    //ids
    glEnableVertexAttribArray(6);
    glVertexAttribIPointer(6, 4, GL_INT, sizeof(Vertex), (void*)offsetof(Vertex, m_BoneIDs));
    // weights
    glEnableVertexAttribArray(7);
    glVertexAttribPointer(7, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, m_Weights));
    
    glBindVertexArray(0);
}

int Mesh::getNumVertices()
{
    !indices.empty()
        ? numVertices = static_cast<int>(indices.size() / 3)
        : numVertices = static_cast<int>(vertices.size() / 3);
    return numVertices;
}

Mesh::Mesh(std::vector<Vertex> vertices, std::vector<GLuint> indices, std::vector<Texture> textures)
{
	this->vertices = vertices;
	this->indices = indices;
	this->textures = textures;
	setup();
}

Mesh::~Mesh() = default;

void Mesh::Delete()
{
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    for (Texture& texture : textures) {
        texture.Delete();
    }
}

void Mesh::Draw(Shader& shader)
{
    shader.Activate();

    // bind appropriate textures
    unsigned int countDiffuse = 0;
    unsigned int countSpecular = 0;
    unsigned int countNormal = 0;
    unsigned int countHeight = 0;

    if (!textures.empty()) {
        for (unsigned int i = 0; i < textures.size(); i++) {
            textures[i].texUnit(shader, textures[i].type.c_str(), i);
            textures[i].Bind();
        }
    }

    // draw mesh
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, static_cast<unsigned int>(indices.size()), GL_UNSIGNED_INT, 0);
    
    glBindVertexArray(0);
    glActiveTexture(GL_TEXTURE0);
}
