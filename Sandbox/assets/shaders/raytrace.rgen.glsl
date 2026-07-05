#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_nonuniform_qualifier : enable

struct RayPayload {
    vec3 hitPos;
    vec3 bc;
    int instanceIndex;
    int primitiveIndex;
    uint hit;
    uint seed;
};

struct Material {
    uint albedoIdx;
    uint normalIdx;
    uint metalnessIdx;
    uint roughnessIdx;
    uint aoIdx;
    uint emissiveIdx;

    vec2 uvOffset;
    vec2 uvScale;
    vec4 albedoFactor;
    vec4 normalFactor;
    float metallicFactor;
    float roughnessFactor;
    float aoFactor;
    float emissiveFactor;
};

const uint MAX_BONE_INFLUENCE =  4;
const float PI = 3.14159265359;

struct Vertex {
    vec3 pos; 
    vec3 col; 
    vec2 uv; 
    vec3 normal; 
    vec3 tangent; 
    vec3 biTangent;
    int m_BoneIDs[MAX_BONE_INFLUENCE]; 
    float m_Weights[MAX_BONE_INFLUENCE];
};

struct Object {
    uint64_t vertexAddress; 
    uint64_t indexAddress;
    uint64_t materialsRef; 
    uint64_t materialIndiciesRef;
};


layout(location = 0) rayPayloadEXT RayPayload payload;

layout(set = 0, binding = 0) uniform accelerationStructureEXT topLevelAS;
layout(set = 0, binding = 1, rgba32f) uniform image2D outputImage;
layout(set = 0, binding = 2) uniform UniformBufferObject {
    mat4 view;
    mat4 prevViewProj;
    mat4 proj;
    vec4 cameraPos;
    mat4 invView;
    mat4 invProj;
    float width;
    float height;
    float frameSeed;
    int frameCount;
    bool clear;
    bool explicitPass;
} ubo;

struct Light {
    vec4 color; 
    vec4 position;
    float intensity;
    int materialIdx;
};

layout(set = 0, binding = 3, std430) readonly buffer LightSSBO {
    Light lights[];
} lightSSBO;


layout(set = 1, binding = 0) uniform sampler2D samplerImages[];


layout(buffer_reference, scalar) buffer ObjectsBuffer { Object objects[]; };
layout(buffer_reference, scalar) buffer Vertices { Vertex v[]; };
layout(buffer_reference, scalar) buffer Indices { uint i[]; };
layout(buffer_reference, scalar) buffer MaterialsBuffer { Material m[]; };
layout(buffer_reference, scalar) readonly buffer MatIndicesBuffer { uint i[]; };

layout(push_constant) uniform PushConstant {
    ObjectsBuffer objRef;
    uint objIdx;
} pc;

uint tea(uint val0, uint val1) {
  uint v0 = val0;
  uint v1 = val1;
  uint s0 = 0;

  for(uint n = 0; n < 16; n++)
  {
    s0 += 0x9e3779b9;
    v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
    v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
  }

  return v0;
}

uint lcg(inout uint prev) {
    uint LCG_A = 1664525u;
    uint LCG_C = 1013904223u;
    prev       = (LCG_A * prev + LCG_C);
    return prev & 0x00FFFFFF;
}

float rnd(inout uint prev) {
    return (float(lcg(prev)) / float(0x01000000));
}


// Cosine-weighted sampling
// get a random rayDir in a hemisphere for diffuse material
vec3 sampleHemisphere(vec3 normal, inout uint seed) {
    float phi = 2.0 * 3.14159265 * rnd(seed);
    float cosTheta = rnd(seed);
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    
    vec3 tangent = normalize(cross(normal, abs(normal.x) > 0.1 ? vec3(0, 1, 0) : vec3(1, 0, 0)));
    vec3 bitangent = cross(normal, tangent);
    
    return tangent * (cos(phi) * sinTheta) + bitangent * (sin(phi) * sinTheta) + normal * cosTheta;
}


