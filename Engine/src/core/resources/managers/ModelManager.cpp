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
#include <assimp/GltfMaterial.h>
#include <assimp_glm_helpers.h>
#include <functional>
#include <algorithm>
#include <set>
#include <filesystem>
#include <cctype>
#include <cmath>

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
        m_logger->warn("Model not found id: {}", id);
        return;
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

    std::string key(path);
    if(m_failedPaths.contains(key)) {
        return 0;
    }

    bool loaded = false;
    try {
        loaded = _loadModel(key);
    }
    catch (const std::exception& e) {
        m_logger->error("Model Loading failed: {}: {}", key, e.what());
    }

    if(!loaded) {
        m_failedPaths.insert(key);
        return 0;
    }

    return _assignID();
}

void ModelManager::clearFailedLoads()
{
    m_failedPaths.clear();
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

    Assimp::Importer import;
    import.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false);
    import.SetPropertyBool(AI_CONFIG_IMPORT_FBX_IGNORE_UP_DIRECTION, true);
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
        m_logger->error("Model Loading failed: {}: ERROR::ASSIMP::{}", path.data(), error);
        return false;
    }

    _correctFbxAxes(scene);

    if(scene->mNumAnimations > 0) {
        animationManager->loadAnimation(path, scene);
    }
    
    uint32_t skeletonID = skeletonManager->addSkeleton(path);
    _processSkeletonNodeHierarchy(skeletonID, scene->mRootNode, -1);
    
    std::vector<uint32_t> meshes = {};
    std::unordered_map<uint32_t, uint32_t> meshCache; //assimp mesh index to engine meshID
    auto skeleton = skeletonManager->getSkeleton(skeletonID);
    _processNode(scene->mRootNode, scene, meshes, meshCache, path, skeletonID, skeleton ? skeleton->rootIndex : 0);

    if(meshes.empty()) {
        m_logger->error("Model Loading failed: {} has no triangle meshes", path.data());
        return false;
    }

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
		std::string_view modelPath,
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

