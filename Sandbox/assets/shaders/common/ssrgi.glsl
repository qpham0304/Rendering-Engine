#ifdef SSR_RAY_QUERY
#extension GL_EXT_ray_query : require
#endif

#include "buffers.glsl"

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform sampler2D u_HiZBuffer;      
layout(set = 0, binding = 1) uniform sampler2D u_GBufferNormals; 
layout(set = 0, binding = 2) uniform sampler2D u_SceneColor; 
layout(set = 0, binding = 3, rgba16f) writeonly uniform image2D u_ResultImage;

layout(set = 0, binding = 4) uniform CameraData {
    mat4 projection;
    mat4 view;
    mat4 invProj;
    mat4 invView;
    vec4 sunDirection;      // xyz: toward the sun
    vec4 sunColor;          // rgb: color * intensity
    vec4 ddgiGridOrigin;
    ivec4 ddgiProbeCounts;
    float skyIntensity;
    float ddgiSpacing;
    float ddgiNormalBias;
    float ddgiIntensity;
    int ddgiReady;          // the probe atlases hold usable light
} camera;

layout(set = 0, binding = 5) uniform sampler2D depthImage;
layout(set = 0, binding = 6) uniform sampler2D albedoImage;
layout(set = 0, binding = 7) uniform sampler2D pbrImage;
layout(set = 0, binding = 8) uniform sampler2D emissiveImage;
layout(set = 0, binding = 9) uniform sampler2D blueNoise;
layout(set = 0, binding = 14, rgba16f) writeonly uniform image2D u_SpecularImage;
layout(set = 0, binding = 15) uniform sampler2D u_MotionVectors;
layout(set = 0, binding = 16) uniform sampler2D u_PrevFinalColor;  // last frame's combine output: direct + all ambient, hdr

layout(push_constant) uniform Constants {
    vec2 screenRes;         // trace resolution, the output size
    float maxDistance;
    int mostDetailedMip;    // finest hiz level the trace goes down to, log2 of the downsample
    float thickness;
    float time;
    float frameSeed;
    int maxSteps;
    int useTLAS;
    uint64_t objectsRef;
    uint64_t lightsRef;
    uint lightCount;
    float maxSampleLuminance;
    int reflectionOnly;
    uint frameIndex;
} pc;

#ifdef SSR_RAY_QUERY
layout(set = 0, binding = 10) uniform accelerationStructureEXT topLevelAS;
layout(set = 0, binding = 11) readonly buffer SHData {
    vec4 shCoeffs[9];
} sh;
layout(set = 0, binding = 12) uniform sampler2D lastFrameIrradianceAtlas;
layout(set = 0, binding = 13) uniform sampler2D lastFrameVisibilityAtlas;
layout(set = 1, binding = 0) uniform sampler2D samplerImages[];

#include "ddgi.glsl"
#include "rayQueryLighting.glsl"

