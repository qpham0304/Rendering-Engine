#include "ModelLoaderGLTF.h"
#include "core/resources/managers/TextureManager.h"
#include "core/resources/managers/MeshManager.h"
#include "core/resources/managers/SkeletonManager.h"
#include "core/resources/managers/MaterialManager.h"
#include "animation/AnimationManager.h"
#include "logging/Logger.h"
#include "core/features/Timer.h"

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <algorithm>
#include <cmath>
#include <map>
#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace {
    constexpr auto kExtensions = fastgltf::Extensions::KHR_texture_transform
        | fastgltf::Extensions::KHR_mesh_quantization
        | fastgltf::Extensions::KHR_materials_emissive_strength
        | fastgltf::Extensions::KHR_materials_pbrSpecularGlossiness;

    constexpr auto kOptions = fastgltf::Options::LoadExternalBuffers
        | fastgltf::Options::DecomposeNodeMatrices
        | fastgltf::Options::GenerateMeshIndices;

    std::optional<size_t> toOptional(const fastgltf::Optional<size_t>& value)
    {
        return value.has_value() ? std::optional<size_t>(value.value()) : std::nullopt;
    }

    fastgltf::TRS restPose(const fastgltf::Node& node)
    {
        if (const auto* trs = std::get_if<fastgltf::TRS>(&node.transform)) {
            return *trs;
        }
        return {};
    }

    glm::vec3 toGlm(const fastgltf::math::fvec3& v)
    {
        return glm::vec3(v.x(), v.y(), v.z());
    }

    glm::quat toGlm(const fastgltf::math::fquat& q)
    {
        return glm::quat(q.w(), q.x(), q.y(), q.z());
    }
}

struct ModelLoaderGLTF::LoadState {
    const fastgltf::Asset& asset;
    std::string_view path;
    uint32_t skeletonID{ 0 };
    std::vector<int> nodeToSkeleton;
    std::vector<std::string> nodeNames;
    std::vector<std::vector<int>> skinJoints;
    std::unordered_map<size_t, uint32_t> materials;
    uint32_t defaultMaterialID{ 0 };
    std::map<std::pair<size_t, std::optional<size_t>>, std::vector<uint32_t>> meshes;
    std::vector<uint32_t> meshIDs;
};

ModelLoaderGLTF::ModelLoaderGLTF(const Context& context)
    : ModelLoader(context)
{
}