void getHitObjectData(out Material mat, out vec3 nrm) {
    Object obj = pc.objRef.objects[payload.instanceIndex];
    
    // dereference addresses
    Vertices vertices = Vertices(obj.vertexAddress);
    Indices indices = Indices(obj.indexAddress);
    MaterialsBuffer materials = MaterialsBuffer(obj.materialsRef);
    MatIndicesBuffer matIndices = MatIndicesBuffer(obj.materialIndiciesRef);
  
    // Use gl_PrimitiveID to access the triangle's vertices and material
    uint i0 = indices.i[3 * payload.primitiveIndex + 0];        // flatten triangle hit indices
    uint i1 = indices.i[3 * payload.primitiveIndex + 1];
    uint i2 = indices.i[3 * payload.primitiveIndex + 2];

    uint matIdx   = matIndices.i[payload.primitiveIndex];       // triangles material index
    mat = materials.m[matIdx];                                  // triangles material

    // Vertex of the triangle (Vertex has pos, nrm, tex)
    Vertex v0 = vertices.v[i0];
    Vertex v1 = vertices.v[i1];
    Vertex v2 = vertices.v[i2];

    // Compute normal at hit position using the provided barycentric coordinates.
    const vec3 bc = payload.bc;                                 // The barycentric coordinates of the hit point
    nrm  = bc.x*v0.normal + bc.y*v1.normal + bc.z*v2.normal;    // Normal = combo of three vertex normals

    // If the material has a texture, read texture and use as the point's diffuse color.
    if (mat.albedoIdx != uint(0)) {
        vec2 uv =  bc.x*v0.uv + bc.y*v1.uv + bc.z*v2.uv;
        mat.albedoFactor = texture(samplerImages[mat.albedoIdx], uv);
    }
    if (mat.metalnessIdx != uint(0)) {
        vec2 uv =  bc.x*v0.uv + bc.y*v1.uv + bc.z*v2.uv;
        mat.metallicFactor = texture(samplerImages[mat.metalnessIdx], uv).b * mat.metallicFactor;
    }
    if (mat.roughnessIdx != uint(0)) {
        vec2 uv =  bc.x*v0.uv + bc.y*v1.uv + bc.z*v2.uv;
        mat.roughnessFactor = texture(samplerImages[mat.roughnessIdx], uv).g * mat.roughnessFactor;
    }
}


vec3 SampleTriangle(vec3 A, vec3 B, vec3 C) {
    float b1 = rnd(payload.seed);
    float b2 = rnd(payload.seed);
    float b0 = 1 - b1 - b2;

    if (b0 < 0.0) {   // outside means reflect back in
        b1 = 1.0 - b1;
        b2 = 1.0 - b2;
        b0 = 1.0 - b1 - b2;
    }

    return b0*A + b1*B + b2*C;
}

float V_SmithGGXCorrelated(float NdotV, float NdotL, float alpha) {
    float a2 = alpha * alpha;
    float GV = NdotL * sqrt(NdotV * NdotV * (1.0 - a2) + a2);
    float GL = NdotV * sqrt(NdotL * NdotL * (1.0 - a2) + a2);
    return 0.5 / (GV + GL + 1e-6);
}

vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness) {
    float a = roughness * roughness;
    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a*a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    
    vec3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;
    
    vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, N));
    vec3 bitangent = cross(N, tangent);
    
    return normalize(tangent * H.x + bitangent * H.y + N * H.z);
}


// D factor
float DistributionGGX(vec3 N, vec3 H, float alphaRoughness) {
    float NdotH = max(dot(N, H), 0.0);
    float a2 = alphaRoughness * alphaRoughness;
    float NdotH2 = NdotH * NdotH;
    
    float numerator = a2;
    float denominator = (NdotH2 * (a2 - 1.0) + 1.0);
    float D = numerator / (PI * denominator * denominator);
    
    return D;
}

// cos(theta)/pi 
// p for sampling hemisphere for diffuse material
float pdfDiffused(vec3 N, vec3 L) {     
    return max(dot(N, L), 0.0) / PI;
}

