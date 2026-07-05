#include "MathIncludes.h"

namespace Math {
    bool DecomposeTransform(const glm::mat4& transform, glm::vec3& translation, glm::vec3& rotation, glm::vec3& scale);
    glm::mat4 faceCameraBillboard(const glm::mat4& cameraView, const glm::mat4& model);
    glm::mat4 createRandomTransform(glm::vec3 ranges, glm::vec3 scale);
}