bool ModelLoaderGLTF::load(std::string_view path, Model& model)
{
    std::string loadTimer = std::string("Model loading ") + path.data();
    Timer timer(loadTimer.c_str(), true);

    const std::filesystem::path filePath(path);
    auto data = fastgltf::GltfDataBuffer::FromPath(filePath);
    if (data.error() != fastgltf::Error::None) {
        m_context.logger->error("Model Loading failed: {}: {}", path, fastgltf::getErrorMessage(data.error()));
        return false;
    }

    fastgltf::Parser parser(kExtensions);
    auto loaded = parser.loadGltf(data.get(), filePath.parent_path(), kOptions);
    if (loaded.error() != fastgltf::Error::None) {
        m_context.logger->error("Model Loading failed: {}: {}", path, fastgltf::getErrorMessage(loaded.error()));
        return false;
    }

    const fastgltf::Asset& asset = loaded.get();
    LoadState state{ asset, path };

    std::unordered_set<std::string> usedNames;
    state.nodeNames.reserve(asset.nodes.size());
    for (size_t i = 0; i < asset.nodes.size(); i++) {
        std::string name = asset.nodes[i].name.empty() ? "node_" + std::to_string(i) : std::string(asset.nodes[i].name);
        if (usedNames.contains(name)) {
            name += "_" + std::to_string(i);
        }
        usedNames.insert(name);
        state.nodeNames.push_back(std::move(name));
    }

    std::vector<size_t> roots;
    if (!asset.scenes.empty()) {
        const size_t sceneIndex = std::min(asset.defaultScene.has_value() ? asset.defaultScene.value() : 0, asset.scenes.size() - 1);
        roots.assign(asset.scenes[sceneIndex].nodeIndices.begin(), asset.scenes[sceneIndex].nodeIndices.end());
    } else {
        std::vector<bool> isChild(asset.nodes.size(), false);
        for (const auto& node : asset.nodes) {
            for (size_t child : node.children) {
                if (child < isChild.size()) {
                    isChild[child] = true;
                }
            }
        }
        for (size_t i = 0; i < asset.nodes.size(); i++) {
            if (!isChild[i]) {
                roots.push_back(i);
            }
        }
    }

    state.skeletonID = m_context.skeletonManager->addSkeleton(path);
    auto skeleton = m_context.skeletonManager->getSkeleton(state.skeletonID);
    if (!skeleton) {
        m_context.logger->error("Model Loading failed: {}: could not create a skeleton", path);
        return false;
    }

    state.nodeToSkeleton.assign(asset.nodes.size(), -1);
    if (roots.size() == 1) {
        _buildSkeleton(state, roots[0], -1);
    } else {
        SkeletonManager::SkeletonNode root{};
        root.name = "ROOT";
        root.vqs.scale = glm::vec3(1.0f);
        skeleton->rootIndex = 0;
        skeleton->boneMap[root.name] = 0;
        skeleton->nodes.push_back(root);
        for (size_t rootNode : roots) {
            _buildSkeleton(state, rootNode, 0);
        }
    }

    state.skinJoints.resize(asset.skins.size());
    for (size_t skinIndex = 0; skinIndex < asset.skins.size(); skinIndex++) {
        const auto& skin = asset.skins[skinIndex];
        std::vector<glm::mat4> inverseBindMatrices(skin.joints.size(), glm::mat4(1.0f));
        if (skin.inverseBindMatrices.has_value()) {
            const auto& accessor = asset.accessors[skin.inverseBindMatrices.value()];
            fastgltf::iterateAccessorWithIndex<glm::mat4>(asset, accessor, [&](const glm::mat4& matrix, size_t i) {
                if (i < inverseBindMatrices.size()) {
                    inverseBindMatrices[i] = matrix;
                }
            });
        }

        auto& joints = state.skinJoints[skinIndex];
        joints.reserve(skin.joints.size());
        for (size_t j = 0; j < skin.joints.size(); j++) {
            const size_t nodeIndex = skin.joints[j];
            const int skeletonIndex = nodeIndex < state.nodeToSkeleton.size() ? state.nodeToSkeleton[nodeIndex] : -1;
            joints.push_back(skeletonIndex);
            if (skeletonIndex >= 0) {
                skeleton->nodes[skeletonIndex].isBone = true;
                skeleton->nodes[skeletonIndex].offsetMatrix = inverseBindMatrices[j];
            }
        }
    }

    for (size_t nodeIndex = 0; nodeIndex < asset.nodes.size(); nodeIndex++) {
        const auto& node = asset.nodes[nodeIndex];
        const int skeletonIndex = state.nodeToSkeleton[nodeIndex];
        if (skeletonIndex < 0 || !node.meshIndex.has_value()) {
            continue;
        }
        std::vector<uint32_t> meshIDs = _loadMesh(state, node.meshIndex.value(), toOptional(node.skinIndex));
        auto& nodeMeshes = skeleton->nodes[skeletonIndex].meshIDs;
        nodeMeshes.insert(nodeMeshes.end(), meshIDs.begin(), meshIDs.end());
    }

    if (state.meshIDs.empty()) {
        m_context.logger->error("Model Loading failed: {} has no triangle meshes", path);
        return false;
    }

    if (!asset.animations.empty()) {
        _loadAnimations(state);
    }

    model.path = path.data();
    model.meshIDs = std::move(state.meshIDs);
    model.skeletonID = state.skeletonID;
    return true;
}

void ModelLoaderGLTF::_buildSkeleton(LoadState& state, size_t nodeIndex, int parentIndex)
{
    if (nodeIndex >= state.asset.nodes.size() || state.nodeToSkeleton[nodeIndex] >= 0) {
        return;
    }

    auto skeleton = m_context.skeletonManager->getSkeleton(state.skeletonID);
    const auto& gltfNode = state.asset.nodes[nodeIndex];
    const fastgltf::TRS trs = restPose(gltfNode);

    SkeletonManager::SkeletonNode node{};
    node.name = state.nodeNames[nodeIndex];
    node.isBone = false;
    node.vqs.position = toGlm(trs.translation);
    node.vqs.rotation.fromGlmQuat(toGlm(trs.rotation));
    node.vqs.scale = toGlm(trs.scale);
    node.parentIndex = parentIndex;

    const int currentIndex = static_cast<int>(skeleton->nodes.size());
    skeleton->boneMap[node.name] = currentIndex;
    skeleton->nodes.push_back(node);
    state.nodeToSkeleton[nodeIndex] = currentIndex;

    if (parentIndex != -1) {
        skeleton->nodes[parentIndex].childrenNodes.push_back(currentIndex);
    } else {
        skeleton->rootIndex = currentIndex;
    }

    for (size_t child : gltfNode.children) {
        _buildSkeleton(state, child, currentIndex);
    }
}

