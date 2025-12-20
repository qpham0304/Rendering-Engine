#include "ModelManager.h"
#include "TextureManager.h"
#include "MeshManager.h"
#include "MaterialManager.h"
#include "logging/Logger.h"
#include "core/features/ServiceLocator.h"
#include "core/features/Timer.h"
#include "core/features/Mesh.h"
#include "core/features/Material.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp_glm_helpers.h>
#include "core/events/EventManager.h"
#include "core/events/Event.h"

ModelManager::ModelManager()
    :   Manager("ModelManager")
{

}

ModelManager::~ModelManager()
{

}

bool ModelManager::init(WindowConfig config)
{
    Service::init(config);

    m_logger = &ServiceLocator::GetService<Logger>("Engine_LoggerPSD");
    textureManager = &ServiceLocator::GetService<TextureManager>("TextureManagerVulkan");
    meshManager = &ServiceLocator::GetService<MeshManager>("MeshManager");
    materialManager = &ServiceLocator::GetService<MaterialManager>("MaterialManagerVulkan");
    
    if (!(m_logger && textureManager && meshManager && materialManager)) {
        return false;
    }

    EventManager& eventManager = EventManager::getInstance();
    eventManager.subscribe(EventType::ModelLoadEvent, [&] (Event& event) {
        ModelLoadEvent& e = static_cast<ModelLoadEvent&>(event);

        if (!e.entity.hasComponent<ModelComponent>()) {
			e.entity.addComponent<ModelComponent>();
		}
		
		ModelComponent& component = e.entity.getComponent<ModelComponent>();
		component.path = "Loading...";
        uint32_t modelID = loadModel(e.path);

		if (component.path != e.path && modelID != 0) {
			component.path = e.path;
            component.modelID = modelID;
		}
		
		else {
            //TODO: imgui or a widget register an event to popup a warning message
            //ideally editor handle it or supress it for run time version
            std::string message = "Failed to load Model from path: " + e.path;
            m_logger->critical(message);

            GuiMessageEvent failEvent(message);
            eventManager.publish(failEvent);
		}
    });

    return true;
}

bool ModelManager::onClose()
{
    WriteLock lock = _lockWrite();
    m_models.clear();
    m_modelData.clear();

    return true;
}

void ModelManager::destroy(uint32_t id)
{
    WriteLock lock = _lockWrite();
    if(m_models.find(id) == m_models.end()) {
        throw std::runtime_error("Cannot find model with given ID");
    }
    m_models.erase(id);
}

std::vector<uint32_t> ModelManager::listIDs() const
{
    std::vector<uint32_t> list;
    for(const auto& [id, texture] : m_models) {
        list.emplace_back(id);
    }
    return list;
}

uint32_t ModelManager::loadModel(std::string_view path)
{
    _loadModel(path);

    return _assignID();
}

const Model* ModelManager::getModel(uint32_t id) const
{
    if (m_models.find(id) == m_models.end()) {
        return nullptr;
    }
    return m_models.at(id).get();
}

void ModelManager::_loadModel(std::string_view path) 
{
    // if (m_modelData.find(path.data()) != m_modelData.end()) {
    //     return;
    // }

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
    
    // process vertices
    for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
        Vertex vertex;
        glm::vec3 vector;
        //SetVertexBoneDataToDefault(vertex);

        // process vertex positions, normals and texture coordinates
        vertex.positions = AssimpGLMHelpers::GetGLMVec(mesh->mVertices[i]);
        vertex.normal = AssimpGLMHelpers::GetGLMVec(mesh->mNormals[i]);

        // does the mesh contain texture coordinates?
        // a vertex can contain up to 8 different texture coordinates.
        // We thus make the assumption that we won't 
        // use models where a vertex can have multiple texture coordinates
        // so we always take the first set (0).
        if (mesh->mTextureCoords[0]) {
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
    MaterialDesc materialDesc{};

    if (mesh->mMaterialIndex >= 0) {
        aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
        
        materialDesc.albedoIDs = _loadMaterial(material, aiTextureType_BASE_COLOR, "albedoMap", directory);
        materialDesc.normalIDs = _loadMaterial(material, aiTextureType_NORMALS, "normalMap", directory);
        materialDesc.metallicIDs = _loadMaterial(material, aiTextureType_METALNESS, "metallicMap", directory);
        materialDesc.roughnessIDs = _loadMaterial(material, aiTextureType_DIFFUSE_ROUGHNESS, "roughnessMap", directory);
        materialDesc.aoIDs = _loadMaterial(material, aiTextureType_LIGHTMAP, "aoMap", directory);
        materialDesc.emissiveIDs = _loadMaterial(material, aiTextureType_EMISSIVE, "emissiveMap", directory);

        // try find other non compatible materials, if still fail...
        // give up and let material manager use fallback materials
        if(materialDesc.metallicIDs.empty()) {  //TODO: need adjustment as this is not the correct pbr fallback
            materialDesc.metallicIDs = _loadMaterial(material, aiTextureType_SPECULAR, "metallicMap", directory);
        }
        if(materialDesc.albedoIDs.empty()){
            materialDesc.albedoIDs = _loadMaterial(material, aiTextureType_DIFFUSE, "albedoMap", directory);
        }
        if(materialDesc.roughnessIDs.empty()) {
            materialDesc.roughnessIDs = _loadMaterial(material, aiTextureType_SHININESS, "roughnessMap", directory);
        }
        if(materialDesc.aoIDs.empty()) {
            materialDesc.aoIDs = _loadMaterial(material, aiTextureType_AMBIENT, "aoMap", directory);
        }
    }

    //ExtractBoneWeightForVertices(vertices, mesh, scene);
    
    Mesh m;
    m.vertices = vertices;
    m.indices = indices;
    m.materialID = materialManager->createMaterial(materialDesc);
    
    return meshManager->loadMesh(m);
}

std::vector<uint32_t> ModelManager::_loadMaterial(aiMaterial* mat, aiTextureType type, std::string typeName, std::string_view directory)
{
    std::vector<uint32_t> textureIDs;
    for (unsigned int i = 0; i < mat->GetTextureCount(type); i++)
    {
        aiString str;
        mat->GetTexture(type, i, &str);

        std::string path = std::string(directory) + '/' + std::string(str.C_Str());
        uint32_t textureID = textureManager->loadTexture(path.data());
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
