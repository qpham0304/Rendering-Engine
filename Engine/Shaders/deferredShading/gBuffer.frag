#version 330 core
layout (location = 0) out vec3 gPosition;
layout (location = 1) out vec4 gNormal;
layout (location = 2) out vec4 gAlbedoSpec;

in vec2 uv;
in vec3 fragPos;
in vec3 normal;

uniform sampler2D albedoMap;
uniform sampler2D metallicMap;
uniform float reflectiveMap = 0.0f; // 0.0 for non reflective and 1.0 for reflective

void main() {
    // store the fragment position vector in the first gbuffer texture
    gPosition = fragPos;
    // also store the per-fragment normals into the gbuffer
    gNormal = vec4(normalize(normal), reflectiveMap);
    // and the diffuse per-fragment color
    gAlbedoSpec.rgb = texture(albedoMap, uv).rgb;
    // gAlbedoSpec.rgb = vec3(1.0);//texture(albedoMap, uv).rgb;
    // store specular intensity in gAlbedoSpec's alpha component
    gAlbedoSpec.a = texture(metallicMap, uv).r;
}  