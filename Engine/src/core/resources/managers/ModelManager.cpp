#include "ModelManager.h"
#include "TextureManager.h"
#include "MeshManager.h"
#include "SkeletonManager.h"
#include "MaterialManager.h"
#include "logging/Logger.h"
#include "animation/AnimationManager.h"
#include "core/features/ServiceLocator.h"
#include "core/features/Timer.h"
#include "core/features/Material.h"
#include "core/events/EventManager.h"
#include "core/events/Event.h"
#include "core/features/EngineUtils.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp_glm_helpers.h>
#include <functional>
#include <algorithm>
#include <set>

const std::unordered_map<std::string, std::function<std::pair<std::string, Mesh>()>> meshPrimitives = {
    { "$prim$Sphere", []() { return std::pair<std::string, Mesh>( "$prim$Sphere", EngineUtils::drawSphere(0.5f, 36, 36) ); }},
    { "$prim$Quad",   []() { return std::pair<std::string, Mesh>( "$prim$Quad", EngineUtils::drawQuad() ); }},
    { "$prim$Cube",   []() { return std::pair<std::string, Mesh>( "$prim$Cube", EngineUtils::drawCube(1.0) ); } }
    // { "$prim$Capsule",[]() { return std::pair<std::string, Mesh>( "$prim$Capsule", EngineUtils::drawCapsule(1.0) ); } },
    // { "$prim$Convex", []() { return std::pair<std::string, Mesh>( "$prim$Convex", EngineUtils::drawConvex(1.0) ); } }
};


ModelManager::ModelManager()
    :   Manager("ModelManager")
{
    setFootprint("ModelManager", sizeof(ModelManager), MemorySystem::Resource);

}

ModelManager::~ModelManager()
{

}

bool ModelManager::init(WindowConfig config)
{
    Service::init(config);

    textureManager = &ServiceLocator::GetService<TextureManager>("TextureManagerVulkan");
    meshManager = &ServiceLocator::GetService<MeshManager>("MeshManager");
    skeletonManager = &ServiceLocator::GetService<SkeletonManager>("SkeletonManager");
    materialManager = &ServiceLocator::GetService<MaterialManager>("MaterialManagerVulkan");
    animationManager = &ServiceLocator::GetService<AnimationManager>("AnimationManager");
    
    if (!(m_logger && textureManager && meshManager && materialManager)) {
        return false;
    }

    EventManager& eventManager = EventManager::getInstance();
    m_eventSubscriptions.push_back(eventManager.subscribe(EventType::ModelLoadEvent, [&] (Event& event) {
        ModelLoadEvent& e = static_cast<ModelLoadEvent&>(event);

        if (!e.entity.hasComponent<ModelComponent>()) {
			e.entity.addComponent<ModelComponent>();
		}

        ModelComponent& modelComponent = e.entity.getComponent<ModelComponent>();
        auto it = meshPrimitives.find(e.path);
        if(it != meshPrimitives.end()) {
            MaterialDesc materialDesc {};
            
            auto pair = it->second();
            Mesh mesh = pair.second;
            mesh.materialID = materialManager->createMaterial(materialDesc);

            Model model {};
            uint32_t meshID = meshManager->loadMesh(mesh);
            model.meshIDs.push_back(meshID);
            model.path = pair.first;

            // single root node skeleton, each primitive instance owns its own mesh so the name must be unique
            // otherwise addSkeleton returns the existing one by name
            std::string skeletonName = pair.first + "_" + std::to_string(m_ids);
            model.skeletonID = skeletonManager->addSkeleton(skeletonName);
            auto skeleton = skeletonManager->getSkeleton(model.skeletonID);
            if (skeleton) {
                SkeletonManager::SkeletonNode node{};
                node.name = pair.first;
                node.meshIDs.push_back(meshID);
                skeleton->rootIndex = 0;
                skeleton->boneMap[node.name] = 0;
                skeleton->nodes.push_back(node);
            }

            modelComponent.modelID = addModel(model);
        }

        else {
            modelComponent.path = "Loading...";
            uint32_t modelID = loadModel(e.path);

            if (modelID != 0) {
                modelComponent.path = e.path;
                modelComponent.modelID = modelID;
            }
            
            else {
                //TODO: imgui or a widget register an event to popup a warning message
                //ideally editor handle it or supress it for run time version
                std::string message = "Failed to load Model from path: " + e.path;
                m_logger->error(message);

                GuiMessageEvent failEvent(message);
                eventManager.publish(failEvent);

                e.entity.removeComponent<ModelComponent>();
            }
        }
    }));

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
    m_models.erase(id); //TODO: might want queue to remove next frame if something goes wrong
}

