#include "ModelLoaderFBX.h"
#include "core/resources/managers/TextureManager.h"
#include "core/resources/managers/MeshManager.h"
#include "core/resources/managers/SkeletonManager.h"
#include "core/resources/managers/MaterialManager.h"
#include "animation/AnimationManager.h"
#include "logging/Logger.h"
#include "core/features/Timer.h"

#include <ufbx.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace {
    struct SceneDeleter {
        void operator()(ufbx_scene* scene) const { ufbx_free_scene(scene); }
    };

    struct BakedAnimDeleter {
        void operator()(ufbx_baked_anim* anim) const { ufbx_free_baked_anim(anim); }
    };

    glm::vec2 toGlm(const ufbx_vec2& v)
    {
        return glm::vec2(static_cast<float>(v.x), static_cast<float>(v.y));
    }

    glm::vec3 toGlm(const ufbx_vec3& v)
    {
        return glm::vec3(static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z));
    }

    glm::quat toGlm(const ufbx_quat& q)
    {
        return glm::quat(static_cast<float>(q.w), static_cast<float>(q.x), static_cast<float>(q.y), static_cast<float>(q.z));
    }

    glm::mat4 toGlm(const ufbx_matrix& m)
    {
        glm::mat4 result(1.0f);
        for (int column = 0; column < 4; column++) {
            result[column] = glm::vec4(toGlm(m.cols[column]), column == 3 ? 1.0f : 0.0f);
        }
        return result;
    }

    std::string toString(const ufbx_string& s)
    {
        return std::string(s.data, s.length);
    }

    const ufbx_texture* mapTexture(const ufbx_material_map& map)
    {
        return map.texture;
    }

    float mapReal(const ufbx_material_map& map, float fallback)
    {
        return map.has_value ? static_cast<float>(map.value_real) : fallback;
    }
}

struct ModelLoaderFBX::LoadState {
    const ufbx_scene* scene;
    std::string_view path;
    uint32_t skeletonID{ 0 };
    std::vector<int> nodeToSkeleton;
    std::vector<std::string> nodeNames;
    std::unordered_map<uint32_t, uint32_t> materials;
    uint32_t defaultMaterialID{ 0 };
    std::unordered_map<uint32_t, std::vector<uint32_t>> meshes;
    std::vector<uint32_t> meshIDs;
};

ModelLoaderFBX::ModelLoaderFBX(const Context& context)
    : ModelLoader(context)
{
}

bool ModelLoaderFBX::load(std::string_view path, Model& model)
{
    std::string loadTimer = std::string("Model loading ") + path.data();
    Timer timer(loadTimer.c_str(), true);

    ufbx_load_opts options{};
    options.target_axes = ufbx_axes_right_handed_y_up;
    options.target_unit_meters = 1.0f;
    options.space_conversion = UFBX_SPACE_CONVERSION_TRANSFORM_ROOT;
    options.geometry_transform_handling = UFBX_GEOMETRY_TRANSFORM_HANDLING_HELPER_NODES;
    options.inherit_mode_handling = UFBX_INHERIT_MODE_HANDLING_HELPER_NODES;
    options.generate_missing_normals = true;
    options.clean_skin_weights = true;

    ufbx_error error{};
    std::unique_ptr<ufbx_scene, SceneDeleter> scene(ufbx_load_file(std::string(path).c_str(), &options, &error));
    if (!scene) {
        char message[512];
        ufbx_format_error(message, sizeof(message), &error);
        m_context.logger->error("Model Loading failed: {}: {}", path, message);
        return false;
    }

    LoadState state{ scene.get(), path };

    std::unordered_set<std::string> usedNames;
    state.nodeNames.reserve(scene->nodes.count);
    for (size_t i = 0; i < scene->nodes.count; i++) {
        const ufbx_node* node = scene->nodes.data[i];
        std::string name = node->name.length == 0 ? "node_" + std::to_string(i) : toString(node->name);
        if (usedNames.contains(name)) {
            name += "_" + std::to_string(i);
        }
        usedNames.insert(name);
        state.nodeNames.push_back(std::move(name));
    }

    state.skeletonID = m_context.skeletonManager->addSkeleton(path);
    auto skeleton = m_context.skeletonManager->getSkeleton(state.skeletonID);
    if (!skeleton) {
        m_context.logger->error("Model Loading failed: {}: could not create a skeleton", path);
        return false;
    }
    _buildSkeleton(state);

    for (size_t i = 0; i < scene->nodes.count; i++) {
        const ufbx_node* node = scene->nodes.data[i];
        const int skeletonIndex = state.nodeToSkeleton[node->typed_id];
        if (!node->mesh || skeletonIndex < 0) {
            continue;
        }
        std::vector<uint32_t> meshIDs = _loadMesh(state, node->mesh);
        auto& nodeMeshes = skeleton->nodes[skeletonIndex].meshIDs;
        nodeMeshes.insert(nodeMeshes.end(), meshIDs.begin(), meshIDs.end());
    }

    if (state.meshIDs.empty()) {
        m_context.logger->error("Model Loading failed: {} has no triangle meshes", path);
        return false;
    }

    if (scene->anim_stacks.count > 0) {
        _loadAnimations(state);
    }

    model.path = path.data();
    model.meshIDs = std::move(state.meshIDs);
    model.skeletonID = state.skeletonID;
    return true;
}

