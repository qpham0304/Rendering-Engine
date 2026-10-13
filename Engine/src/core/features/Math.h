#pragma once

#include "MathIncludes.h"

namespace Math {
    bool decomposeTransform(const glm::mat4& transform, glm::vec3& translation, glm::vec3& rotation, glm::vec3& scale);
    glm::mat4 faceCameraBillboard(const glm::mat4& cameraView, const glm::mat4& model);
    glm::mat4 createRandomTransform(glm::vec3 ranges, glm::vec3 scale);

    
    struct Quat {
        float x{ 0.0f };
        float y{ 0.0f };
        float z{ 0.0f };
        float w{ 1.0f };

        Quat& operator=(const Quat& other) {
            if(this != &other) {
                x = other.x;
                y = other.y;
                z = other.z;
                w = other.w;
            }
            return *this;
        }

        Quat operator+(const Quat& other) const {
            return { x + other.x, y + other.y, z + other.z, w + other.w };
        }

        Quat operator*(const Quat& other) const {
            Quat result;
            result.x = w * other.x + x * other.w + y * other.z - z * other.y;
            result.y = w * other.y - x * other.z + y * other.w + z * other.x;
            result.z = w * other.z + x * other.y - y * other.x + z * other.w;
            result.w = w * other.w - x * other.x - y * other.y - z * other.z;
            
            return result;
        }

        void fromGlmQuat(const glm::quat& other) {
            x = other.x;
            y = other.y;
            z = other.z;
            w = other.w;
        }

        glm::mat4 toMat4() {
            float x2 = x + x;
            float y2 = y + y;
            float z2 = z + z;

            float xx = x * x2;
            float xy = x * y2;
            float xz = x * z2;

            float yy = y * y2;
            float yz = y * z2;
            float zz = z * z2;

            float wx = w * x2;
            float wy = w * y2;
            float wz = w * z2;

            glm::mat4 m;

            m[0][0] = 1.0f - (yy + zz);
            m[0][1] = xy + wz;
            m[0][2] = xz - wy;
            m[0][3] = 0.0f;

            m[1][0] = xy - wz;
            m[1][1] = 1.0f - (xx + zz);
            m[1][2] = yz + wx;
            m[1][3] = 0.0f;

            m[2][0] = xz + wy;
            m[2][1] = yz - wx;
            m[2][2] = 1.0f - (xx + yy);
            m[2][3] = 0.0f;

            m[3][0] = 0.0f;
            m[3][1] = 0.0f;
            m[3][2] = 0.0f;
            m[3][3] = 1.0f;

            return m;
        }
    };
    struct VQS {
        glm::vec3 position {};
        Quat rotation {};
        glm::vec3 scale {};

        glm::mat4 toMat4() { 
            glm::mat4 T = glm::translate(glm::mat4(1.0), position);
            glm::mat4 R = rotation.toMat4();
            glm::mat4 S = glm::scale(glm::mat4(1.0), scale);
            // glm::mat4 T(1.0);
            // T[3] = glm::vec4(position, 1.0);
            // glm::mat4 R(rotation.toMat4());
            // glm::mat4 S(1.0);
            // S[0][0] = scale.x;
            // S[1][1] = scale.y;
            // S[2][2] = scale.z;

            return T * R * S;
        }
        
    };


    float dot(const Quat& q1, const Quat& q2);
    Quat normalize(const Quat& quat);
    Quat conjugate(Quat quat);
    Quat inverse(Quat quat);
    float length(Quat quat);
    
    glm::vec3 lerp(const glm::vec3& a, const glm::vec3& b, float t);
    Quat nlerp(const Quat& a, const Quat& b, float t);
    Quat slerp(const Quat& a, const Quat& b, float t);
    VQS interpolate(const VQS& a, const VQS& b, float t);

}