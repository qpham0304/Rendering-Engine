#include "ModelLoaderAssimp.h"
#include "core/resources/managers/TextureManager.h"
#include "core/resources/managers/MeshManager.h"
#include "core/resources/managers/SkeletonManager.h"
#include "core/resources/managers/MaterialManager.h"
#include "animation/AnimationManager.h"
#include "logging/Logger.h"
#include "core/features/Timer.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/GltfMaterial.h>
#include <assimp_glm_helpers.h>
#include <algorithm>
#include <filesystem>
#include <cctype>
#include <cmath>

ModelLoaderAssimp::ModelLoaderAssimp(const Context& context)
    : ModelLoader(context)
{
}

bool ModelLoaderAssimp::load(std::string_view path, Model& model)
{
    std::string loadTimer = std::string("Model loading ") + path.data();
    Timer timer(loadTimer.c_str(), true);

    Assimp::Importer import;
    import.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_POINT | aiPrimitiveType_LINE);
    unsigned int flags = aiProcess_Triangulate
        | aiProcess_SortByPType
        | aiProcess_FindDegenerates
        | aiProcess_FindInvalidData
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
        m_context.logger->error("Model Loading failed: {}: ERROR::ASSIMP::{}", path.data(), error);
        return false;
    }

    if(scene->mNumAnimations > 0) {
        _loadAnimations(path, scene);
    }

    uint32_t skeletonID = m_context.skeletonManager->addSkeleton(path);
    _processSkeletonNodeHierarchy(skeletonID, scene->mRootNode, -1);

    std::vector<uint32_t> meshes = {};
    std::unordered_map<uint32_t, uint32_t> meshCache; //assimp mesh index to engine meshID
    auto skeleton = m_context.skeletonManager->getSkeleton(skeletonID);
    _processNode(scene->mRootNode, scene, meshes, meshCache, path, skeletonID, skeleton ? skeleton->rootIndex : 0);

    if(meshes.empty()) {
        m_context.logger->error("Model Loading failed: {} has no triangle meshes", path.data());
        return false;
    }

    model.path = path.data();
    model.meshIDs = std::move(meshes);
    model.skeletonID = skeletonID;
    return true;
}

void ModelLoaderAssimp::_processNode(aiNode* node,
		const aiScene* scene,
		std::vector<uint32_t>& meshes,
        std::unordered_map<uint32_t, uint32_t>& meshCache,
		std::string_view modelPath,
		uint32_t skeletonID,
		uint32_t skeletonNodeIndex
){
    auto skeleton = m_context.skeletonManager->getSkeleton(skeletonID);

    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        uint32_t aiMeshIndex = node->mMeshes[i];
        uint32_t meshID = 0;
        
        auto it = meshCache.find(aiMeshIndex);
        if(it != meshCache.end()) {
            meshID = it->second;
        } else {
            aiMesh* mesh = scene->mMeshes[aiMeshIndex];
            meshID = _processMesh(mesh, scene, modelPath, skeletonID);
            meshCache[aiMeshIndex] = meshID;
            if(meshID != 0) {
                meshes.push_back(meshID);
            }
        }

        if(meshID != 0 && skeleton) {
            skeleton->nodes[skeletonNodeIndex].meshIDs.push_back(meshID);
        }
    }

    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        uint32_t childIndex = skeleton ? skeleton->nodes[skeletonNodeIndex].childrenNodes[i] : 0;
        _processNode(node->mChildren[i], scene, meshes, meshCache, modelPath, skeletonID, childIndex);
    }
}