void ModelLoaderFBX::_buildSkeleton(LoadState& state)
{
    auto skeleton = m_context.skeletonManager->getSkeleton(state.skeletonID);
    state.nodeToSkeleton.assign(state.scene->nodes.count, -1);

    std::vector<std::pair<const ufbx_node*, int>> stack{ { state.scene->root_node, -1 } };
    while (!stack.empty()) {
        auto [ufbxNode, parentIndex] = stack.back();
        stack.pop_back();

        SkeletonManager::SkeletonNode node{};
        node.name = state.nodeNames[ufbxNode->typed_id];
        node.isBone = false;
        node.vqs.position = toGlm(ufbxNode->local_transform.translation);
        node.vqs.rotation.fromGlmQuat(toGlm(ufbxNode->local_transform.rotation));
        node.vqs.scale = toGlm(ufbxNode->local_transform.scale);
        node.parentIndex = parentIndex;

        const int currentIndex = static_cast<int>(skeleton->nodes.size());
        skeleton->boneMap[node.name] = currentIndex;
        skeleton->nodes.push_back(node);
        state.nodeToSkeleton[ufbxNode->typed_id] = currentIndex;

        if (parentIndex != -1) {
            skeleton->nodes[parentIndex].childrenNodes.push_back(currentIndex);
        } else {
            skeleton->rootIndex = currentIndex;
        }

        for (size_t i = ufbxNode->children.count; i > 0; i--) {
            stack.push_back({ ufbxNode->children.data[i - 1], currentIndex });
        }
    }
}

