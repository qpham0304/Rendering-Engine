#include "ModelLoader.h"

#include <algorithm>
#include <filesystem>
#include <vector>
#include <cmath>
#include <cctype>

ModelLoader::ModelLoader(const Context& context)
    : m_context(context)
{
    
}

std::string ModelLoader::_resolveTexturePath(std::string_view texturePath, std::string_view modelPath) const
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

void ModelLoader::_setDefaultBone(Vertex& vertex)
{
    for (int i = 0; i < MAX_BONE_INFLUENCE; i++) {
        vertex.m_boneIDs[i] = -1;
        vertex.m_weights[i] = 0.0f;
    }
}

void ModelLoader::_generateNormals(std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
{
    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        Vertex& a = vertices[indices[i]];
        Vertex& b = vertices[indices[i + 1]];
        Vertex& c = vertices[indices[i + 2]];
        const glm::vec3 faceNormal = glm::cross(b.positions - a.positions, c.positions - a.positions);
        a.normal += faceNormal;
        b.normal += faceNormal;
        c.normal += faceNormal;
    }
    for (Vertex& vertex : vertices) {
        const float length = glm::length(vertex.normal);
        vertex.normal = length > 0.0f ? vertex.normal / length : glm::vec3(0.0f, 1.0f, 0.0f);
    }
}

void ModelLoader::_generateTangents(std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
{
    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        Vertex& a = vertices[indices[i]];
        Vertex& b = vertices[indices[i + 1]];
        Vertex& c = vertices[indices[i + 2]];
        const glm::vec3 edge1 = b.positions - a.positions;
        const glm::vec3 edge2 = c.positions - a.positions;
        const glm::vec2 deltaUV1 = b.texCoords - a.texCoords;
        const glm::vec2 deltaUV2 = c.texCoords - a.texCoords;
        const float determinant = deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y;
        if (std::abs(determinant) < 1e-12f) {
            continue;
        }
        const float r = 1.0f / determinant;
        const glm::vec3 tangent = (edge1 * deltaUV2.y - edge2 * deltaUV1.y) * r;
        const glm::vec3 bitangent = (edge2 * deltaUV1.x - edge1 * deltaUV2.x) * r;
        for (Vertex* vertex : { &a, &b, &c }) {
            vertex->tangent += tangent;
            vertex->bitangent += bitangent;
        }
    }
    for (Vertex& vertex : vertices) {
        glm::vec3 tangent = vertex.tangent - vertex.normal * glm::dot(vertex.normal, vertex.tangent);
        if (glm::length(tangent) < 1e-6f) {
            continue;
        }
        tangent = glm::normalize(tangent);
        const float handedness = glm::dot(glm::cross(vertex.normal, tangent), vertex.bitangent) < 0.0f ? -1.0f : 1.0f;
        vertex.tangent = tangent;
        vertex.bitangent = glm::cross(vertex.normal, tangent) * handedness;
    }
}

// obj and fbx exporters often put the normal map in the bump slot, a real height map has no use here
bool ModelLoader::_looksLikeNormalMap(std::string_view name)
{
    std::string lower = std::filesystem::path(name).stem().string();
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return lower.find("norm") != std::string::npos || lower.find("nrm") != std::string::npos || lower.ends_with("_n");
}
