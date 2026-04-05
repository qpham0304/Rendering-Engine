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

    glm::vec2 uv;
    glm::vec4 albedo;
    glm::vec4 normal;
    float metallic;
    float roughness;
    float ao;
    float emissive;
};