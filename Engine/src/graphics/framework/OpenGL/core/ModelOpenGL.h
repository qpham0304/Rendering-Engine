#pragma once
#include <glad/glad.h> 

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <stb/stb_image.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp_glm_helpers.h>
#include "MeshOpenGL.h"
#include "graphics/framework/OpenGL/core/ShaderOpenGL.h"
#include "graphics/framework/OpenGL/core/TextureOpenGL.h"

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
    int id = 0;

    /*offset matrix transforms vertex from model space to bone space*/
    glm::mat4 offset{};

};

class ModelOpenGL
{
public:
    ModelOpenGL(const char* path);
    ModelOpenGL(const ModelOpenGL& other);
    ModelOpenGL(std::vector<MeshOpenGL> meshes, std::string path = "untitled");
    ModelOpenGL& operator=(const ModelOpenGL& other);
    ~ModelOpenGL();

    void Draw(ShaderOpenGL& shader);
    void Draw(ShaderOpenGL& shader, unsigned int numInstances);

    std::map<std::string, BoneInfo> GetBoneInfoMap();
    int& GetBoneCount();
    int getNumVertices();
    std::string getPath();
    std::string getDirectory();
    std::string getFileName();
    std::string getExtension();

public:
    std::map<std::string, BoneInfo> m_BoneInfoMap;
    std::unordered_map<std::string, TextureOpenGL> loaded_textures;
    std::vector<MeshOpenGL> meshes;

private:
    int m_BoneCounter = 0;
    std::string path;
    std::string directory;
    std::string fileName;
    std::string extension;

    void loadDefaultTexture(const std::string& path, const std::string& type);
    void loadModel(std::string path);
    void processNode(aiNode* node, const aiScene* scene);
    MeshOpenGL processMesh(aiMesh* mesh, const aiScene* scene);
    std::vector<TextureOpenGL> loadMaterialTextures(aiMaterial* mat, aiTextureType type, std::string typeName);


    void SetVertexBoneDataToDefault(MeshOpenGL::Vertex& vertex);
    void SetVertexBoneData(MeshOpenGL::Vertex& vertex, int boneID, float weight);
    void ExtractBoneWeightForVertices(std::vector<MeshOpenGL::Vertex>& vertices, aiMesh* mesh, const aiScene* scene);
};