// the reflected surface is off screen or hidden, so there is no lit color to read back:
// shade the hit the way the ddgi probes do (sun and light components with shadow rays, probe bounce or sky, emissive).
// world space in and out
bool traceScene(vec3 origin, vec3 dir, float maxDistance, out vec3 radiance, out float hitT) {
    radiance = vec3(0.0);
    hitT = 0.0;

    rayQueryEXT rayQuery;
    rayQueryInitializeEXT(rayQuery, topLevelAS, gl_RayFlagsOpaqueEXT, 0xFF, origin, 0.01, dir, maxDistance);
    while (rayQueryProceedEXT(rayQuery)) {}

    if (rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionNoneEXT) {
        return false;
    }

    hitT = rayQueryGetIntersectionTEXT(rayQuery, true);
    int customIndex = rayQueryGetIntersectionInstanceCustomIndexEXT(rayQuery, true);
    int primitiveID = rayQueryGetIntersectionPrimitiveIndexEXT(rayQuery, true);
    vec2 attribs = rayQueryGetIntersectionBarycentricsEXT(rayQuery, true);
    mat4x3 worldToObject = rayQueryGetIntersectionWorldToObjectEXT(rayQuery, true);

    Object obj = ObjectsBuffer(pc.objectsRef).objects[customIndex];
    Vertices vertices = Vertices(obj.vertexAddress);
    Indices indices = Indices(obj.indexAddress);
    MaterialsBuffer materials = MaterialsBuffer(obj.materialsRef);
    MaterialIndicesBuffer matIndices = MaterialIndicesBuffer(obj.materialIndicesRef);

    Vertex v0 = vertices.v[indices.i[primitiveID * 3 + 0]];
    Vertex v1 = vertices.v[indices.i[primitiveID * 3 + 1]];
    Vertex v2 = vertices.v[indices.i[primitiveID * 3 + 2]];

    vec3 bc = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
    vec2 uv = v0.uv * bc.x + v1.uv * bc.y + v2.uv * bc.z;
    vec3 nrmObject = v0.normal * bc.x + v1.normal * bc.y + v2.normal * bc.z;
    if (dot(nrmObject, nrmObject) < 1e-8) {
        nrmObject = cross(v1.pos - v0.pos, v2.pos - v0.pos);
    }
    vec3 N = normalize(vec3(nrmObject * worldToObject));

    // the back of a surface: the ray started inside or behind geometry, nothing reflects from there
    if (dot(N, dir) > 0.0) {
        return true;
    }

    Material mat = materials.m[matIndices.i[primitiveID]];
    vec2 matUV = materialUV(mat, uv, textureSize(samplerImages[nonuniformEXT(mat.albedoIdx)], 0));
    vec3 albedo = textureLod(samplerImages[nonuniformEXT(mat.albedoIdx)], matUV, 0.0).rgb * mat.albedoFactor.rgb;
    vec3 emissive = textureLod(samplerImages[nonuniformEXT(mat.emissiveIdx)], matUV, 0.0).rgb * mat.emissiveFactor;

    vec3 hitPos = origin + dir * hitT;
    vec3 L = normalize(camera.sunDirection.xyz);
    float NdotL = max(dot(N, L), 0.0);
    vec3 sunIrradiance = vec3(0.0);
    if (NdotL > 0.0) {
        sunIrradiance = camera.sunColor.rgb * NdotL * traceVisibility(hitPos + N * 0.01, L, 10000.0);
    }
    vec3 localIrradiance = localLightIrradiance(hitPos + N * 0.01, N, pc.lightsRef, pc.lightCount);

    // bounce light from last frame's probes, the open sky outside the grid or without ddgi. getSkyIrradiance carries a 1 / PI
    vec3 ambientIrradiance = getSkyIrradiance(N) * PI * camera.skyIntensity;
    if (camera.ddgiReady != 0) {
        DDGIVolume volume = DDGIVolume(camera.ddgiGridOrigin.xyz, camera.ddgiSpacing, camera.ddgiProbeCounts.xyz, camera.ddgiNormalBias);
        vec4 ddgi = ddgiSampleIrradiance(hitPos, N, -dir, volume, lastFrameIrradianceAtlas, lastFrameVisibilityAtlas);
        ambientIrradiance = mix(ambientIrradiance, ddgi.rgb * camera.ddgiIntensity, ddgi.a);
    }

    // same emissive boost as the screen space hits
    radiance = albedo * (sunIrradiance + localIrradiance + ambientIrradiance) / PI + emissive * 10.0;
    return true;
}
#endif