uint32_t ModelLoaderAssimp::_processMesh(aiMesh* mesh, const aiScene* scene, std::string_view modelPath, uint32_t skeletonID)
{
    // Triangulate tags meshes that had quads or ngons with NGONEncodingFlag, they are still pure triangles
    const unsigned int primitiveTypes = mesh->mPrimitiveTypes & ~aiPrimitiveType_NGONEncodingFlag;
    if (primitiveTypes != aiPrimitiveType_TRIANGLE || mesh->mNumVertices == 0) {
        m_context.logger->warn("Skipping mesh '{}' in {}: not a triangle mesh", mesh->mName.C_Str(), modelPath);
        return 0;
    }

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<uint32_t> materialIndices;
    materialIndices.reserve(mesh->mNumFaces);

    const bool hasNormals = mesh->HasNormals();
    const bool hasTangents = mesh->HasTangentsAndBitangents();

    // process vertices
    for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
        Vertex vertex;
        _setDefaultBone(vertex);

        // process vertex positions, normals and texture coordinates
        vertex.positions = AssimpGLMHelpers::toGlmVec3(mesh->mVertices[i]);
        if (hasNormals) {
            vertex.normal = AssimpGLMHelpers::toGlmVec3(mesh->mNormals[i]);
        }

        // does the mesh contain texture coordinates?
        // a vertex can contain up to 8 different texture coordinates.
        // We thus make the assumption that we won't 
        // use models where a vertex can have multiple texture coordinates
        // so we always take the first set (0).
        if (mesh->mTextureCoords[0]) {
            vertex.texCoords.x = mesh->mTextureCoords[0][i].x;
            vertex.texCoords.y = mesh->mTextureCoords[0][i].y;
            if (hasTangents) {
                vertex.tangent.x = mesh->mTangents[i].x;
                vertex.tangent.y = mesh->mTangents[i].y;
                vertex.tangent.z = mesh->mTangents[i].z;
                vertex.bitangent.x = mesh->mBitangents[i].x;
                vertex.bitangent.y = mesh->mBitangents[i].y;
                vertex.bitangent.z = mesh->mBitangents[i].z;
            }
        } else {
            vertex.texCoords = glm::vec2(0.0f, 0.0f);
        }

        vertices.push_back(vertex);
    }

    MaterialDesc materialDesc{};
    materialDesc.roughness = 0.75f;
    materialDesc.emissive = 0.0f;
    if (mesh->mMaterialIndex < scene->mNumMaterials) {
        materialDesc = _processMaterial(scene->mMaterials[mesh->mMaterialIndex], scene, modelPath);
    }

    uint32_t assignedMatID = m_context.materialManager->createMaterial(materialDesc);
    
    // process indices
    for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
        materialIndices.push_back(assignedMatID);

        aiFace face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++) {
            indices.push_back(face.mIndices[j]);
        }
    }

    _extractBoneWeightForVertices(skeletonID, vertices, mesh);
    
    Mesh m;
    m.vertices = vertices;
    m.indices = indices;
    m.materialID = assignedMatID;
    m.materialIndices = materialIndices;
    return m_context.meshManager->loadMesh(m);
}