std::vector<uint32_t> ModelLoaderFBX::_loadMesh(LoadState& state, const ufbx_mesh* mesh)
{
    auto cached = state.meshes.find(mesh->typed_id);
    if (cached != state.meshes.end()) {
        return cached->second;
    }

    auto skeleton = m_context.skeletonManager->getSkeleton(state.skeletonID);
    const ufbx_skin_deformer* skin = mesh->skin_deformers.count > 0 ? mesh->skin_deformers.data[0] : nullptr;
    std::vector<int> clusterBones;
    if (skin) {
        clusterBones.reserve(skin->clusters.count);
        for (size_t i = 0; i < skin->clusters.count; i++) {
            const ufbx_skin_cluster* cluster = skin->clusters.data[i];
            const int boneIndex = cluster->bone_node ? state.nodeToSkeleton[cluster->bone_node->typed_id] : -1;
            clusterBones.push_back(boneIndex);
            if (boneIndex >= 0) {
                skeleton->nodes[boneIndex].isBone = true;
                skeleton->nodes[boneIndex].offsetMatrix = toGlm(cluster->geometry_to_bone);
            }
        }
    }

    const bool hasUVs = mesh->vertex_uv.exists;
    const bool hasTangents = mesh->vertex_tangent.exists && mesh->vertex_bitangent.exists;
    std::vector<uint32_t> triangleIndices(mesh->max_face_triangles * 3);
    std::vector<uint32_t> meshIDs;

    for (size_t partIndex = 0; partIndex < mesh->material_parts.count; partIndex++) {
        const ufbx_mesh_part& part = mesh->material_parts.data[partIndex];
        if (part.num_triangles == 0) {
            continue;
        }

        std::vector<Vertex> vertices;
        vertices.reserve(part.num_triangles * 3);
        for (size_t f = 0; f < part.face_indices.count; f++) {
            const ufbx_face face = mesh->faces.data[part.face_indices.data[f]];
            const uint32_t triangleCount = ufbx_triangulate_face(triangleIndices.data(), triangleIndices.size(), mesh, face);
            for (uint32_t corner = 0; corner < triangleCount * 3; corner++) {
                const uint32_t index = triangleIndices[corner];
                Vertex vertex;
                _setDefaultBone(vertex);
                vertex.positions = toGlm(ufbx_get_vertex_vec3(&mesh->vertex_position, index));
                if (mesh->vertex_normal.exists) {
                    vertex.normal = toGlm(ufbx_get_vertex_vec3(&mesh->vertex_normal, index));
                }
                if (hasUVs) {
                    vertex.texCoords = toGlm(ufbx_get_vertex_vec2(&mesh->vertex_uv, index));
                }
                if (hasTangents) {
                    vertex.tangent = toGlm(ufbx_get_vertex_vec3(&mesh->vertex_tangent, index));
                    vertex.bitangent = toGlm(ufbx_get_vertex_vec3(&mesh->vertex_bitangent, index));
                }

                if (skin) {
                    const ufbx_skin_vertex& skinVertex = skin->vertices.data[mesh->vertex_indices.data[index]];
                    std::array<std::pair<float, int>, MAX_BONE_INFLUENCE> influences{};
                    int count = 0;
                    for (uint32_t w = 0; w < skinVertex.num_weights; w++) {
                        const ufbx_skin_weight& skinWeight = skin->weights.data[skinVertex.weight_begin + w];
                        const int bone = clusterBones[skinWeight.cluster_index];
                        const float weight = static_cast<float>(skinWeight.weight);
                        if (bone < 0 || weight <= 0.0f) {
                            continue;
                        }
                        if (count < MAX_BONE_INFLUENCE) {
                            influences[count++] = { weight, bone };
                        } else {
                            auto smallest = std::min_element(influences.begin(), influences.end());
                            if (weight > smallest->first) {
                                *smallest = { weight, bone };
                            }
                        }
                    }
                    float total = 0.0f;
                    for (int k = 0; k < count; k++) {
                        total += influences[k].first;
                    }
                    for (int k = 0; k < count; k++) {
                        vertex.m_boneIDs[k] = influences[k].second;
                        vertex.m_weights[k] = influences[k].first / total;
                    }
                }

                vertices.push_back(vertex);
            }
        }

        std::vector<uint32_t> indices(vertices.size());
        ufbx_vertex_stream stream{ vertices.data(), vertices.size(), sizeof(Vertex) };
        ufbx_error error{};
        const size_t uniqueCount = ufbx_generate_indices(&stream, 1, indices.data(), indices.size(), nullptr, &error);
        if (error.type != UFBX_ERROR_NONE) {
            for (uint32_t i = 0; i < indices.size(); i++) {
                indices[i] = i;
            }
        } else {
            vertices.resize(uniqueCount);
        }

        if (hasUVs && !hasTangents) {
            _generateTangents(vertices, indices);
        }

        const ufbx_material* material = part.index < mesh->materials.count ? mesh->materials.data[part.index] : nullptr;
        const uint32_t materialID = _loadMaterial(state, material);

        Mesh engineMesh;
        engineMesh.vertices = std::move(vertices);
        engineMesh.indices = std::move(indices);
        engineMesh.materialID = materialID;
        engineMesh.materialIndices.assign(engineMesh.indices.size() / 3, materialID);
        const uint32_t meshID = m_context.meshManager->loadMesh(engineMesh);
        if (meshID != 0) {
            meshIDs.push_back(meshID);
            state.meshIDs.push_back(meshID);
        }
    }

    state.meshes[mesh->typed_id] = meshIDs;
    return meshIDs;
}

uint32_t ModelLoaderFBX::_loadMaterial(LoadState& state, const ufbx_material* material)
{
    if (!material) {
        if (state.defaultMaterialID == 0) {
            MaterialDesc desc{};
            desc.roughness = 0.75f;
            desc.emissive = 0.0f;
            state.defaultMaterialID = m_context.materialManager->createMaterial(desc);
        }
        return state.defaultMaterialID;
    }

    auto it = state.materials.find(material->typed_id);
    if (it != state.materials.end()) {
        return it->second;
    }

    const uint32_t materialID = m_context.materialManager->createMaterial(_processMaterial(state, material));
    state.materials[material->typed_id] = materialID;
    return materialID;
}

