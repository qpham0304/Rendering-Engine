#version 460 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

out vec3 fragPos;
out vec2 uv;
out vec3 normal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform bool invertedNormals = false;
uniform bool invertedTexCoords = false;

void main()
{
    vec4 worldPos = model * vec4(aPos, 1.0);
    fragPos = worldPos.xyz; 
    uv = aTexCoords;
    invertedTexCoords ? uv.y *= -1 : uv.y; // TODO: only use this to flip texture horizontally
    
    mat3 normalMatrix = transpose(inverse(mat3(model)));
    normal = normalMatrix * (invertedNormals ? -aNormal : aNormal);

    gl_Position = projection * view * worldPos;
}