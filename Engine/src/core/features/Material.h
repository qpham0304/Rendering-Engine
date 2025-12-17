#pragma once

#include <vector>

struct MaterialDesc {
    std::vector<uint32_t> albedoIDs;
    std::vector<uint32_t> normalIDs;
    std::vector<uint32_t> metallicIDs;
    std::vector<uint32_t> roughnessIDs;
    std::vector<uint32_t> aoIDs;
    std::vector<uint32_t> emissiveIDs;
};