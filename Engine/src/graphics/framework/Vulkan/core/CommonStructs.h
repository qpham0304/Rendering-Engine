#pragma once 

#include <cstdint>

namespace Common {
    const int MAX_INSTANCES = 1024 * 10;
    const int MAX_LIGHTS = 1000;

    struct PointLight {
        glm::vec4 color;
        glm::vec4 position;
        float intensity = 0.0f;
        float padding[3] = {};
    };

    struct PushBDA {
        uint64_t objectsRef;
		uint64_t boneMatricesRef;
    };

    enum ObjectFlags : uint32_t {
        OBJECT_LIGHT_PROBE = 1 << 0,    // for the shader to know what the object is
    };

    struct Object {
        uint64_t vertexAddress = 0;
        uint64_t indexAddress = 0;
        uint64_t materialsRef = 0;
        uint64_t materialIndicesRef = 0;
        glm::mat4 model;
        glm::mat4 prevModel;
        uint32_t boneMatricesIdx = 0;
        uint32_t materialIdx = 0;
        uint32_t flags = 0;
        uint32_t padding = 0;
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