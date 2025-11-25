#pragma once
#include <glad/glad.h> 

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <stb/stb_image.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp_glm_helpers.h>
#include "mesh.h"
#include "shader.h"
#include "texture.h"
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <map>
#include <vector>
#include <chrono>


struct BoneInfo
{
    /*id is index in finalBoneMatrices*/
    int id;

    /*offset matrix transforms vertex from model space to bone space*/
    glm::mat4 offset;

};

class Model
{
public:
    Model(const char* path);
    Model(const Model& other);
    Model(std::vector<Mesh> meshes, std::string path = "untitled");
    Model& operator=(const Model& other);
    ~Model();

    void Draw(Shader& shader);
    void Draw(Shader& shader, unsigned int numInstances);

    std::map<std::string, BoneInfo> GetBoneInfoMap();
    int& GetBoneCount();
    int getNumVertices();
    std::string getPath();
    std::string getDirectory();
    std::string getFileName();
    std::string getExtension();

public:
    std::map<std::string, BoneInfo> m_BoneInfoMap;
    std::unordered_map<std::string, Texture> loaded_textures;
    std::vector<Mesh> meshes;

private:
    int m_BoneCounter = 0;
    std::string path;
    std::string directory;
    std::string fileName;
    std::string extension;

    void loadDefaultTexture(const std::string& path, const std::string& type);
    void loadModel(std::string path);
    void processNode(aiNode* node, const aiScene* scene);
    Mesh processMesh(aiMesh* mesh, const aiScene* scene);
    std::vector<Texture> loadMaterialTextures(aiMaterial* mat, aiTextureType type, std::string typeName);


    void SetVertexBoneDataToDefault(Vertex& vertex);
    void SetVertexBoneData(Vertex& vertex, int boneID, float weight);
    void ExtractBoneWeightForVertices(std::vector<Vertex>& vertices, aiMesh* mesh, const aiScene* scene);
};