MaterialDesc ModelLoaderFBX::_processMaterial(LoadState& state, const ufbx_material* material)
{
    const auto& pbr = material->pbr;
    const auto& fbx = material->fbx;

    MaterialDesc desc{};
    desc.emissive = 0.0f;

    const ufbx_texture* albedo = mapTexture(pbr.base_color) ? mapTexture(pbr.base_color) : mapTexture(fbx.diffuse_color);
    desc.albedoID = _loadTexture(state, albedo, false);
    if (desc.albedoID == 0 && pbr.base_color.has_value) {
        const glm::vec4 color(toGlm(pbr.base_color.value_vec3), 1.0f);
        desc.albedo = color * mapReal(pbr.base_factor, 1.0f);
        desc.albedo.a = 1.0f;
    }

    const ufbx_texture* normal = mapTexture(pbr.normal_map) ? mapTexture(pbr.normal_map) : mapTexture(fbx.normal_map);
    if (!normal && mapTexture(fbx.bump) && _looksLikeNormalMap(toString(fbx.bump.texture->relative_filename))) {
        normal = mapTexture(fbx.bump);
    }
    desc.normalID = _loadTexture(state, normal, true);

    desc.metallicID = _loadTexture(state, mapTexture(pbr.metalness), true);
    desc.metallic = mapReal(pbr.metalness, desc.metallicID != 0 ? 1.0f : 0.0f);

    desc.roughnessID = _loadTexture(state, mapTexture(pbr.roughness), true);
    if (desc.roughnessID != 0 && material->features.roughness_as_glossiness.enabled) {
        desc.roughnessChannel = 0;
        desc.roughnessIsSmoothness = 1;
    }
    if (desc.roughnessID == 0) {
        const ufbx_texture* gloss = mapTexture(pbr.glossiness) ? mapTexture(pbr.glossiness) : mapTexture(fbx.specular_exponent);
        desc.roughnessID = _loadTexture(state, gloss, true);
        if (desc.roughnessID != 0) {
            desc.roughnessChannel = 0;
            desc.roughnessIsSmoothness = 1;
        }
    }

    const float shininess = mapReal(fbx.specular_exponent, 0.0f);
    if (desc.roughnessID != 0) {
        desc.roughness = 1.0f;
    } else if (material->features.pbr.enabled && pbr.roughness.has_value) {
        desc.roughness = glm::clamp(static_cast<float>(pbr.roughness.value_real), 0.05f, 1.0f);
    } else if (shininess > 0.0f) {
        desc.roughness = glm::clamp(std::pow(2.0f / (shininess + 2.0f), 0.25f), 0.05f, 1.0f);
    } else {
        desc.roughness = 0.75f;
    }

    const ufbx_texture* ao = mapTexture(pbr.ambient_occlusion) ? mapTexture(pbr.ambient_occlusion) : mapTexture(fbx.ambient_color);
    desc.aoID = _loadTexture(state, ao, true);

    const ufbx_texture* emissiveTexture = mapTexture(pbr.emission_color) ? mapTexture(pbr.emission_color) : mapTexture(fbx.emission_color);
    desc.emissiveID = _loadTexture(state, emissiveTexture, false);

    const glm::vec3 emissive = toGlm(pbr.emission_color.value_vec3);
    const float intensity = mapReal(pbr.emission_factor, 1.0f);
    const float peak = std::max({ emissive.r, emissive.g, emissive.b });
    if (desc.emissiveID != 0) {
        desc.emissive = (peak > 0.0f ? peak : 1.0f) * intensity;
    } else if (peak > 0.0f && intensity > 0.0f) {
        auto toByte = [peak](float c) { return static_cast<uint8_t>(std::lround(glm::clamp(c / peak, 0.0f, 1.0f) * 255.0f)); };
        desc.emissiveID = m_context.textureManager->createColorTexture({ toByte(emissive.r), toByte(emissive.g), toByte(emissive.b), 255 }, true);
        desc.emissive = peak * intensity;
    }

    return desc;
}