std::vector<uint32_t> ModelLoaderGLTF::_loadMesh(LoadState& state, size_t meshIndex, std::optional<size_t> skinIndex)
{
    if (meshIndex >= state.asset.meshes.size()) {
        return {};
    }

    const auto key = std::make_pair(meshIndex, skinIndex);
    auto it = state.meshes.find(key);
    if (it != state.meshes.end()) {
        return it->second;
    }

    std::vector<uint32_t> meshIDs;
    for (const auto& primitive : state.asset.meshes[meshIndex].primitives) {
        const uint32_t meshID = _loadPrimitive(state, primitive, skinIndex);
        if (meshID != 0) {
            meshIDs.push_back(meshID);
            state.meshIDs.push_back(meshID);
        }
    }
    state.meshes[key] = meshIDs;
    return meshIDs;
}

uint32_t ModelLoaderGLTF::_loadPrimitive(LoadState& state, const fastgltf::Primitive& primitive, std::optional<size_t> skinIndex)
{
    const fastgltf::Asset& asset = state.asset;

    auto positionAttribute = primitive.findAttribute("POSITION");
    if (primitive.type != fastgltf::PrimitiveType::Triangles || positionAttribute == primitive.attributes.end()) {
        m_context.logger->warn("Skipping a primitive in {}: not a triangle mesh", state.path);
        return 0;
    }

    const auto& positionAccessor = asset.accessors[positionAttribute->accessorIndex];
    if (positionAccessor.count == 0 || !primitive.indicesAccessor.has_value()) {
        return 0;
    }

    std::vector<Vertex> vertices(positionAccessor.count);
    for (Vertex& vertex : vertices) {
        _setDefaultBone(vertex);
    }
    fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, positionAccessor, [&](const glm::vec3& position, size_t i) {
        vertices[i].positions = position;
    });

    const auto& indexAccessor = asset.accessors[primitive.indicesAccessor.value()];
    std::vector<uint32_t> indices(indexAccessor.count);
    fastgltf::copyFromAccessor<std::uint32_t>(asset, indexAccessor, indices.data());
    indices.resize(indices.size() - indices.size() % 3);
    if (indices.empty() || std::any_of(indices.begin(), indices.end(), [&](uint32_t index) { return index >= vertices.size(); })) {
        m_context.logger->warn("Skipping a primitive in {}: invalid indices", state.path);
        return 0;
    }

    auto normalAttribute = primitive.findAttribute("NORMAL");
    if (normalAttribute != primitive.attributes.end()) {
        fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, asset.accessors[normalAttribute->accessorIndex], [&](const glm::vec3& normal, size_t i) {
            if (i < vertices.size()) {
                vertices[i].normal = normal;
            }
        });
    } else {
        _generateNormals(vertices, indices);
    }

    // assimp flipped v for gltf and the texture manager flips images on load, keep the same convention
    auto texCoordAttribute = primitive.findAttribute("TEXCOORD_0");
    const bool hasTexCoords = texCoordAttribute != primitive.attributes.end();
    if (hasTexCoords) {
        fastgltf::iterateAccessorWithIndex<glm::vec2>(asset, asset.accessors[texCoordAttribute->accessorIndex], [&](const glm::vec2& uv, size_t i) {
            if (i < vertices.size()) {
                vertices[i].texCoords = glm::vec2(uv.x, 1.0f - uv.y);
            }
        });
    }

    auto tangentAttribute = primitive.findAttribute("TANGENT");
    if (tangentAttribute != primitive.attributes.end()) {
        fastgltf::iterateAccessorWithIndex<glm::vec4>(asset, asset.accessors[tangentAttribute->accessorIndex], [&](const glm::vec4& tangent, size_t i) {
            if (i < vertices.size()) {
                vertices[i].tangent = glm::vec3(tangent);
                vertices[i].bitangent = glm::cross(vertices[i].normal, glm::vec3(tangent)) * tangent.w;
            }
        });
    } else if (hasTexCoords) {
        _generateTangents(vertices, indices);
    }

    auto jointAttribute = primitive.findAttribute("JOINTS_0");
    auto weightAttribute = primitive.findAttribute("WEIGHTS_0");
    if (skinIndex && *skinIndex < state.skinJoints.size() && jointAttribute != primitive.attributes.end() && weightAttribute != primitive.attributes.end()) {
        const auto& joints = state.skinJoints[*skinIndex];
        std::vector<glm::u16vec4> vertexJoints(vertices.size(), glm::u16vec4(0));
        std::vector<glm::vec4> vertexWeights(vertices.size(), glm::vec4(0.0f));
        fastgltf::iterateAccessorWithIndex<glm::u16vec4>(asset, asset.accessors[jointAttribute->accessorIndex], [&](const glm::u16vec4& joint, size_t i) {
            if (i < vertexJoints.size()) {
                vertexJoints[i] = joint;
            }
        });
        fastgltf::iterateAccessorWithIndex<glm::vec4>(asset, asset.accessors[weightAttribute->accessorIndex], [&](const glm::vec4& weight, size_t i) {
            if (i < vertexWeights.size()) {
                vertexWeights[i] = weight;
            }
        });

        for (size_t i = 0; i < vertices.size(); i++) {
            int slot = 0;
            for (int k = 0; k < MAX_BONE_INFLUENCE; k++) {
                const uint16_t joint = vertexJoints[i][k];
                const float weight = vertexWeights[i][k];
                if (weight <= 0.0f || joint >= joints.size() || joints[joint] < 0) {
                    continue;
                }
                vertices[i].m_boneIDs[slot] = joints[joint];
                vertices[i].m_weights[slot] = weight;
                slot++;
            }
        }
    }

    const uint32_t materialID = _loadMaterial(state, primitive);

    Mesh mesh;
    mesh.vertices = std::move(vertices);
    mesh.indices = std::move(indices);
    mesh.materialID = materialID;
    mesh.materialIndices.assign(mesh.indices.size() / 3, materialID);
    return m_context.meshManager->loadMesh(mesh);
}