std::vector<uint32_t> ModelManager::listIDs() const
{
    std::vector<uint32_t> list;
    for(const auto& [id, model] : m_models) {
        list.emplace_back(id);
    }
    return list;
}

uint32_t ModelManager::loadModel(std::string_view path)
{
    if (m_modelData.find(path.data()) != m_modelData.end()) {
        return m_modelData.at(path.data());;
    }

    // primitives are generated instead of imported, one shared copy per path
    auto primitive = meshPrimitives.find(path.data());
    if(primitive != meshPrimitives.end()) {
        Mesh mesh = primitive->second().second;
        mesh.materialID = materialManager->createMaterial(MaterialDesc{});

        Model model {};
        model.meshIDs.push_back(meshManager->loadMesh(mesh));
        model.path = path.data();
        m_modelData[path.data()] = m_ids;
        return addModel(model);
    }

    if(!_loadModel(path)) {
        return 0;
    }

    return _assignID();
}

std::vector<std::string> ModelManager::listPrimitives()
{
    std::vector<std::string> names;
    for(const auto& [name, generator] : meshPrimitives) {
        names.push_back(name);
    }
    std::sort(names.begin(), names.end());
    return names;
}

std::vector<std::string> ModelManager::listModelPaths()
{
    ReadLock lock = _lockRead();
    std::set<std::string> paths;
    for(const auto& [id, model] : m_models) {
        if(model && !model->path.empty()) {
            paths.insert(model->path);
        }
    }
    return { paths.begin(), paths.end() };
}

uint32_t ModelManager::addModel(Model& model)
{
    m_models[m_ids] = std::make_shared<Model>(std::move(model));
    return _assignID();
}

Model* ModelManager::getModel(uint32_t id) const
{
    if (m_models.find(id) == m_models.end()) {
        return nullptr;
    }
    return m_models.at(id).get();
}

bool ModelManager::_loadModel(std::string_view path) 
{
    std::string loadTimer = std::string("Model loading ") + path.data();
    Timer timer(loadTimer.c_str(), true);

    std::string directory = std::string(path).substr(0, path.find_last_of('/'));
    std::string fileName = std::string(path).substr(path.find_last_of('/') + 1);

    Assimp::Importer import;
    unsigned int flags = aiProcess_Triangulate
        | aiProcess_GenSmoothNormals
        | aiProcess_GlobalScale
        | aiProcess_CalcTangentSpace
        | aiProcess_SplitByBoneCount
        | aiProcess_LimitBoneWeights
        | aiProcess_JoinIdenticalVertices
        | aiProcess_RemoveRedundantMaterials
        | aiProcess_ValidateDataStructure;

    const aiScene* scene = import.ReadFile(path.data(), flags);
    
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::string error = import.GetErrorString();
        m_logger->error("Model Loading failed: ERROR::ASSIMP::{}", error);
        return false;
    }

    if(scene->mNumAnimations > 0) {
        animationManager->loadAnimation(path, scene);
    }
    
    uint32_t skeletonID = skeletonManager->addSkeleton(fileName);
    _processSkeletonNodeHierarchy(skeletonID, scene->mRootNode, -1);
    
    std::vector<uint32_t> meshes = {};
    std::unordered_map<uint32_t, uint32_t> meshCache; //assimp mesh index to engine meshID
    auto skeleton = skeletonManager->getSkeleton(skeletonID);
    _processNode(scene->mRootNode, scene, meshes, meshCache, directory, skeletonID, skeleton->rootIndex);
    
    m_modelData[path.data()] = m_ids;
    m_models[m_ids] = std::make_shared<Model>();
    m_models[m_ids]->path = path.data();
    
    for (const auto& mesh : meshes) {
        m_models[m_ids]->meshIDs.push_back(mesh);
    }

    if(skeletonID != 0) {
        m_models[m_ids]->skeletonID = skeletonID;
    }

    return true;
}