// Generate a random unsigned int from two unsigned int values, using 16 pairs
// of rounds of the Tiny Encryption Algorithm. See Zafar, Olano, and Curtis,
// "GPU Random Numbers via the Tiny Encryption Algorithm"
uint tea(uint val0, uint val1)
{
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

// Generate a random unsigned int in [0, 2^24) given the previous RNG state
// using the Numerical Recipes linear congruential generator
uint lcg(inout uint prev)
{
    uint LCG_A = 1664525u;
    uint LCG_C = 1013904223u;
    prev       = (LCG_A * prev + LCG_C);
    return prev & 0x00FFFFFF;
}

// Generate a random float in [0, 1) given the previous RNG state
float rnd(inout uint prev)
{
    return (float(lcg(prev)) / float(0x01000000));
}

vec3 toTangentSpace(vec3 v, vec3 N) {
    vec3 up = abs(N.z) < 0.999 ? vec3(0,0,1) : vec3(1,0,0);
    vec3 tangentX = normalize(cross(up, N));
    vec3 tangentY = cross(N, tangentX);

    return v.x * tangentX + v.y * tangentY + v.z * N;
}

vec3 SampleLobe(vec3 N, float r1, float r2) {
    float s = sqrt(1 - r1 * r1);
    vec3 k = vec3(s*cos(r2), s*sin(r2), r1);
    k = toTangentSpace(k, N);

    return k;
}


vec3 SampleBrdf(inout uint seed, in vec3 N) {
    float r1 = sqrt(rnd(seed));
    float r2 = float(2*PI*rnd(seed));
    vec3 Wi = SampleLobe(N, r1, r2);

    return normalize(Wi);
    return Wi;
}

float hash(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

vec3 sampleHemisphere(vec3 normal, vec2 noise) {
    float h1 = noise.x;
    float h2 = noise.y;
    
    float phi = 2.0 * PI * h1;
    float cosTheta = sqrt(1.0 - h2);
    float sinTheta = sqrt(h2);
    
    vec3 up = abs(normal.z) < 0.999 ? vec3(0,0,1) : vec3(1,0,0);
    vec3 tangent = normalize(cross(up, normal));
    vec3 bitangent = cross(normal, tangent);
    
    return tangent * (cos(phi) * sinTheta) + bitangent * (sin(phi) * sinTheta) + normal * cosTheta;
}


float RadicalInverse_VdC(uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

vec2 Hammersley(uint i, uint N) {
    return vec2(float(i)/float(N), RadicalInverse_VdC(i));
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

vec3 getPos(vec2 uv, float depth) {
    vec4 clip = vec4(uv * 2.0 - 1.0, depth, 1.0);
    vec4 view = camera.invProj * clip;
    return view.xyz / view.w;
}

// project view space position to UV
vec2 getUV(vec3 pos) {
    vec4 clip = camera.projection * vec4(pos, 1.0);
    vec3 ndc = clip.xyz / clip.w;
    return ndc.xy * 0.5 + 0.5;
}

// view space position to (uv, depth), depth is linear in screen space so the ray stays a straight line there
vec3 toScreen(vec3 pos) {
    vec4 clip = camera.projection * vec4(pos, 1.0);
    vec3 ndc = clip.xyz / clip.w;
    return vec3(ndc.xy * 0.5 + 0.5, ndc.z);
}

float linearDepth(float depth) {
    vec4 view = camera.invProj * vec4(0.0, 0.0, depth, 1.0);
    return view.z / view.w;
}

// hierarchical march over the min-depth pyramid: while the ray is in front of a cell's closest depth it skips
// the whole cell and climbs a level, otherwise it drops a level, so empty and occluded space costs a few steps
// instead of one step per pixel. origin and direction are (uv, depth)
vec3 hizTrace(vec3 origin, vec3 direction, out bool validHit) {
    int maxMip = textureQueryLevels(u_HiZBuffer) - 1;
    int mip = pc.mostDetailedMip;
    vec2 mipRes = vec2(textureSize(u_HiZBuffer, mip));

    vec3 invDir = vec3(
        direction.x != 0.0 ? 1.0 / direction.x : 1e30,
        direction.y != 0.0 ? 1.0 / direction.y : 1e30,
        direction.z != 0.0 ? 1.0 / direction.z : 1e30
    );
    // the cell boundary the ray exits through, nudged past it so floor() lands in the next cell
    vec2 floorOffset = vec2(direction.x < 0.0 ? 0.0 : 1.0, direction.y < 0.0 ? 0.0 : 1.0);
    vec2 uvOffset = 0.005 * exp2(float(pc.mostDetailedMip)) / vec2(textureSize(u_HiZBuffer, 0)) * sign(direction.xy);

    // step out of the starting cell first or the ray hits its own surface
    vec2 xyPlane = (floor(mipRes * origin.xy) + floorOffset) / mipRes + uvOffset;
    vec2 tStart = (xyPlane - origin.xy) * invDir.xy;
    float t = min(tStart.x, tStart.y);
    vec3 position = origin + t * direction;

    int i = 0;
    while (i < pc.maxSteps && mip >= pc.mostDetailedMip) {
        if (any(lessThan(position.xy, vec2(0.0))) || any(greaterThan(position.xy, vec2(1.0))) || position.z >= 1.0) {
            validHit = false;
            return position;
        }

        vec2 cell = floor(mipRes * position.xy);
        float surfaceZ = texelFetch(u_HiZBuffer, ivec2(cell), mip).r;

        vec3 planes = vec3((cell + floorOffset) / mipRes + uvOffset, surfaceZ);
        vec3 tPlanes = (planes - origin) * invDir;
        tPlanes.z = direction.z > 0.0 ? tPlanes.z : 1e30;  // a ray coming toward the camera never reaches the depth plane
        float tMin = min(min(tPlanes.x, tPlanes.y), tPlanes.z);

        bool aboveSurface = surfaceZ > position.z;
        bool skippedCell = tMin != tPlanes.z && aboveSurface;
        t = aboveSurface ? tMin : t;
        position = origin + t * direction;

        mip = skippedCell ? min(mip + 1, maxMip) : mip - 1;
        mipRes = vec2(textureSize(u_HiZBuffer, max(mip, 0)));
        i++;
    }

    validHit = i < pc.maxSteps;
    return position;
}

// visible normal sampling of the ggx distribution (Heitz 2018), ve is the view direction in tangent space (z = normal)
vec3 sampleGGXVNDF(vec3 ve, float alpha, vec2 u) {
    vec3 vh = normalize(vec3(alpha * ve.x, alpha * ve.y, ve.z));
    float lensq = vh.x * vh.x + vh.y * vh.y;
    vec3 t1 = lensq > 0.0 ? vec3(-vh.y, vh.x, 0.0) * inversesqrt(lensq) : vec3(1.0, 0.0, 0.0);
    vec3 t2 = cross(vh, t1);

    float r = sqrt(u.x);
    float phi = 2.0 * PI * u.y;
    float p1 = r * cos(phi);
    float p2 = r * sin(phi);
    float s = 0.5 * (1.0 + vh.z);
    p2 = (1.0 - s) * sqrt(1.0 - p1 * p1) + s * p2;

    vec3 nh = p1 * t1 + p2 * t2 + sqrt(max(0.0, 1.0 - p1 * p1 - p2 * p2)) * vh;
    return normalize(vec3(alpha * nh.x, alpha * nh.y, max(0.0, nh.z)));
}

// one ray from a view space start: the hi-z march on screen, then the tlas when it is enabled and the ray left the
// screen or went behind something. confidence is how much of the result is a real hit (screen edges fade it out),
// the combine pass fills the rest from the probes / environment, so a miss is never counted twice
float traceRay(vec3 startView, vec3 rayDir, out vec3 radiance) {
    radiance = vec3(0.0);

    vec3 endView = startView + rayDir * (0.1 * -startView.z);
    vec3 origin = toScreen(startView);
    vec3 direction = toScreen(endView) - origin;

    bool validHit = false;
    vec3 hitPos = hizTrace(origin, direction, validHit);
    vec2 hitUV = hitPos.xy;

    // the pyramid only says the ray reached the closest surface, it is a hit when it didn't pass behind it by more than the thickness
    if (validHit) {
        float surfaceDepth = textureLod(depthImage, hitUV, 0.0).r;
        float surfaceZ = linearDepth(surfaceDepth);
        float thickness = pc.thickness * (1.0 + 0.02 * -surfaceZ);  // far surfaces cover more depth per pixel
        vec3 hitView = getPos(hitUV, hitPos.z);
        bool inRange = length(hitView - startView) < pc.maxDistance;

        // the depth buffer only holds the side facing the camera: a ray reaching it from behind (behind a thin card
        // or wall) passed under its back face, which the screen doesn't have, so its front color would leak through.
        // g-buffer normals face the camera (back faces are flipped), a ray going the same way arrived from behind
        vec3 hitNormal = normalize(mat3(camera.view) * textureLod(u_GBufferNormals, hitUV, 0.0).xyz);
        bool frontFacing = dot(hitNormal, rayDir) < 0.0;

        if (surfaceDepth < 1.0 && abs(surfaceZ - linearDepth(hitPos.z)) < thickness && inRange && frontFacing) {
            // everything leaving the hit surface: last frame's final color (direct, emissive and its own ambient, so a
            // surface in shadow still returns the sky light it bounces), reprojected to where it was then.
            // this frame's direct light only where the point wasn't on screen last frame
            vec2 prevHitUV = hitUV - textureLod(u_MotionVectors, hitUV, 0.0).rg;
            bool prevOnScreen = all(greaterThanEqual(prevHitUV, vec2(0.0))) && all(lessThanEqual(prevHitUV, vec2(1.0)));
            radiance = prevOnScreen ? textureLod(u_PrevFinalColor, prevHitUV, 0.0).rgb : textureLod(u_SceneColor, hitUV, 0.0).rgb;
            radiance = (any(isnan(radiance)) || any(isinf(radiance))) ? vec3(0.0) : max(radiance, vec3(0.0));
            vec2 edge = smoothstep(vec2(0.0), vec2(0.05), hitUV) * smoothstep(vec2(1.0), vec2(0.95), hitUV);
            return edge.x * edge.y;
        }
    }

#ifdef SSR_RAY_QUERY
    if (pc.useTLAS != 0) {
        vec3 worldStart = (camera.invView * vec4(startView, 1.0)).xyz;
        vec3 worldDir = normalize(mat3(camera.invView) * rayDir);
        float hitT;
        if (traceScene(worldStart, worldDir, pc.maxDistance, radiance, hitT)) {
            return 1.0;
        }
    }
#endif
    return 0.0;
}

// one cosine weighted diffuse ray and one ggx specular ray per pixel. both outputs are the incoming radiance only,
// 2x2 order that alternates diagonals, nested once more for a 4x4 block so consecutive frames land far apart
ivec2 subPixelOffset(uint index, int scale) {
    const ivec2 order[4] = ivec2[](ivec2(0, 0), ivec2(1, 1), ivec2(1, 0), ivec2(0, 1));
    if (scale == 2) {
        return order[index & 3u];
    }
    if (scale == 4) {
        return order[index & 3u] * 2 + order[(index >> 2) & 3u];
    }
    return ivec2(0);
}

// with no albedo or fresnel (demodulated): texture detail stays out of the temporal history, which keeps it from
// smearing, and the combine pass multiplies the material back in.
// rgb = radiance * confidence, a = confidence
void main() {
    ivec2 pixelCoord = ivec2(gl_GlobalInvocationID.xy);
    if (pixelCoord.x >= pc.screenRes.x || pixelCoord.y >= pc.screenRes.y) return;

    // below full resolution each texel covers a block of render pixels, trace a different one of them every frame
    // so the history sees the real surface of each pixel instead of the depth / normal blended at the block center
    int scale = 1 << pc.mostDetailedMip;
    ivec2 renderRes = textureSize(u_GBufferNormals, 0);
    ivec2 renderPixel = min(pixelCoord * scale + subPixelOffset(pc.frameIndex, scale), renderRes - 1);
    vec2 uv = (vec2(renderPixel) + 0.5) / vec2(renderRes);
    float depth = texture(depthImage, uv).r;

    if (depth >= 1.0 || depth <= 0.00001) {
        imageStore(u_ResultImage, pixelCoord, vec4(0.0));
        imageStore(u_SpecularImage, pixelCoord, vec4(0.0));
        return;
    }

    vec3 normalW = normalize(texture(u_GBufferNormals, uv).xyz);
    vec3 N = normalize(mat3(camera.view) * normalW);
    float roughness = max(texture(pbrImage, uv).g, 0.045);

    vec3 viewPos = getPos(uv, depth);
    vec3 V = -normalize(viewPos);
    if (dot(N, V) < 0.0) {
        N = normalize(N + V * (0.01 - dot(N, V)));     // normal mapped past the silhouette, same fix as the light pass
    }

    // pushed off the surface so the first cells don't report a hit on the surface itself
    float bias = 0.05 / max(clamp(dot(V, N), 0.0, 1.0), 0.1);
    vec3 startView = viewPos + N * bias;

    uint seed = tea(pixelCoord.y * uint(pc.screenRes.x) + pixelCoord.x, uint(pc.frameSeed));

    // diffuse: cosine weighted, the cos / pi of the lambert brdf cancels with the pdf, so the sample is just the radiance
    vec3 diffuseDir = sampleHemisphere(N, vec2(rnd(seed), rnd(seed)));
    vec3 diffuseRadiance = vec3(0.0);
    float diffuseConfidence = 0.0;
    if (pc.reflectionOnly == 0) {
        diffuseConfidence = traceRay(startView, diffuseDir, diffuseRadiance);   // 0 confidence: combine uses ddgi / sky
    }

    // specular: visible normals of the ggx lobe, the reflected direction is distributed like the brdf, so the
    // denoised average is the radiance over the lobe the combine pass weights with the split sum (like the prefiltered env)
    vec3 T = normalize(abs(N.y) < 0.999 ? cross(N, vec3(0.0, 1.0, 0.0)) : cross(N, vec3(1.0, 0.0, 0.0)));
    vec3 B = cross(N, T);
    vec3 Ve = vec3(dot(V, T), dot(V, B), dot(V, N));
    vec3 Hl = sampleGGXVNDF(Ve, roughness * roughness, vec2(rnd(seed), rnd(seed)));
    vec3 H = normalize(T * Hl.x + B * Hl.y + N * Hl.z);
    vec3 specularDir = reflect(-V, H);
    vec3 specularRadiance = vec3(0.0);
    float specularConfidence = 0.0;
    if (dot(specularDir, N) > 0.0) {
        specularConfidence = traceRay(startView, specularDir, specularRadiance);
    }

    // a single very bright hit (the sun on a light surface) would sit in the history as a firefly for many frames
    float MAX_SAMPLE_LUMA = pc.maxSampleLuminance;
    float diffuseLuma = dot(diffuseRadiance, vec3(0.2126, 0.7152, 0.0722));
    diffuseRadiance *= diffuseLuma > MAX_SAMPLE_LUMA ? MAX_SAMPLE_LUMA / diffuseLuma : 1.0;
    float specularLuma = dot(specularRadiance, vec3(0.2126, 0.7152, 0.0722));
    specularRadiance *= specularLuma > MAX_SAMPLE_LUMA ? MAX_SAMPLE_LUMA / specularLuma : 1.0;

    vec4 diffuseOut = vec4(diffuseRadiance * diffuseConfidence, diffuseConfidence);
    vec4 specularOut = vec4(specularRadiance * specularConfidence, specularConfidence);

    // one bad value would stay in the temporal history
    if (any(isnan(diffuseOut)) || any(isinf(diffuseOut))) {
        diffuseOut = vec4(0.0);
    }
    if (any(isnan(specularOut)) || any(isinf(specularOut))) {
        specularOut = vec4(0.0);
    }

    imageStore(u_ResultImage, pixelCoord, diffuseOut);
    imageStore(u_SpecularImage, pixelCoord, specularOut);
}