uint32_t ModelLoaderGLTF::_loadMaterial(LoadState& state, const fastgltf::Primitive& primitive)
{
    if (!primitive.materialIndex.has_value() || primitive.materialIndex.value() >= state.asset.materials.size()) {
        if (state.defaultMaterialID == 0) {
            MaterialDesc desc{};
            desc.roughness = 0.75f;
            desc.emissive = 0.0f;
            state.defaultMaterialID = m_context.materialManager->createMaterial(desc);
        }
        return state.defaultMaterialID;
    }

    const size_t materialIndex = primitive.materialIndex.value();
    auto it = state.materials.find(materialIndex);
    if (it != state.materials.end()) {
        return it->second;
    }

    const uint32_t materialID = m_context.materialManager->createMaterial(_processMaterial(state, state.asset.materials[materialIndex]));
    state.materials[materialIndex] = materialID;
    return materialID;
}

MaterialDesc ModelLoaderGLTF::_processMaterial(LoadState& state, const fastgltf::Material& material)
{
    MaterialDesc desc{};
    desc.emissive = 0.0f;

    if (const auto* specGloss = material.specularGlossiness.get()) {
        const auto& diffuse = specGloss->diffuseFactor;
        desc.albedo = glm::vec4(diffuse[0], diffuse[1], diffuse[2], diffuse[3]);
        if (specGloss->diffuseTexture.has_value()) {
            desc.albedoID = _loadTexture(state, specGloss->diffuseTexture.value(), false);
        }

        // glossiness lives in the alpha of the specular-glossiness texture
        if (specGloss->specularGlossinessTexture.has_value()) {
            desc.roughnessID = _loadTexture(state, specGloss->specularGlossinessTexture.value(), true);
        }
        if (desc.roughnessID != 0) {
            desc.roughnessChannel = 3;
            desc.roughnessIsSmoothness = 1;
            desc.roughness = 1.0f;
        } else {
            desc.roughness = glm::clamp(1.0f - static_cast<float>(specGloss->glossinessFactor), 0.05f, 1.0f);
        }
        desc.metallic = 0.0f;
    } else {
        const auto& pbr = material.pbrData;
        desc.albedo = glm::vec4(pbr.baseColorFactor[0], pbr.baseColorFactor[1], pbr.baseColorFactor[2], pbr.baseColorFactor[3]);
        if (pbr.baseColorTexture.has_value()) {
            desc.albedoID = _loadTexture(state, pbr.baseColorTexture.value(), false);
        }

        if (pbr.metallicRoughnessTexture.has_value()) {
            const uint32_t textureID = _loadTexture(state, pbr.metallicRoughnessTexture.value(), true);
            desc.metallicID = textureID;
            desc.roughnessID = textureID;
        }
        desc.metallic = pbr.metallicFactor;
        desc.roughness = desc.roughnessID != 0 ? pbr.roughnessFactor : glm::clamp(static_cast<float>(pbr.roughnessFactor), 0.05f, 1.0f);
    }

    if (material.normalTexture.has_value()) {
        desc.normalID = _loadTexture(state, material.normalTexture.value(), true);
    }
    if (material.occlusionTexture.has_value()) {
        desc.aoID = _loadTexture(state, material.occlusionTexture.value(), true);
    }
    if (material.emissiveTexture.has_value()) {
        desc.emissiveID = _loadTexture(state, material.emissiveTexture.value(), false);
    }

    const glm::vec3 emissive(material.emissiveFactor[0], material.emissiveFactor[1], material.emissiveFactor[2]);
    const float intensity = material.emissiveStrength;
    const float peak = std::max({ emissive.r, emissive.g, emissive.b });
    if (desc.emissiveID != 0) {
        desc.emissive = (peak > 0.0f ? peak : 1.0f) * intensity;
    } else if (peak > 0.0f) {
        auto toByte = [peak](float c) { return static_cast<uint8_t>(std::lround(glm::clamp(c / peak, 0.0f, 1.0f) * 255.0f)); };
        desc.emissiveID = m_context.textureManager->createColorTexture({ toByte(emissive.r), toByte(emissive.g), toByte(emissive.b), 255 }, true);
        desc.emissive = peak * intensity;
    }

    if (material.alphaMode == fastgltf::AlphaMode::Mask) {
        desc.alphaCutoff = material.alphaCutoff;
    }

    return desc;
}