uint32_t ModelLoaderFBX::_loadTexture(LoadState& state, const ufbx_texture* texture, bool isDataTexture)
{
    if (!texture) {
        return 0;
    }
    if (texture->type != UFBX_TEXTURE_FILE && texture->file_textures.count > 0) {
        texture = texture->file_textures.data[0];
    }

    const std::string relative = toString(texture->relative_filename);
    const ufbx_blob* content = texture->content.size > 0 ? &texture->content
        : (texture->video && texture->video->content.size > 0 ? &texture->video->content : nullptr);

    uint32_t textureID = 0;
    if (content) {
        const std::string cacheKey = std::string(state.path) + "_" + (relative.empty() ? std::to_string(texture->typed_id) : relative);
        textureID = m_context.textureManager->loadTextureFromMemory(content->data, content->size, cacheKey, TextureManager::FULL_MIP_CHAIN, isDataTexture);
    } else {
        std::error_code error;
        std::string file;
        for (const ufbx_string& candidate : { texture->filename, texture->absolute_filename }) {
            if (candidate.length > 0 && std::filesystem::is_regular_file(std::filesystem::u8path(toString(candidate)), error)) {
                file = toString(candidate);
                break;
            }
        }
        if (file.empty()) {
            const std::string reference = relative.empty() ? toString(texture->absolute_filename) : relative;
            if (reference.empty()) {
                return 0;
            }
            file = _resolveTexturePath(reference, state.path);
        }
        textureID = m_context.textureManager->loadTexture(file, TextureManager::FULL_MIP_CHAIN, isDataTexture);
    }

    // a checker normal or roughness map breaks the lighting, let the material fall back to its flat default
    if (isDataTexture && textureID == 0) {
        return 0;
    }
    return textureID;
}

void ModelLoaderFBX::_loadAnimations(LoadState& state)
{
    const ufbx_scene* scene = state.scene;
    AnimationData animationData;

    for (size_t stackIndex = 0; stackIndex < scene->anim_stacks.count; stackIndex++) {
        const ufbx_anim_stack* stack = scene->anim_stacks.data[stackIndex];
        ufbx_bake_opts bakeOptions{};
        ufbx_error error{};
        std::unique_ptr<ufbx_baked_anim, BakedAnimDeleter> baked(ufbx_bake_anim(scene, stack->anim, &bakeOptions, &error));
        if (!baked) {
            char message[512];
            ufbx_format_error(message, sizeof(message), &error);
            m_context.logger->warn("Skipping animation {} in {}: {}", toString(stack->name), state.path, message);
            continue;
        }

        const double start = baked->key_time_min;
        auto animation = std::make_shared<Animation>();
        animation->ticksPerSecond = 1.0f;
        animation->duration = static_cast<float>(baked->key_time_max - start);
        if (animation->duration <= 0.0f) {
            continue;
        }

        for (size_t i = 0; i < baked->nodes.count; i++) {
            const ufbx_baked_node& bakedNode = baked->nodes.data[i];
            const ufbx_node* node = scene->nodes.data[bakedNode.typed_id];
            if (node == scene->root_node) {
                continue;
            }

            Track track{};
            track.boneName = state.nodeNames[bakedNode.typed_id];
            for (size_t k = 0; k < bakedNode.translation_keys.count; k++) {
                KeyPosition key{};
                key.timeStamp = static_cast<float>(bakedNode.translation_keys.data[k].time - start);
                key.position = toGlm(bakedNode.translation_keys.data[k].value);
                track.keyPositions.push_back(key);
            }
            for (size_t k = 0; k < bakedNode.rotation_keys.count; k++) {
                KeyRotation key{};
                key.timeStamp = static_cast<float>(bakedNode.rotation_keys.data[k].time - start);
                key.orientation.fromGlmQuat(toGlm(bakedNode.rotation_keys.data[k].value));
                track.keyRotations.push_back(key);
            }
            for (size_t k = 0; k < bakedNode.scale_keys.count; k++) {
                KeyScale key{};
                key.timeStamp = static_cast<float>(bakedNode.scale_keys.data[k].time - start);
                key.scale = toGlm(bakedNode.scale_keys.data[k].value);
                track.keyScales.push_back(key);
            }

            // the animator resets a channel without keys to zero, hold the rest pose instead
            if (track.keyPositions.empty()) {
                KeyPosition key{};
                key.position = toGlm(node->local_transform.translation);
                track.keyPositions.push_back(key);
            }
            if (track.keyRotations.empty()) {
                KeyRotation key{};
                key.orientation.fromGlmQuat(toGlm(node->local_transform.rotation));
                track.keyRotations.push_back(key);
            }
            if (track.keyScales.empty()) {
                KeyScale key{};
                key.scale = toGlm(node->local_transform.scale);
                track.keyScales.push_back(key);
            }
            animation->tracks.push_back(std::move(track));
        }

        std::string name = stack->name.length == 0 ? "animation_" + std::to_string(stackIndex) : toString(stack->name);
        animationData.try_emplace(std::move(name), animation);
    }

    m_context.animationManager->addAnimations(state.path, std::move(animationData));
}
