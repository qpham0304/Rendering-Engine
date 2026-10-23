#pragma once

#include "core/features/MathIncludes.h"
#include <nlohmann/json.hpp>

// nlohmann adl hooks for the glm types reflected structs use, vectors and matrices are written as arrays
namespace glm {
	inline void to_json(nlohmann::json& j, const glm::ivec3& v) {
		j = nlohmann::json{v.x, v.y, v.z};
	}

	inline void from_json(const nlohmann::json& j, glm::ivec3& v) {
		j.at(0).get_to(v.x);
		j.at(1).get_to(v.y);
		j.at(2).get_to(v.z);
	}

	inline void to_json(nlohmann::json& j, const glm::vec3& v) {
        j = nlohmann::json{v.x, v.y, v.z};
    }

    inline void from_json(const nlohmann::json& j, glm::vec3& v) {
        j.at(0).get_to(v.x);
        j.at(1).get_to(v.y);
        j.at(2).get_to(v.z);
    }

    inline void to_json(nlohmann::json& j, const glm::vec4& v) {
        j = nlohmann::json{v.x, v.y, v.z, v.w};
    }

    inline void from_json(const nlohmann::json& j, glm::vec4& v) {
        j.at(0).get_to(v.x);
        j.at(1).get_to(v.y);
        j.at(2).get_to(v.z);
        j.at(3).get_to(v.w);
    }

	inline void to_json(nlohmann::json& j, const glm::mat4& m) {
        j = nlohmann::json{m[0], m[1], m[2], m[3]};
    }

    inline void from_json(const nlohmann::json& j, glm::mat4& m) {
        j.at(0).get_to(m[0]);
        j.at(1).get_to(m[1]);
        j.at(2).get_to(m[2]);
		j.at(3).get_to(m[3]);
    }
}