MaterialDesc ModelLoaderAssimp::_processMaterial(aiMaterial* material, const aiScene* scene, std::string_view modelPath)
{
    MaterialDesc desc{};
    desc.emissive = 0.0f;

    desc.albedoID = _loadTexture(material, scene, { aiTextureType_BASE_COLOR, aiTextureType_DIFFUSE }, false, modelPath);
    desc.normalID = _loadTexture(material, scene, { aiTextureType_NORMALS, aiTextureType_NORMAL_CAMERA }, true, modelPath);
    aiString heightName;
    if (desc.normalID == 0 && material->GetTexture(aiTextureType_HEIGHT, 0, &heightName) == AI_SUCCESS && _looksLikeNormalMap(heightName.C_Str())) {
        desc.normalID = _loadTexture(material, scene, { aiTextureType_HEIGHT }, true, modelPath);
    }
    desc.metallicID = _loadTexture(material, scene, { aiTextureType_METALNESS }, true, modelPath);
    desc.roughnessID = _loadTexture(material, scene, { aiTextureType_DIFFUSE_ROUGHNESS }, true, modelPath);
    if (desc.roughnessID == 0) {
        desc.roughnessID = _loadTexture(material, scene, { aiTextureType_SHININESS }, true, modelPath);
        if (desc.roughnessID != 0) {
            desc.roughnessChannel = 0;
            desc.roughnessIsSmoothness = 1;
        }
    }
    desc.aoID = _loadTexture(material, scene, { aiTextureType_LIGHTMAP, aiTextureType_AMBIENT_OCCLUSION, aiTextureType_AMBIENT }, true, modelPath);
    desc.emissiveID = _loadTexture(material, scene, { aiTextureType_EMISSIVE, aiTextureType_EMISSION_COLOR }, false, modelPath);

    aiColor4D baseColor;
    aiColor3D diffuse;
    if (material->Get(AI_MATKEY_BASE_COLOR, baseColor) == AI_SUCCESS) {
        desc.albedo = glm::vec4(baseColor.r, baseColor.g, baseColor.b, baseColor.a);
    } else if (desc.albedoID == 0 && material->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse) == AI_SUCCESS) {
        desc.albedo = glm::vec4(diffuse.r, diffuse.g, diffuse.b, 1.0f);
    }

    float metallic = 0.0f;
    if (material->Get(AI_MATKEY_METALLIC_FACTOR, metallic) == AI_SUCCESS) {
        desc.metallic = metallic;
    } else {
        desc.metallic = desc.metallicID != 0 ? 1.0f : 0.0f;
    }

    float roughness = 1.0f;
    float shininess = 0.0f;
    const bool hasRoughness = material->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) == AI_SUCCESS;
    if (desc.roughnessID != 0) {
        desc.roughness = hasRoughness ? roughness : 1.0f;
    } else if (hasRoughness) {
        desc.roughness = glm::clamp(roughness, 0.05f, 1.0f);
    } else if (material->Get(AI_MATKEY_SHININESS, shininess) == AI_SUCCESS && shininess > 0.0f) {
        desc.roughness = glm::clamp(std::pow(2.0f / (shininess + 2.0f), 0.25f), 0.05f, 1.0f);
    } else {
        desc.roughness = 0.75f;
    }

    aiColor3D emissive(0.0f, 0.0f, 0.0f);
    float intensity = 1.0f;
    material->Get(AI_MATKEY_COLOR_EMISSIVE, emissive);
    material->Get(AI_MATKEY_EMISSIVE_INTENSITY, intensity);
    const float peak = std::max({ emissive.r, emissive.g, emissive.b });
    if (desc.emissiveID != 0) {
        desc.emissive = (peak > 0.0f ? peak : 1.0f) * intensity;
    } else if (peak > 0.0f) {
        auto toByte = [peak](float c) { return static_cast<uint8_t>(std::lround(glm::clamp(c / peak, 0.0f, 1.0f) * 255.0f)); };
        desc.emissiveID = m_context.textureManager->createColorTexture({ toByte(emissive.r), toByte(emissive.g), toByte(emissive.b), 255 }, true);
        desc.emissive = peak * intensity;
    }

    // foliage cards are quads, the leaf shape comes from the albedo alpha
    aiString alphaMode;
    if (material->Get(AI_MATKEY_GLTF_ALPHAMODE, alphaMode) == AI_SUCCESS && std::string_view(alphaMode.C_Str()) == "MASK") {
        float cutoff = 0.5f;
        material->Get(AI_MATKEY_GLTF_ALPHACUTOFF, cutoff);
        desc.alphaCutoff = cutoff;
    }

    return desc;
}

uint32_t ModelLoaderAssimp::_loadTexture(aiMaterial* material, const aiScene* scene, std::initializer_list<aiTextureType> types, bool isDataTexture, std::string_view modelPath)
{
    for (aiTextureType type : types) {
        for (unsigned int i = 0; i < material->GetTextureCount(type); i++) {
            aiString str;
            if (material->GetTexture(type, i, &str) != AI_SUCCESS) {
                continue;
            }
            std::string texturePath = str.C_Str();

            uint32_t textureID = 0;
            const aiTexture* embeddedTexture = scene->GetEmbeddedTexture(str.C_Str());
            if (embeddedTexture) {
                if (embeddedTexture->mHeight != 0) {
                    m_context.logger->warn("Skipping {} in {}: uncompressed embedded textures are not supported yet", texturePath, modelPath);
                    continue;
                }
                std::string cacheKey = std::string(modelPath) + "_" + texturePath;
                textureID = m_context.textureManager->loadTextureFromMemory(embeddedTexture->pcData, embeddedTexture->mWidth, cacheKey, TextureManager::FULL_MIP_CHAIN, isDataTexture);
            } else if (!texturePath.empty() && texturePath[0] == '*') {
                m_context.logger->warn("Skipping {} in {}: no embedded texture at that index", texturePath, modelPath);
                continue;
            } else {
                textureID = m_context.textureManager->loadTexture(_resolveTexturePath(texturePath, modelPath), TextureManager::FULL_MIP_CHAIN, isDataTexture);
            }

            // a checker normal or roughness map breaks the lighting, let the material fall back to its flat default
            if (isDataTexture && textureID == 0) {
                continue;
            }
            return textureID;
        }
    }
    return 0;
}