void ModelManager::_processNode(aiNode* node,
		const aiScene* scene,
		std::vector<uint32_t>& meshes,
        std::unordered_map<uint32_t, uint32_t>& meshCache,
		std::string_view directory,
		uint32_t skeletonID,
		uint32_t skeletonNodeIndex
){
    auto skeleton = skeletonManager->getSkeleton(skeletonID);

    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        uint32_t aiMeshIndex = node->mMeshes[i];
        uint32_t meshID = 0;
        
        auto it = meshCache.find(aiMeshIndex);
        if(it != meshCache.end()) {
            meshID = it->second;
        } else {
            aiMesh* mesh = scene->mMeshes[aiMeshIndex];
            meshID = _processMesh(mesh, scene, directory, skeletonID);
            meshCache[aiMeshIndex] = meshID;
            meshes.push_back(meshID);
        }

        if(skeleton) {
            skeleton->nodes[skeletonNodeIndex].meshIDs.push_back(meshID);
        }
    }

    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        uint32_t childIndex = skeleton ? skeleton->nodes[skeletonNodeIndex].childrenNodes[i] : 0;
        _processNode(node->mChildren[i], scene, meshes, meshCache, directory, skeletonID, childIndex);
    }
}

uint32_t ModelManager::_processMesh(aiMesh* mesh, const aiScene* scene, std::string_view directory, uint32_t skeletonID)
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<uint32_t> materialIndices;
    materialIndices.reserve(mesh->mNumFaces);

    // process vertices
    for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
        Vertex vertex;
        _setDefaultBone(vertex);

        // process vertex positions, normals and texture coordinates
        vertex.positions = AssimpGLMHelpers::toGlmVec3(mesh->mVertices[i]);
        vertex.normal = AssimpGLMHelpers::toGlmVec3(mesh->mNormals[i]);

        // does the mesh contain texture coordinates?
        // a vertex can contain up to 8 different texture coordinates.
        // We thus make the assumption that we won't 
        // use models where a vertex can have multiple texture coordinates
        // so we always take the first set (0).
        if (mesh->mTextureCoords[0]) {
            vertex.texCoords.x = mesh->mTextureCoords[0][i].x;
            vertex.texCoords.y = mesh->mTextureCoords[0][i].y;
            vertex.tangent.x = mesh->mTangents[i].x;
            vertex.tangent.y = mesh->mTangents[i].y;
            vertex.tangent.z = mesh->mTangents[i].z;
            vertex.bitangent.x = mesh->mBitangents[i].x;
            vertex.bitangent.y = mesh->mBitangents[i].y;
            vertex.bitangent.z = mesh->mBitangents[i].z;
        } else {
            vertex.texCoords = glm::vec2(0.0f, 0.0f);
        }

        vertices.push_back(vertex);
    }

    // process material
    MaterialDesc materialDesc{};
    materialDesc.uv = glm::vec2(1.0f, 1.0f);
    materialDesc.albedo = glm::vec4(1.0);
    materialDesc.normal = glm::vec4(0.0);
    materialDesc.metallic  = 0.001f;
    materialDesc.roughness = 0.75f;
    materialDesc.ao        = 1.0f;
    materialDesc.emissive  = 0.0f;

    if (mesh->mMaterialIndex >= 0) {
        aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
        
        materialDesc.albedoIDs = _loadMaterial(material, scene, aiTextureType_BASE_COLOR, "albedoMap", directory);
        materialDesc.normalIDs = _loadMaterial(material, scene, aiTextureType_NORMALS, "normalMap", directory);
        materialDesc.metallicIDs = _loadMaterial(material, scene, aiTextureType_METALNESS, "metallicMap", directory);
        materialDesc.roughnessIDs = _loadMaterial(material, scene, aiTextureType_DIFFUSE_ROUGHNESS, "roughnessMap", directory);
        materialDesc.aoIDs = _loadMaterial(material, scene, aiTextureType_LIGHTMAP, "aoMap", directory);
        materialDesc.emissiveIDs = _loadMaterial(material, scene, aiTextureType_EMISSIVE, "emissiveMap", directory);
        if(!materialDesc.emissiveIDs.empty()){
            materialDesc.emissive  = 1.0f;
        }

        // try find other non compatible materials, if still fail...
        // give up and let material manager use fallback materials
        if(materialDesc.metallicIDs.empty()) {  //TODO: need adjustment since this is not the correct pbr fallback
            materialDesc.metallicIDs = _loadMaterial(material, scene, aiTextureType_SPECULAR, "metallicMap", directory);
        }
        if(materialDesc.albedoIDs.empty()){
            materialDesc.albedoIDs = _loadMaterial(material, scene, aiTextureType_DIFFUSE, "albedoMap", directory);
        }
        if(materialDesc.roughnessIDs.empty()) {
            materialDesc.roughnessIDs = _loadMaterial(material, scene, aiTextureType_SHININESS, "roughnessMap", directory);
        }
        if(materialDesc.aoIDs.empty()) {
            materialDesc.aoIDs = _loadMaterial(material, scene, aiTextureType_AMBIENT, "aoMap", directory);
        }
    }

    uint32_t assignedMatID = materialManager->createMaterial(materialDesc);
    
    // process indices
    for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
        materialIndices.push_back(assignedMatID);

        aiFace face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++) {
            indices.push_back(face.mIndices[j]);
        }
    }

    _extractBoneWeightForVertices(skeletonID, vertices, mesh, scene);
    
    Mesh m;
    m.vertices = vertices;
    m.indices = indices;
    m.materialID = assignedMatID;
    m.materialIndices = materialIndices;
    return meshManager->loadMesh(m);
}

