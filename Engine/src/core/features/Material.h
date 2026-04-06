#pragma once

#include <vector>
#include <glm/glm.hpp>

struct MaterialDesc {
    std::vector<uint32_t> albedoIDs = {};
    std::vector<uint32_t> normalIDs = {};
    std::vector<uint32_t> metallicIDs = {};
    std::vector<uint32_t> roughnessIDs = {};
    std::vector<uint32_t> aoIDs = {};
    std::vector<uint32_t> emissiveIDs = {};
    
    uint32_t materialIdx;

    glm::vec2 uv = glm::vec2(0.0f);     // not the actual uv, just the offset
    glm::vec4 albedo = glm::vec4(1.0f);
    glm::vec4 normal = glm::vec4(0.0f);
    float metallic  = 1.0f;
    float roughness = 1.0f;
    float ao        = 1.0f;
    float emissive  = 1.0f;
};