void ModelLoaderAssimp::_loadAnimations(std::string_view path, const aiScene* scene)
{
    AnimationData animationData;
    for(int i = 0; i < scene->mNumAnimations; i++) {
        aiAnimation* aiAnimation = scene->mAnimations[i];
        
        auto animation = std::make_shared<Animation>();
        animation->ticksPerSecond = aiAnimation->mTicksPerSecond;
        animation->duration = aiAnimation->mDuration;

        //TODO: support mesh morph channel
        // for(int j = 0; j < aiAnimation->mNumMorphMeshChannels; j++) {
        //     aiMeshMorphAnim* aiMeshMorphChannel = aiAnimation->mMorphMeshChannels[j];

        //     for(int k = 0; k < aiMeshMorphChannel->mNumKeys; k++) {
        //         aiMeshMorphKey morpKey = aiMeshMorphChannel->mKeys[k];

        //     }
        // }
        
        for(int j = 0; j < aiAnimation->mNumChannels; j++) {
            aiNodeAnim* aiChannel = aiAnimation->mChannels[j];

            Track track{};
            track.boneName = aiChannel->mNodeName.C_Str();

            for(int k = 0; k < aiChannel->mNumPositionKeys; k++) {
                aiVectorKey aiPositionKey = aiChannel->mPositionKeys[k];
                KeyPosition keyPosition{};
                keyPosition.timeStamp = aiPositionKey.mTime;
                keyPosition.position = AssimpGLMHelpers::toGlmVec3(aiPositionKey.mValue);
                track.keyPositions.push_back(keyPosition);
            }
     
            for(int k = 0; k < aiChannel->mNumRotationKeys; k++) {
                aiQuatKey aiRotationKey = aiChannel->mRotationKeys[k];
                aiQuaternion aiQuat = aiRotationKey.mValue;
                KeyRotation keyRotation{};
                keyRotation.timeStamp = aiRotationKey.mTime;
                keyRotation.orientation.fromGlmQuat(glm::quat(aiQuat.w, aiQuat.x, aiQuat.y, aiQuat.z));
                track.keyRotations.push_back(keyRotation);
            }
     
            for(int k = 0; k < aiChannel->mNumScalingKeys; k++) {
                aiVectorKey aiScalingKey = aiChannel->mScalingKeys[k];
                KeyScale keyScale{};
                keyScale.timeStamp = aiScalingKey.mTime;
                keyScale.scale = AssimpGLMHelpers::toGlmVec3(aiScalingKey.mValue);
                track.keyScales.push_back(keyScale);
            }

            animation->tracks.push_back(track);
        }
        animationData.try_emplace(aiAnimation->mName.C_Str(), animation);
    }

    m_context.animationManager->addAnimations(path, std::move(animationData));
}

void ModelLoaderAssimp::_processSkeletonNodeHierarchy(uint32_t skeletonID, aiNode* aiNode, int parentIndex)
{
    auto skeleton = m_context.skeletonManager->getSkeleton(skeletonID);
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

void ModelLoaderAssimp::_extractBoneWeightForVertices(uint32_t skeletonID, std::vector<Vertex>& vertices, aiMesh* mesh)
{
    auto skeleton = m_context.skeletonManager->getSkeleton(skeletonID);
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
                unsigned int vertexId = weights[i].mVertexId;
                float weight = weights[i].mWeight;
                if (vertexId >= vertices.size()) {
                    continue;
                }

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