// (D * NdotH)/(4 * VdotH) 
// p for importance sample GGX for specular material
float pdfSpecular(vec3 N, vec3 V, vec3 L, float alphaRoughness) {
    vec3 H = normalize(L + V);
    float NdotH = max(dot(N, H), 0.0);
    float VdotH = max(dot(V, H), 0.0);
    float D = DistributionGGX(N, H, alphaRoughness);

    float num = D * NdotH;
    float denom = 4.0 * VdotH + 0.0001;

    return num / (denom + 1e-7);
}

// F factor
vec3 Fresnel(vec3 Ks, float d) {
    // return Ks + (1 - Ks) * pow(1 - abs(d), 5);
    return Ks + (1 - Ks) * pow(clamp(1.0 - d, 0.0, 1.0), 5);
}

vec3 FresnelRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float SchlickGGX(float NdotV, float alphaRoughness) {
    float r = (alphaRoughness + 1.0);
    float k = (r*r) / 8.0;

    float numerator   = NdotV;
    float denominator = NdotV * (1.0 - k) + k;
	
    return numerator / denominator;
}

//G factor
float GGXSmith(float NdotV, float NdotL, float alphaRoughness) {
    float GV = SchlickGGX(NdotV, alphaRoughness);
    float GL = SchlickGGX(NdotL, alphaRoughness);
    
    return GV * GL;
}

vec3 CookTorranceSpecular(vec3 N, vec3 V, vec3 L, vec3 F, float alphaRoughness) {
    vec3 H = normalize(V + L);
    
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float NdotH = max(dot(N, H), 0.0);
    float VdotH = max(dot(V, H), 0.0);

    float D = DistributionGGX(N, H, alphaRoughness);
    float G = GGXSmith(NdotV, NdotL, alphaRoughness);

    return (D * F * G) / (4.0 * NdotV * NdotL + 1e-5);
}

