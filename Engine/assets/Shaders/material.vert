#version 460 core

layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 aColor;
layout (location = 2) in vec2 vertexUV;
layout (location = 3) in vec3 aNormal;
layout (location = 4) in vec3 tangent;
layout (location = 5) in vec3 bitangent;
layout (location = 6) in ivec4 boneIds;
layout (location = 7) in vec4 weights;

out VS_OUT {
    vec2 uv;
    vec3 normal;
    vec3 updatedPos;
    vec4 fragPosLight;
    vec4 totalPos;
    mat4 model;
    vec3 lightPos;
    vec3 camPos;
    vec3 fragPos;
    mat3 TBN;
} vert_out;


uniform mat4 matrix;
uniform mat3 normalMatrix;
uniform mat4 mvp;
uniform mat4 lightMVP;
//uniform mat4 normalMap;
uniform bool hasAnimation = true;
uniform vec3 camPos;
uniform vec3 lightPos;


const int MAX_BONES = 100;
const int MAX_BONE_INFLUENCE = 4;
uniform mat4 finalBonesMatrices[MAX_BONES];


void main()
{
    vec3 T = normalize(normalMatrix * tangent);
    vec3 N = normalize(normalMatrix * aNormal);
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);
    mat3 TBN = transpose(mat3(T, B, N));

	vert_out.uv = vertexUV;
	//vert_out.normal = normalize(normalMatrix * aNormal);
	vert_out.normal = (normalMatrix * aNormal);

	vec4 totalPosition = vec4(0.0f);
    for(int i = 0 ; i < MAX_BONE_INFLUENCE ; i++)
    {
        if(boneIds[i] == -1) 
            continue;
        if(boneIds[i] >= MAX_BONES) 
        {
            totalPosition = vec4(pos,1.0f);
            break;
        }
        vec4 localPosition = finalBonesMatrices[boneIds[i]] * vec4(pos,1.0f);
        totalPosition += localPosition * weights[i];
        vec3 localNormal = mat3(finalBonesMatrices[boneIds[i]]) * vert_out.normal;
    }

    
    int condition = int(hasAnimation);
    totalPosition = condition * totalPosition + (1 - condition) * vec4(pos, 1.0f);

    vert_out.totalPos = totalPosition;
	vert_out.updatedPos = (matrix * totalPosition).xyz;
	vert_out.fragPosLight = lightMVP * vec4(vert_out.updatedPos, 1.0f);
    vert_out.model = matrix;
    vert_out.TBN = TBN;
    vert_out.lightPos = lightPos;
    vert_out.camPos = camPos;
    vert_out.fragPos = vert_out.updatedPos;
    gl_Position = mvp * vec4(vert_out.updatedPos, 1.0f);
}