uint32_t ModelLoaderGLTF::_loadTexture(LoadState& state, const fastgltf::TextureInfo& info, bool isDataTexture)
{
    const fastgltf::Asset& asset = state.asset;
    if (info.textureIndex >= asset.textures.size()) {
        return 0;
    }

    const auto& texture = asset.textures[info.textureIndex];
    if (!texture.imageIndex.has_value() || texture.imageIndex.value() >= asset.images.size()) {
        m_context.logger->warn("Skipping texture {} in {}: only png and jpg images are supported", info.textureIndex, state.path);
        return 0;
    }

    const size_t imageIndex = texture.imageIndex.value();
    const auto& image = asset.images[imageIndex];
    const std::string cacheKey = std::string(state.path) + "_image" + std::to_string(imageIndex);

    auto fromMemory = [&](const std::byte* bytes, size_t size) -> uint32_t {
        return m_context.textureManager->loadTextureFromMemory(bytes, size, cacheKey, TextureManager::FULL_MIP_CHAIN, isDataTexture);
    };

    return std::visit(fastgltf::visitor {
        [&](const auto&) -> uint32_t {
            m_context.logger->warn("Skipping image {} in {}: unsupported image source", imageIndex, state.path);
            return 0;
        },
        [&](const fastgltf::sources::URI& source) -> uint32_t {
            if (!source.uri.isLocalPath()) {
                m_context.logger->warn("Skipping image {} in {}: not a local file", imageIndex, state.path);
                return 0;
            }
            return m_context.textureManager->loadTexture(_resolveTexturePath(source.uri.path(), state.path), TextureManager::FULL_MIP_CHAIN, isDataTexture);
        },
        [&](const fastgltf::sources::Array& source) -> uint32_t {
            return fromMemory(source.bytes.data(), source.bytes.size_bytes());
        },
        [&](const fastgltf::sources::Vector& source) -> uint32_t {
            return fromMemory(source.bytes.data(), source.bytes.size());
        },
        [&](const fastgltf::sources::BufferView& source) -> uint32_t {
            const auto bytes = fastgltf::DefaultBufferDataAdapter{}(asset, source.bufferViewIndex);
            return fromMemory(bytes.data(), bytes.size_bytes());
        },
    }, image.data);
}

