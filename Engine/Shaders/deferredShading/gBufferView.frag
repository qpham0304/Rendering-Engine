#version 460 core
layout (location = 0) out vec4 gNormal;
layout (location = 1) out vec4 gAlbedoSpec;
layout (location = 2) out vec4 gSpecular;

in vec2 uv;
in vec3 fragPos;
in vec3 normal;

uniform sampler2D albedoMap;
uniform sampler2D metallicMap;
uniform float reflectiveMap = 0.0f; // 0.0 for non reflective and 1.0 for reflective

void main() {
    // gNormal = vec4(normal, reflectiveMap);
    // gNormal.a = reflectiveMap;
    gNormal.xyz = normalize(normal);
    gNormal.a = reflectiveMap;
    gAlbedoSpec.rgb = texture(albedoMap, uv).rgb;
    gAlbedoSpec.a = texture(metallicMap, uv).r;
    gSpecular.a = texture(metallicMap, uv).r;
}  