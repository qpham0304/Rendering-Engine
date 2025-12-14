#include "ModelManager.h"
#include "TextureManager.h"
#include "MeshManager.h"
#include "MaterialManager.h"
#include "logging/Logger.h"
#include "core/features/ServiceLocator.h"
#include "core/features/Timer.h"
#include "core/features/Mesh.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp_glm_helpers.h>


ModelManager::ModelManager()
{

}

ModelManager::~ModelManager()
{

}

int ModelManager::init(WindowConfig config)
{
    Service::init(config);

    m_logger = &ServiceLocator::GetService<Logger>("Engine_LoggerPSD");
    textureManager = &ServiceLocator::GetService<TextureManager>("TextureManager");
    meshManager = &ServiceLocator::GetService<MeshManager>("MeshManager");
    if (!(m_logger && textureManager && meshManager)) {
        return -1;
    }
    return 0;
}

int ModelManager::onClose()
{
    WriteLock lock = _lockWrite();
    m_models.clear();
    m_modelData.clear();

    return 0;
}

void ModelManager::destroy(uint32_t id)
{
    WriteLock lock = _lockWrite();
    if(m_models.find(id) == m_models.end()) {
        throw std::runtime_error("Cannot find model with given ID");
    }
    m_models.erase(id);
}

uint32_t ModelManager::loadModel(std::string_view path)
{
    _loadModel(path);

    return _assignID();
}

Model* ModelManager::getModel(uint32_t id)
{
    if (m_models.find(id) == m_models.end()) {
        return nullptr;
    }
    return m_models[id].get();
}

void ModelManager::_loadModel(std::string_view path) 
{
    std::string loadTimer = std::string("Model loading ") + path.data();
    Timer(loadTimer.c_str());

    std::string directory = std::string(path).substr(0, path.find_last_of('/'));
    std::string fileName = std::string(path).substr(path.find_last_of('/') + 1);

    Assimp::Importer import;
    unsigned int flags = aiProcess_Triangulate
        | aiProcess_GenSmoothNormals
        | aiProcess_GlobalScale
        | aiProcess_FlipUVs
        | aiProcess_CalcTangentSpace
        | aiProcess_SplitByBoneCount
        | aiProcess_LimitBoneWeights
        | aiProcess_JoinIdenticalVertices
        | aiProcess_ValidateDataStructure;

    const aiScene* scene = import.ReadFile(path.data(), flags);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::string error = import.GetErrorString();
        m_logger->error("Model Loading failed: ERROR::ASSIMP::{}", error);
    }

    std::vector<uint32_t> meshes = {};
    _processNode(scene->mRootNode, scene, meshes, directory);
    
    m_modelData[path.data()] = m_ids;
    m_models[m_ids] = std::make_shared<Model>();

    for (const auto& mesh : meshes) {
        m_models[m_ids]->meshIDs.push_back(mesh);
    }

}

void ModelManager::_processNode(aiNode* node, const aiScene* scene, std::vector<uint32_t>& meshes, std::string_view directory)
{
    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        meshes.push_back(_processMesh(mesh, scene, directory));
    }

    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        _processNode(node->mChildren[i], scene, meshes, directory);
    }
}

