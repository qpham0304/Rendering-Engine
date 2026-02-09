#version 460

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;
layout(location = 4) in vec3 inTangent;

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
layout(location = 2) out vec3 outWorldPos; // Renamed for clarity
layout(location = 3) out mat3 outTBN;

void main() {
    mat4 modelMatrix = ssbo.models[gl_InstanceIndex];
    
    // 1. Position Calculation
    vec4 worldPos = modelMatrix * vec4(inPosition, 1.0);
    outWorldPos = worldPos.xyz;
    
    // Still need ubo.view * ubo.proj for the screen position!
    gl_Position = ubo.proj * ubo.view * worldPos;

    // 2. World Space Normal Matrix
    mat3 normalMatrix = transpose(inverse(mat3(modelMatrix)));
    
    // 3. Construct TBN in World Space
    vec3 T = normalize(normalMatrix * inTangent);
    vec3 N = normalize(normalMatrix * inNormal);
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);

    outTBN = mat3(T, B, N);
    outTexCoord = inTexCoord;
    outNormal = N; // Base normal for safety checks
}