#version 460

#extension GL_GOOGLE_include_directive : require

#include "common/buffers.glsl"

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec4 fragColor;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 invNormal;
    mat4 view;
    mat4 prevViewProj;
    mat4 proj;
    vec4 cameraPos;
    mat4 invView;
    mat4 invProj;
    float width;
    float height;
} ubo;

// mirrored by RibbonPointGPU in RibbonRendererVulkan.h
struct RibbonPoint {
    vec3 position;
    float width;
    vec4 color;
    float u;            // distance along the ribbon already tiled and scrolled
};

layout(buffer_reference, scalar) buffer RibbonPoints { RibbonPoint p[]; };

layout(push_constant) uniform RibbonPushConstant {
    RibbonPoints pointsRef;
    uint firstPoint;    // this strip's points in the shared buffer
    uint pointCount;    // pairs for a paired strip
    int textureID;
    uint flags;
} pc;

const uint STRIP_FLAG_PAIRED = 1u;  // match STRIP_FLAG_PAIRED in RibbonRendererVulkan.h

// one quad per segment, x picks the segment end, y the side
const vec2 corners[6] = vec2[](
    vec2(0.0, -1.0), vec2(1.0, -1.0), vec2(1.0, 1.0),
    vec2(0.0, -1.0), vec2(1.0, 1.0),  vec2(0.0, 1.0)
);

RibbonPoint pointAt(uint i) {
    return pc.pointsRef.p[pc.firstPoint + i];
}

void main() {
    uint segment = gl_VertexIndex / 6;
    vec2 corner = corners[gl_VertexIndex % 6];
    uint index = segment + uint(corner.x);

    // a blade sweep already has both edges in world space, inner edge then tip edge
    if ((pc.flags & STRIP_FLAG_PAIRED) != 0u) {
        RibbonPoint edge = pointAt(index * 2u + (corner.y > 0.0 ? 1u : 0u));
        fragTexCoord = vec2(edge.u, corner.y * 0.5 + 0.5);
        fragColor = edge.color;
        gl_Position = ubo.proj * ubo.view * vec4(edge.position, 1.0);
        return;
    }

    RibbonPoint point = pointAt(index);

    // neighbours on both sides give each joint one shared direction, so segments meet without gaps or overlaps
    uint prev = index > 0 ? index - 1 : index;
    uint next = index + 1 < pc.pointCount ? index + 1 : index;
    vec3 tangent = pointAt(next).position - pointAt(prev).position;
    vec3 toCamera = ubo.cameraPos.xyz - point.position;

    vec3 side = cross(tangent, toCamera);
    float sideLength = length(side);
    // looking straight down the ribbon leaves no width direction, any perpendicular one will do
    side = sideLength > 1e-6 ? side / sideLength : normalize(cross(tangent + vec3(1e-3), vec3(0.0, 1.0, 0.0)));

    vec3 worldPos = point.position + side * (corner.y * 0.5 * point.width);

    fragTexCoord = vec2(point.u, corner.y * 0.5 + 0.5);
    fragColor = point.color;
    gl_Position = ubo.proj * ubo.view * vec4(worldPos, 1.0);
}