std::vector<uint32_t> ModelManager::_loadMaterial(aiMaterial* mat, const aiScene* scene, aiTextureType type, std::string typeName, std::string_view directory)
{
    bool isData = (type == aiTextureType_NORMALS || 
                   type == aiTextureType_METALNESS || 
                   type == aiTextureType_DIFFUSE_ROUGHNESS || 
                   type == aiTextureType_AMBIENT_OCCLUSION || 
                   type == aiTextureType_HEIGHT || 
                   type == aiTextureType_SHININESS);
                   
    std::vector<uint32_t> textureIDs;
    for (unsigned int i = 0; i < mat->GetTextureCount(type); i++) {
        const uint32_t defaultMipLevel = TextureManager::FULL_MIP_CHAIN;

        aiString str;
        mat->GetTexture(type, i, &str);
        std::string texturePathStr = str.C_Str();

        uint32_t textureID = 0;
        size_t asteriskPos = texturePathStr.find('*');

        if (asteriskPos != std::string::npos) {
            int textureIndex = std::stoi(texturePathStr.substr(asteriskPos + 1));
            const aiTexture* embeddedTexture = scene->mTextures[textureIndex];

            if (embeddedTexture->mHeight == 0) {
                // create a unique cache key for compressed memory bytes (PNG/JPG) 
                std::string cacheKey = std::string(directory) + "_" + texturePathStr; // e.g., "assets/DamagedHelmet_/*0"
                
                textureID = textureManager->loadTextureFromMemory(
                    embeddedTexture->pcData,
                    embeddedTexture->mWidth, // Byte size when mHeight == 0
                    cacheKey,
                    defaultMipLevel,
                    isData
                );
            } else {
                throw std::runtime_error("Uncompressed embedded textures (mHeight > 0) not yet supported.");
            }
        } else {
            std::string path = std::string(directory) + '/' + std::string(str.C_Str());
            textureID = textureManager->loadTexture(path.data(), defaultMipLevel, isData);
        }
        
        textureIDs.push_back(textureID);
    }

    return textureIDs;
}