uint32_t ModelManager::_processMesh(aiMesh* mesh, const aiScene* scene, std::string_view directory)
{
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;
    std::vector<uint8_t> textures;

    // process vertices
    for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
        Vertex vertex;
        glm::vec3 vector;
        //SetVertexBoneDataToDefault(vertex);

        // process vertex positions, normals and texture coordinates
        vertex.positions = AssimpGLMHelpers::GetGLMVec(mesh->mVertices[i]);
        vertex.normal = AssimpGLMHelpers::GetGLMVec(mesh->mNormals[i]);

        // does the mesh contain texture coordinates?
        // a vertex can contain up to 8 different texture coordinates. We thus make the assumption that we won't 
        // use models where a vertex can have multiple texture coordinates so we always take the first set (0).
        if (mesh->mTextureCoords[0]){
            glm::vec2 vec;
            vec.x = mesh->mTextureCoords[0][i].x;
            vec.y = mesh->mTextureCoords[0][i].y;
            vertex.texCoords = vec;
            vector.x = mesh->mTangents[i].x;
            vector.y = mesh->mTangents[i].y;
            vector.z = mesh->mTangents[i].z;
            vertex.tangent = vector;
            vector.x = mesh->mBitangents[i].x;
            vector.y = mesh->mBitangents[i].y;
            vector.z = mesh->mBitangents[i].z;
            vertex.bitangent = vector;
        } else {
            vertex.texCoords = glm::vec2(0.0f, 0.0f);
        }

        vertices.push_back(vertex);
    }

    // process indices
    for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
        aiFace face = mesh->mFaces[i];
        
        for (unsigned int j = 0; j < face.mNumIndices; j++) {
            indices.push_back(face.mIndices[j]);
        }
    }

    // process material
    if (mesh->mMaterialIndex >= 0)
    {
        aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
        std::vector<uint8_t> albedoMaps;
        std::vector<uint8_t> normalMaps;
        std::vector<uint8_t> metalnessMaps;
        std::vector<uint8_t> roughnessMaps;
        std::vector<uint8_t> aoMaps;
        std::vector<uint8_t> emissiveMaps;

        //TODO: find a better way to support materials PBR or not might need different descriptors
        std::string extension = ".gltf";    
        if (extension == ".gltf") {
           albedoMaps = _loadMaterialTextures(material, aiTextureType_BASE_COLOR, "albedoMap", directory);
           textures.insert(textures.end(), albedoMaps.begin(), albedoMaps.end());

           normalMaps = _loadMaterialTextures(material, aiTextureType_NORMALS, "normalMap", directory);
           textures.insert(textures.end(), normalMaps.begin(), normalMaps.end());

           metalnessMaps = _loadMaterialTextures(material, aiTextureType_METALNESS, "metallicMap", directory);
           textures.insert(textures.end(), metalnessMaps.begin(), metalnessMaps.end());

           roughnessMaps = _loadMaterialTextures(material, aiTextureType_DIFFUSE_ROUGHNESS, "roughnessMap", directory);
           textures.insert(textures.end(), roughnessMaps.begin(), roughnessMaps.end());

           aoMaps = _loadMaterialTextures(material, aiTextureType_LIGHTMAP, "aoMap", directory);
           textures.insert(textures.end(), aoMaps.begin(), aoMaps.end());

           emissiveMaps = _loadMaterialTextures(material, aiTextureType_EMISSIVE, "emissiveMap", directory);
           textures.insert(textures.end(), emissiveMaps.begin(), emissiveMaps.end());
        }

        // try to set up as much materials as possible (might look wrong in PBR shading)
        else {
            albedoMaps = _loadMaterialTextures(material, aiTextureType_DIFFUSE, "albedoMap", directory);
            textures.insert(textures.end(), albedoMaps.begin(), albedoMaps.end());

            normalMaps = _loadMaterialTextures(material, aiTextureType_NORMALS, "normalMap", directory);
            textures.insert(textures.end(), normalMaps.begin(), normalMaps.end());

            roughnessMaps = _loadMaterialTextures(material, aiTextureType_SPECULAR, "metallicMap", directory);
            textures.insert(textures.end(), roughnessMaps.begin(), roughnessMaps.end());

            roughnessMaps = _loadMaterialTextures(material, aiTextureType_SHININESS, "roughnessMap", directory);
            textures.insert(textures.end(), roughnessMaps.begin(), roughnessMaps.end());

            aoMaps = _loadMaterialTextures(material, aiTextureType_AMBIENT, "aoMap", directory);
            textures.insert(textures.end(), aoMaps.begin(), aoMaps.end());

            emissiveMaps = _loadMaterialTextures(material, aiTextureType_EMISSIVE, "emissiveMap", directory);
            textures.insert(textures.end(), emissiveMaps.begin(), emissiveMaps.end());
        }
    }

    //ExtractBoneWeightForVertices(vertices, mesh, scene);

    Mesh m;
    m.vertices = vertices;
    m.indices = indices;
    m.materialIDs = textures;
    
    return meshManager->loadMesh(m);
}

std::vector<uint8_t> ModelManager::_loadMaterialTextures(aiMaterial* mat, aiTextureType type, std::string typeName, std::string_view directory)
{
    std::vector<uint8_t> textureIDs;
    for (unsigned int i = 0; i < mat->GetTextureCount(type); i++)
    {
        aiString str;
        mat->GetTexture(type, i, &str);

        std::string path = std::string(directory) + '/' + std::string(str.C_Str());
        uint8_t textureID = textureManager->loadTexture(path.data());
        textureIDs.push_back(textureID);
    }


    return textureIDs;
}


//void ModelManager::ExtractBoneWeightForVertices(std::vector<MeshOpenGL::Vertex>& vertices, aiMesh* mesh, const aiScene* scene)
//{
//    auto& boneInfoMap = m_BoneInfoMap;
//    int& boneCount = m_BoneCounter;
//
//    for (unsigned int boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex) {
//        int boneID = -1;
//        std::string boneName = mesh->mBones[boneIndex]->mName.C_Str();
//        if (boneInfoMap.find(boneName) == boneInfoMap.end()) {
//            BoneInfo newBoneInfo;
//            newBoneInfo.id = boneCount;
//            newBoneInfo.offset = AssimpGLMHelpers::ConvertMatrixToGLMFormat(mesh->mBones[boneIndex]->mOffsetMatrix);
//            boneInfoMap[boneName] = newBoneInfo;
//            boneID = boneCount;
//            boneCount++;
//        }
//
//        else {
//            boneID = boneInfoMap[boneName].id;
//        }
//
//        assert(boneID != -1);
//        auto weights = mesh->mBones[boneIndex]->mWeights;
//        int numWeights = mesh->mBones[boneIndex]->mNumWeights;
//
//        for (int weightIndex = 0; weightIndex < numWeights; ++weightIndex) {
//            int vertexId = weights[weightIndex].mVertexId;
//            float weight = weights[weightIndex].mWeight;
//            assert(vertexId <= vertices.size());
//            SetVertexBoneData(vertices[vertexId], boneID, weight);
//        }
//    }
//}
