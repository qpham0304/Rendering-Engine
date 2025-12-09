#version 460 core
layout (location = 0) out vec4 gAlbedo;
layout (location = 1) out vec4 gNormal;
layout (location = 2) out vec4 gMetalRoughness;
layout (location = 3) out vec4 gEmissive;
layout (location = 4) out vec4 gDUV;

in vec2 uv;
in vec3 fragPos;
in vec3 normal;

uniform sampler2D albedoMap;
uniform sampler2D normalMap;
uniform sampler2D metallicMap;
uniform sampler2D roughnessMap;
uniform sampler2D aoMap;
uniform sampler2D emissiveMap;
uniform sampler2D duvMap;

uniform float reflectiveMap = 0.0f; // 0.0 for non reflective and 1.0 for reflective
// uniform mat4 inverseViewMat;

vec3 getNormalFromMap() {
    vec3 tangentNormal = texture(normalMap, uv).xyz * 2.0 - 1.0;

    vec3 Q1  = dFdx(fragPos);
    vec3 Q2  = dFdy(fragPos);
    vec2 st1 = dFdx(uv);
    vec2 st2 = dFdy(uv);

    vec3 N   = normalize(normal);
    vec3 T  = normalize(Q1*st2.t - Q2*st1.t);
    vec3 B  = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * tangentNormal);
}


void main() {
    gAlbedo.rgb = texture(albedoMap, uv).rgb;
    gAlbedo.a = texture(aoMap, uv).r;
    
    vec3 N = getNormalFromMap();
    gNormal.rgb = normalize(N);
    gNormal.a = 1.0;
    
    gMetalRoughness.r = texture(metallicMap, uv).r;
    gMetalRoughness.g = texture(roughnessMap, uv).g;
    gMetalRoughness.b = texture(metallicMap, uv).b;
    gMetalRoughness.a = texture(roughnessMap, uv).r;
    // gMetalRoughness = texture(roughnessMap, uv);

    gEmissive = texture(emissiveMap, uv);
    gDUV = texture(duvMap, uv);
}  