void ModelLoaderGLTF::_loadAnimations(LoadState& state)
{
    const fastgltf::Asset& asset = state.asset;
    AnimationData animationData;

    for (size_t animationIndex = 0; animationIndex < asset.animations.size(); animationIndex++) {
        const auto& gltfAnimation = asset.animations[animationIndex];
        auto animation = std::make_shared<Animation>();
        animation->ticksPerSecond = 1.0f;

        std::map<size_t, Track> tracks;
        for (const auto& channel : gltfAnimation.channels) {
            if (!channel.nodeIndex.has_value() || channel.nodeIndex.value() >= asset.nodes.size()
                || channel.path == fastgltf::AnimationPath::Weights || channel.samplerIndex >= gltfAnimation.samplers.size()) {
                continue;
            }

            const auto& sampler = gltfAnimation.samplers[channel.samplerIndex];
            const auto& inputAccessor = asset.accessors[sampler.inputAccessor];
            const auto& outputAccessor = asset.accessors[sampler.outputAccessor];

            std::vector<float> times(inputAccessor.count);
            fastgltf::copyFromAccessor<float>(asset, inputAccessor, times.data());
            if (times.empty()) {
                continue;
            }
            animation->duration = std::max(animation->duration, times.back());

            // cubic spline keys are stored as in-tangent, value, out-tangent
            const bool cubic = sampler.interpolation == fastgltf::AnimationInterpolation::CubicSpline;
            const size_t stride = cubic ? 3 : 1;
            const size_t offset = cubic ? 1 : 0;

            const size_t nodeIndex = channel.nodeIndex.value();
            Track& track = tracks[nodeIndex];
            track.boneName = state.nodeNames[nodeIndex];

            if (channel.path == fastgltf::AnimationPath::Rotation) {
                std::vector<glm::vec4> values(outputAccessor.count);
                fastgltf::copyFromAccessor<glm::vec4>(asset, outputAccessor, values.data());
                for (size_t k = 0; k < times.size() && k * stride + offset < values.size(); k++) {
                    const glm::vec4& v = values[k * stride + offset];
                    KeyRotation key{};
                    key.timeStamp = times[k];
                    key.orientation.fromGlmQuat(glm::normalize(glm::quat(v.w, v.x, v.y, v.z)));
                    track.keyRotations.push_back(key);
                }
                continue;
            }

            std::vector<glm::vec3> values(outputAccessor.count);
            fastgltf::copyFromAccessor<glm::vec3>(asset, outputAccessor, values.data());
            for (size_t k = 0; k < times.size() && k * stride + offset < values.size(); k++) {
                const glm::vec3& value = values[k * stride + offset];
                if (channel.path == fastgltf::AnimationPath::Translation) {
                    KeyPosition key{};
                    key.timeStamp = times[k];
                    key.position = value;
                    track.keyPositions.push_back(key);
                } else {
                    KeyScale key{};
                    key.timeStamp = times[k];
                    key.scale = value;
                    track.keyScales.push_back(key);
                }
            }
        }

        if (tracks.empty() || animation->duration <= 0.0f) {
            continue;
        }

        // the animator resets a channel without keys to zero, hold the rest pose instead
        for (auto& [nodeIndex, track] : tracks) {
            const fastgltf::TRS trs = restPose(asset.nodes[nodeIndex]);
            if (track.keyPositions.empty()) {
                KeyPosition key{};
                key.position = toGlm(trs.translation);
                track.keyPositions.push_back(key);
            }
            if (track.keyRotations.empty()) {
                KeyRotation key{};
                key.orientation.fromGlmQuat(toGlm(trs.rotation));
                track.keyRotations.push_back(key);
            }
            if (track.keyScales.empty()) {
                KeyScale key{};
                key.scale = toGlm(trs.scale);
                track.keyScales.push_back(key);
            }
            animation->tracks.push_back(std::move(track));
        }

        std::string name = gltfAnimation.name.empty() ? "animation_" + std::to_string(animationIndex) : std::string(gltfAnimation.name);
        animationData.try_emplace(std::move(name), animation);
    }

    m_context.animationManager->addAnimations(state.path, std::move(animationData));
}