void ModelManager::_processSkeletonNodeHierarchy(uint32_t skeletonID, aiNode* aiNode, int parentIndex)
{
    auto skeleton = skeletonManager->getSkeleton(skeletonID);
    if (!skeleton) {
        return;
    }

    std::string nodeName = aiNode->mName.C_Str();

    aiVector3D aiTranslation;
    aiQuaternion aiRotation;
    aiVector3D aiScale;
    aiNode->mTransformation.Decompose(aiScale, aiRotation, aiTranslation);  //local matrix of each node relative to the parent

    SkeletonManager::SkeletonNode node{};
    node.name = nodeName;
    node.isBone = false;
    node.vqs.position = glm::vec3(aiTranslation.x, aiTranslation.y, aiTranslation.z);
    node.vqs.rotation.fromGlmQuat(glm::quat(aiRotation.w, aiRotation.x, aiRotation.y, aiRotation.z));
    node.vqs.scale = glm::vec3(aiScale.x, aiScale.y, aiScale.z);
    node.parentIndex = parentIndex;

    int currentIndex = skeleton->nodes.size();
    skeleton->boneMap[nodeName] = currentIndex;
    skeleton->nodes.push_back(node);

    if (parentIndex != -1) {
        skeleton->nodes[parentIndex].childrenNodes.push_back(currentIndex);
    } else {
        skeleton->rootIndex = currentIndex;
    }

    for (unsigned int i = 0; i < aiNode->mNumChildren; ++i) {
        _processSkeletonNodeHierarchy(skeletonID, aiNode->mChildren[i], currentIndex);
    }
}

void ModelManager::_setDefaultBone(Vertex &vertex)
{
    for (int i = 0; i < MAX_BONE_INFLUENCE; i++) {
        vertex.m_boneIDs[i] = -1;
        vertex.m_weights[i] = 0.0f;
    }
}

void ModelManager::_extractBoneWeightForVertices(uint32_t skeletonID, std::vector<Vertex>& vertices, aiMesh* mesh, const aiScene* scene)
{
    auto skeleton = skeletonManager->getSkeleton(skeletonID);
    if(!skeleton) {
        return;
    }

    for (unsigned int boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex) {
        std::string boneName = mesh->mBones[boneIndex]->mName.C_Str();
        glm::mat4 offsetMatrix = AssimpGLMHelpers::toGlmMat4(mesh->mBones[boneIndex]->mOffsetMatrix); 

        auto it = skeleton->boneMap.find(boneName);
        if (it != skeleton->boneMap.end()) {
            int nodeBoneIndex = it->second;
            auto& existingNode = skeleton->nodes[nodeBoneIndex];
            existingNode.isBone = true;
            existingNode.offsetMatrix = offsetMatrix;

            auto weights = mesh->mBones[boneIndex]->mWeights;
            int numWeights = mesh->mBones[boneIndex]->mNumWeights;

            for (int i = 0; i < numWeights; ++i) {
                int vertexId = weights[i].mVertexId;
                float weight = weights[i].mWeight;

                for(int j = 0; j < MAX_BONE_INFLUENCE; j++) {
                    Vertex& vertex = vertices[vertexId];
                    if(vertex.m_weights[j] == 0.0f) {
                        vertex.m_boneIDs[j] = nodeBoneIndex;
                        vertex.m_weights[j] = weight;
                        break;
                    }
                }
            }
        }
    }
}
