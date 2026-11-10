#pragma once

#include <cstdint>
#include <glm/glm.hpp>

struct MaterialDesc {
    uint32_t albedoID = 0;
    uint32_t normalID = 0;
    uint32_t metallicID = 0;
    uint32_t roughnessID = 0;
    uint32_t aoID = 0;
    uint32_t emissiveID = 0;

    glm::vec2 uv = glm::vec2(0.0f);         // not the actual uv, just the offset uv
    glm::vec2 uvScale = glm::vec2(1.0f);    // not the actual uv, just the offset uv
    glm::vec4 albedo = glm::vec4(1.0f);
    glm::vec4 normal = glm::vec4(0.0f);
    float metallic  = 0.0f;
    float roughness = 1.0f;
    float ao        = 1.0f;
    float emissive  = 1.0f;
    float alphaCutoff = 0.0f;       // 0 is opaque, otherwise texels with albedo alpha below it are discarded
    int metallicChannel  = 2;       // 0 = R, 1 = G, 2 = B, 3 = A
    int roughnessChannel = 1;
    int aoChannel        = 0;
    int roughnessIsSmoothness = 0;  // 1 for gloss/smoothness maps, the shader reads 1 - value
};

static_assert(sizeof(MaterialDesc) == 108, "MaterialDesc must match the shader Material layout");