void main() {
ivec2 pixelCoords = ivec2(gl_LaunchIDEXT.xy);
    payload.seed = tea(gl_LaunchIDEXT.y * gl_LaunchSizeEXT.x + gl_LaunchIDEXT.x, uint(ubo.frameSeed));

    // jittered Camera Ray for anti aliasing
    vec2 subpixel = vec2(rnd(payload.seed), rnd(payload.seed));
    vec2 pixelUV = (vec2(pixelCoords) + subpixel) / vec2(gl_LaunchSizeEXT.xy);
    vec2 d = pixelUV * 2.0 - 1.0;

    vec3 rayOrigin = vec3(ubo.invView * vec4(0.0, 0.0, 0.0, 1.0)); 
    vec4 target = ubo.invProj * vec4(d.x, d.y, 1.0, 1.0);
    vec3 rayDir = vec3(ubo.invView * vec4(normalize(target.xyz), 0));
    
    vec3 accumulatedColor = vec3(0.0);  // C
    vec3 throughput = vec3(1.0);        // W
    
    const int numBounces = 8; // 32 is overkill but might needed for indoor scenes
    for (int i = 0; i < numBounces; i++) {
        payload.hit = 0;
        traceRayEXT(topLevelAS, gl_RayFlagsOpaqueEXT, 0xFF, 0, 0, 0, rayOrigin, 0.001, rayDir, 10000.0, 0);

        // if missed, sample the environment sky
        if (payload.hit == 0) {
            accumulatedColor += throughput * mix(vec3(0.02), vec3(0.1, 0.3, 0.5), max(rayDir.y, 0.0));
            break;
        }
        
        Material mat;
        vec3 worldNormal;
        getHitObjectData(mat, worldNormal);
        vec3 N = normalize(worldNormal);
        vec3 V = -rayDir;

        vec3 emissive = mat.emissiveFactor * mat.albedoFactor.rgb;
        if (mat.emissiveIdx != uint(0)) {
            Object obj = pc.objRef.objects[payload.instanceIndex];
            Vertices vertices = Vertices(obj.vertexAddress);
            Indices indices = Indices(obj.indexAddress);

            uint i0 = indices.i[3 * payload.primitiveIndex + 0];
            uint i1 = indices.i[3 * payload.primitiveIndex + 1];
            uint i2 = indices.i[3 * payload.primitiveIndex + 2];

            vec2 uv = payload.bc.x * vertices.v[i0].uv + 
                    payload.bc.y * vertices.v[i1].uv + 
                    payload.bc.z * vertices.v[i2].uv;

            emissive = texture(samplerImages[nonuniformEXT(mat.emissiveIdx)], uv).rgb * mat.emissiveFactor;
        } else {
            // if no texture, just use the factor/albedo as a flat emissive color
            emissive = mat.albedoFactor.rgb * mat.emissiveFactor;
        }

        if (ubo.explicitPass) {
            
        }

        accumulatedColor += throughput * emissive;

        float roughness = clamp(mat.roughnessFactor, 0.02, 1.0);
        float alpha = roughness * roughness;
        vec3 F0 = mix(vec3(0.04), mat.albedoFactor.rgb, mat.metallicFactor);

        // select brdf lobe between diffuse and specular
        // calculate Fresnel for the viewing angle to decide probability
        vec3 F_view = FresnelRoughness(max(dot(N, V), 0.0), F0, roughness);
        float specProb = clamp(max(F_view.r, max(F_view.g, F_view.b)), 0.1, 0.9);
        
        vec3 L;
        vec3 BRDF;
        float pdf;

        if (rnd(payload.seed) < specProb) {
            // GGX Importance Sampling specular reflection
            vec3 H = ImportanceSampleGGX(vec2(rnd(payload.seed), rnd(payload.seed)), N, roughness);
            L = reflect(-V, H);
            
            float NdotL = max(dot(N, L), 0.0);
            float NdotV = max(dot(N, V), 0.0);
            float NdotH = max(dot(N, H), 0.0);
            float VdotH = max(dot(V, H), 0.0);

            if (NdotL > 0.0) {
                float D = DistributionGGX(N, H, alpha);
                vec3 F = Fresnel(F0, VdotH);
                float Vis = V_SmithGGXCorrelated(NdotV, NdotL, alpha);
                
                BRDF = F * D * Vis;
                pdf = (D * NdotH) / (4.0 * VdotH) * specProb;
            } else {
                break;
            }
        } else {
            // cosine weight diffuse reflection 
            L = sampleHemisphere(N, payload.seed);
            float NdotL = max(dot(N, L), 0.0);
            
            vec3 F = FresnelRoughness(NdotL, F0, roughness);
            vec3 kD = (vec3(1.0) - F) * (1.0 - mat.metallicFactor);
            
            BRDF = kD * (mat.albedoFactor.rgb / PI);
            pdf = (NdotL / PI) * (1.0 - specProb);
        }

        if (pdf <= 0.0) {
            break;
        }

        vec3 weight = (BRDF * max(dot(N, L), 0.0)) / pdf;
        throughput *= weight;

        // russian roulette flip to have more bounces
        // add more weight to surviving ray and terminate ray that could not make it
        if (i > 3) {
            float p = clamp(max(throughput.r, max(throughput.g, throughput.b)), 0.1, 0.95);
            if (rnd(payload.seed) > p) break;
            throughput /= p;
        }

        rayOrigin = payload.hitPos + N * 0.001; // offset along normal to avoid self-intersection
        rayDir = L;
    }
    
    vec4 old = imageLoad(outputImage, pixelCoords);
    vec3 ave = old.xyz;
    float numSamples = old.w;

    if(ubo.clear) {
        ave = vec3(0.0);
        numSamples = 0.0;        
        imageStore(outputImage, pixelCoords, vec4(ave, numSamples));
    }

    ave += (accumulatedColor - ave)/(numSamples + 1);
    numSamples += 1;
    imageStore(outputImage, pixelCoords, vec4(ave, numSamples));
}