uint32_t ModelManager::_processMesh(aiMesh* mesh, const aiScene* scene, std::string_view modelPath, uint32_t skeletonID)
{
    // Triangulate tags meshes that had quads or ngons with NGONEncodingFlag, they are still pure triangles
    const unsigned int primitiveTypes = mesh->mPrimitiveTypes & ~aiPrimitiveType_NGONEncodingFlag;
    if (primitiveTypes != aiPrimitiveType_TRIANGLE || mesh->mNumVertices == 0) {
        m_logger->warn("Skipping mesh '{}' in {}: not a triangle mesh", mesh->mName.C_Str(), modelPath);
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

namespace {
    std::string toLower(std::string_view text)
    {
        std::string lower(text);
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return lower;
    }

    bool isFbxPath(std::string_view path)
    {
        return toLower(std::filesystem::path(path).extension().string()) == ".fbx";
    }

    // obj and fbx exporters often put the normal map in the bump slot, a real height map has no use here
    bool looksLikeNormalMap(std::string_view name)
    {
        std::string lower = toLower(std::filesystem::path(name).stem().string());
        return lower.find("norm") != std::string::npos || lower.find("nrm") != std::string::npos || lower.ends_with("_n");
    }
}

MaterialDesc ModelManager::_processMaterial(aiMaterial* material, const aiScene* scene, std::string_view modelPath)
{
    MaterialDesc desc{};
    desc.emissive = 0.0f;

    desc.albedoID = _loadTexture(material, scene, { aiTextureType_BASE_COLOR, aiTextureType_DIFFUSE }, false, modelPath);
    desc.normalID = _loadTexture(material, scene, { aiTextureType_NORMALS, aiTextureType_NORMAL_CAMERA }, true, modelPath);
    aiString heightName;
    if (desc.normalID == 0 && material->GetTexture(aiTextureType_HEIGHT, 0, &heightName) == AI_SUCCESS && looksLikeNormalMap(heightName.C_Str())) {
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

    // assimp derives the fbx roughness factor from the phong exponent, it must not scale an authored roughness map
    float roughness = 1.0f;
    float shininess = 0.0f;
    const bool hasRoughness = material->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) == AI_SUCCESS;
    if (desc.roughnessID != 0) {
        desc.roughness = (hasRoughness && !isFbxPath(modelPath)) ? roughness : 1.0f;
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
        desc.emissiveID = textureManager->createColorTexture({ toByte(emissive.r), toByte(emissive.g), toByte(emissive.b), 255 }, true);
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

uint32_t ModelManager::_loadTexture(aiMaterial* material, const aiScene* scene, std::initializer_list<aiTextureType> types, bool isDataTexture, std::string_view modelPath)
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
                    m_logger->warn("Skipping {} in {}: uncompressed embedded textures are not supported yet", texturePath, modelPath);
                    continue;
                }
                std::string cacheKey = std::string(modelPath) + "_" + texturePath;
                textureID = textureManager->loadTextureFromMemory(embeddedTexture->pcData, embeddedTexture->mWidth, cacheKey, TextureManager::FULL_MIP_CHAIN, isDataTexture);
            } else if (!texturePath.empty() && texturePath[0] == '*') {
                m_logger->warn("Skipping {} in {}: no embedded texture at that index", texturePath, modelPath);
                continue;
            } else {
                textureID = textureManager->loadTexture(_resolveTexturePath(texturePath, modelPath), TextureManager::FULL_MIP_CHAIN, isDataTexture);
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

std::string ModelManager::_resolveTexturePath(std::string_view texturePath, std::string_view modelPath) const
{
    namespace fs = std::filesystem;

    std::string normalized(texturePath);
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    if (normalized.starts_with("file://")) {
        normalized.erase(0, 7);
    }

    const fs::path directory = fs::path(modelPath).parent_path();
    const fs::path relative(normalized);
    const fs::path filename = relative.filename();

    std::vector<fs::path> candidates;
    if (relative.is_absolute()) {
        candidates.push_back(relative);
    }
    candidates.push_back(directory / relative);
    for (const fs::path& folder : { directory, directory / "textures", directory / "Textures", directory.parent_path() / "textures", directory.parent_path() / "Textures" }) {
        candidates.push_back(folder / filename);
    }

    // exporters reference the source .psd/.tga while the package ships a png or jpg
    const size_t namedCandidates = candidates.size();
    for (size_t i = 0; i < namedCandidates; i++) {
        for (const char* extension : { ".png", ".jpg", ".jpeg", ".tga" }) {
            candidates.push_back(fs::path(candidates[i]).replace_extension(extension));
        }
    }

    std::error_code error;
    for (const fs::path& candidate : candidates) {
        if (fs::is_regular_file(candidate, error)) {
            return candidate.lexically_normal().generic_string();
        }
    }
    return (directory / relative).lexically_normal().generic_string();
}

// assimp's own fbx axis fix also multiplies the root by UnitScaleFactor on top of GlobalScale, so meter files came in 100x
void ModelManager::_correctFbxAxes(const aiScene* scene) const
{
    if (!scene->mMetaData || !scene->mRootNode) {
        return;
    }

    int32_t upAxis = 1, upSign = 1, frontAxis = 2, frontSign = 1, coordAxis = 0, coordSign = 1;
    if (!scene->mMetaData->Get("UpAxis", upAxis) || !scene->mMetaData->Get("CoordAxis", coordAxis)) {
        return;
    }
    scene->mMetaData->Get("UpAxisSign", upSign);
    scene->mMetaData->Get("FrontAxis", frontAxis);
    scene->mMetaData->Get("FrontAxisSign", frontSign);
    scene->mMetaData->Get("CoordAxisSign", coordSign);
    if (upAxis < 0 || upAxis > 2 || frontAxis < 0 || frontAxis > 2 || coordAxis < 0 || coordAxis > 2) {
        return;
    }

    aiVector3D up, front, right;
    up[upAxis] = static_cast<ai_real>(upSign);
    front[frontAxis] = static_cast<ai_real>(frontSign);
    right[coordAxis] = static_cast<ai_real>(coordSign);

    aiMatrix4x4 axes(right.x, right.y, right.z, 0.0f,
                     up.x, up.y, up.z, 0.0f,
                     front.x, front.y, front.z, 0.0f,
                     0.0f, 0.0f, 0.0f, 1.0f);
    if (axes.IsIdentity()) {
        return;
    }

    // the skeleton stores nodes as translation, rotation and scale, a mirroring basis can't be represented
    if (axes.Determinant() < 0.0f) {
        m_logger->warn("Skipping the fbx axis correction: the file's axes are mirrored");
        return;
    }
    scene->mRootNode->mTransformation *= axes;
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
