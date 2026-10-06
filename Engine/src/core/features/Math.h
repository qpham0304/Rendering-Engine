#include "MathIncludes.h"

namespace Math {
    bool decomposeTransform(const glm::mat4& transform, glm::vec3& translation, glm::vec3& rotation, glm::vec3& scale);
    glm::mat4 faceCameraBillboard(const glm::mat4& cameraView, const glm::mat4& model);
    glm::mat4 createRandomTransform(glm::vec3 ranges, glm::vec3 scale);

    struct VQS {
        glm::vec3 position {};
        glm::vec3 rotation {};
        glm::vec3 scale {};

        glm::mat4 toMat4() { return glm::mat4(1.0); }
    };

    // slerp();
    // lerp();


}