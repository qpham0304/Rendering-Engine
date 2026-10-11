#include "Math.h"

float Math::dot(const Quat &q1, const Quat &q2)
{
    return q1.x * q2.x + q1.y * q2.y + q1.z * q2.z + q1.w * q2.w;
}

Math::Quat Math::normalize(const Quat &q)
{
    Quat result{};
    float length = Math::length(q);
    if(length > 0.0f) {
        float invLength = 1.0f / length;
        result.x = q.x * invLength;
        result.y = q.y * invLength;
        result.z = q.z * invLength;
        result.w = q.w * invLength;
    }
    return result;
}

Math::Quat Math::conjugate(Quat q)
{
    return {-q.x, -q.y, -q.z, q.w};
}

Math::Quat Math::inverse(Quat quat)
{
    return conjugate(quat);
}

float Math::length(Quat q)
{
    return std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
}

glm::vec3 Math::lerp(const glm::vec3 &a, const glm::vec3 &b, float t)
{
    glm::vec3 result{};
    result.x = a.x * (1 - t) + b.x * t;
    result.y = a.y * (1 - t) + b.y * t;
    result.z = a.z * (1 - t) + b.z * t;
    return result;
}

Math::Quat Math::nlerp(const Quat &a, const Quat &b, float t)
{
    float dot = Math::dot(a, b);
    Quat tempB = b;

    // take the shorter path if dot product is negative
    if (dot < 0.0f) {
        tempB = {-b.x, -b.y, -b.z, -b.w};
    }

    Quat result{};
    result.x = a.x * (1 - t) + tempB.x * t;
    result.y = a.y * (1 - t) + tempB.y * t;
    result.z = a.z * (1 - t) + tempB.z * t;
    result.w = a.w * (1 - t) + tempB.w * t;

    return Math::normalize(result);
}

Math::Quat Math::slerp(const Quat &a, const Quat &b, float t)
{
    Quat result{};
    float dot = Math::dot(a, b);   // cosTheta
    Quat tempB = b;

    // take the shorter path if dot product is negative
    if(dot < 0.0) {
        tempB = {-b.x, -b.y, -b.z, -b.w};
        dot = -dot;
    }

    if(dot > 0.9995) {
        return Math::nlerp(a, tempB, t);
    }
    
    float theta_0 = glm::acos(dot);
    float theta = theta_0 * t;
    float sinTheta_0 = glm::sin(theta_0);
    float sinTheta = glm::sin(theta);

    float s1 = glm::cos(theta) - dot * sinTheta / sinTheta_0;
    float s2 = sinTheta / sinTheta_0;
    
    result.x = a.x * s1 + tempB.x * s2;
    result.y = a.y * s1 + tempB.y * s2;
    result.z = a.z * s1 + tempB.z * s2;
    result.w = a.w * s1 + tempB.w * s2;

    return result;
}