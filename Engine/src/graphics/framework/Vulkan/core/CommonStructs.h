#pragma once 

#include <cstdint>

namespace Common {
    const int MAX_INSTANCES = 1024 * 10;

    struct PushBDA {
        uint64_t objectsRef;
		uint64_t boneMatricesRef;
        uint32_t objectIdx;
		uint32_t boneMatricesIdx;
    };

    struct ObjectRefs {
        uint64_t vertexAddress = 0;
        uint64_t indexAddress = 0;
        uint64_t materialsRef = 0;
        uint64_t materialIndicesRef = 0;
    };

    
	struct UniformBufferObject {
		glm::mat4 invNormal;
		glm::mat4 view;
		glm::mat4 prevViewProj;
		glm::mat4 proj;
		glm::vec4 cameraPos;
		glm::mat4 invView;
		glm::mat4 invProj;
		float width;
		float height;
	};

}