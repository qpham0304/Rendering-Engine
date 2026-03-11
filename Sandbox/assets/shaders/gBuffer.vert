#version 460

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;
layout(location = 4) in vec3 inTangent;
layout(location = 5) in vec3 inBiTangent;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 invNormal;
    mat4 view;
    mat4 proj;
    vec4 cameraPos;
} ubo;

layout(set = 0, binding = 1, std430) readonly buffer StorageBufferObject {
    mat4 models[];
} ssbo;

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec2 outTexCoord;
layout(location = 2) out vec3 outWorldPos;
layout(location = 3) out mat3 outTBN;

void main() {
    mat4 modelMatrix = ssbo.models[gl_InstanceIndex];
    vec4 worldPos = modelMatrix * vec4(inPosition, 1.0);
    outWorldPos = worldPos.xyz;

    mat3 normalMatrix = transpose(inverse(mat3(modelMatrix)));
    vec3 N = normalize(normalMatrix * inNormal);
    vec3 tangent;
    vec3 c1 = cross(inNormal, vec3(0.0, 0.0, 1.0));
    vec3 c2 = cross(inNormal, vec3(0.0, 1.0, 0.0));
    
    if (length(c1) > length(c2)) {
        tangent = c1;
    } else {
        tangent = c2;
    }
    
    vec3 T = normalize(normalMatrix * tangent);
    vec3 B = normalize(cross(N, T));

    outTBN = mat3(T, B, N);
    outTexCoord = inTexCoord;
    outNormal = N;
    gl_Position = ubo.proj * ubo.view * worldPos;
}