#ifndef DDGI_GLSL
#define DDGI_GLSL

// Shared by ddgiProbeTrace / ddgiUpdateIrradiance / ddgiUpdateVisibility / deferredCombine
// so the grid indexing and the atlas layout only live in one place.
//
// Grid:  probe (x, y, z) sits at gridOrigin + vec3(x, y, z) * gridSpacing
//        probeIdx = x + z * Cx + y * Cx * Cz     (C = probeCounts, same order as SandBoxLayer::createLightProbes)
//
// Atlas: one tile per probe, Cx tiles per row and Cz * Cy rows -> tile = (probeIdx % Cx, probeIdx / Cx)
//        a tile is the octahedral map of the probe (RES x RES) plus a 1 texel border on every side,
//        the border repeats the opposite edge so bilinear filtering never bleeds into the next probe
//
// Units: the irradiance atlas stores sum(radiance * cos) / sum(cos), which is irradiance / PI

const int DDGI_IRRADIANCE_RES = 8;      // must match DDGIBuilderVulkan::IRRADIANCE_RES
const int DDGI_VISIBILITY_RES = 16;     // must match DDGIBuilderVulkan::VISIBILITY_RES
const int DDGI_RAYS_PER_PROBE = 64;     // must match DDGIBuilderVulkan::raysPerProbe and local_size_x of ddgiProbeTrace
const float DDGI_MISS_DISTANCE = 10000.0;
const float DDGI_PI = 3.14159265359;

struct DDGIVolume {
    vec3 origin;
    float spacing;
    ivec3 probeCounts;
    float normalBias;
};

vec2 ddgiSignNotZero(vec2 v) {
    return vec2(v.x >= 0.0 ? 1.0 : -1.0, v.y >= 0.0 ? 1.0 : -1.0);
}

// unit direction -> [-1, 1]^2
vec2 ddgiOctEncode(vec3 v) {
    float l1norm = abs(v.x) + abs(v.y) + abs(v.z);
    vec2 result = v.xy * (1.0 / max(l1norm, 1e-6));
    if (v.z < 0.0) {
        result = (1.0 - abs(result.yx)) * ddgiSignNotZero(result);
    }
    return result;
}

// [-1, 1]^2 -> unit direction
vec3 ddgiOctDecode(vec2 o) {
    vec3 v = vec3(o.x, o.y, 1.0 - abs(o.x) - abs(o.y));
    if (v.z < 0.0) {
        v.xy = (1.0 - abs(v.yx)) * ddgiSignNotZero(v.xy);
    }
    return normalize(v);
}

vec3 ddgiSphericalFibonacci(float i, float n) {
    const float PHI = sqrt(5.0) * 0.5 + 0.5;
    float phi = 2.0 * DDGI_PI * fract(i * (PHI - 1.0));
    float cosTheta = 1.0 - (2.0 * i + 1.0) / n;
    float sinTheta = sqrt(max(0.0, 1.0 - cosTheta * cosTheta));
    return vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
}

ivec3 ddgiProbeCoord(int probeIdx, ivec3 counts) {
    return ivec3(probeIdx % counts.x, probeIdx / (counts.x * counts.z), (probeIdx / counts.x) % counts.z);
}

int ddgiProbeIndex(ivec3 coord, ivec3 counts) {
    return coord.x + coord.z * counts.x + coord.y * counts.x * counts.z;
}

ivec2 ddgiAtlasSize(int res, ivec3 counts) {
    return ivec2(counts.x, counts.y * counts.z) * (res + 2);
}

vec3 ddgiProbePosition(ivec3 coord, vec3 origin, float spacing) {
    return origin + vec3(coord) * spacing;
}

// texel inside a (res + 2) tile -> the direction it stores
// border texels return the direction of the interior texel they duplicate
vec3 ddgiTexelDirection(ivec2 tileTexel, int res) {
    ivec2 p = tileTexel - 1;    // interior coords, -1 or res on the border
    bool borderX = p.x < 0 || p.x >= res;
    bool borderY = p.y < 0 || p.y >= res;
    ivec2 clamped = clamp(p, ivec2(0), ivec2(res - 1));

    if (borderX && borderY) {
        p = (res - 1) - clamped;                            // corner: diagonally opposite corner
    } else if (borderX) {
        p = ivec2(clamped.x, (res - 1) - p.y);              // left / right column: mirrored along y
    } else if (borderY) {
        p = ivec2((res - 1) - p.x, clamped.y);              // top / bottom row: mirrored along x
    }

    vec2 oct = (vec2(p) + 0.5) / float(res) * 2.0 - 1.0;
    return ddgiOctDecode(oct);
}

vec2 ddgiAtlasUV(int probeIdx, vec3 direction, int res, ivec3 counts) {
    int stride = res + 2;
    vec2 oct = ddgiOctEncode(normalize(direction)) * 0.5 + 0.5;
    ivec2 tile = ivec2(probeIdx % counts.x, probeIdx / counts.x);
    vec2 texel = vec2(tile * stride) + 1.0 + oct * float(res);
    return texel / vec2(ddgiAtlasSize(res, counts));
}

// worldPos / N: the surface being lit, V: from the surface toward whoever is looking at it
// rgb: irradiance arriving at the surface  ->  diffuse = albedo / PI * rgb
// a:   how much the grid covers this point (1 inside, fades to 0 over one cell outside)
vec4 ddgiSampleIrradiance(
    vec3 worldPos, vec3 N, vec3 V,
    DDGIVolume volume,
    sampler2D irradianceAtlas, sampler2D visibilityAtlas
) {
    ivec3 counts = volume.probeCounts;
    vec3 maxCoord = vec3(counts - 1);

    vec3 gridPos = (worldPos - volume.origin) / volume.spacing;
    vec3 outside = max(max(-gridPos, gridPos - maxCoord), vec3(0.0));
    float coverage = clamp(1.0 - max(outside.x, max(outside.y, outside.z)), 0.0, 1.0);
    if (coverage <= 0.0) {
        return vec4(0.0);
    }

    // push the sample point off the surface so it does not sit exactly on the shadow boundary of the probes
    vec3 biasedPos = worldPos + (N * 0.2 + V * 0.8) * (0.75 * volume.spacing) * volume.normalBias;
    vec3 biasedGridPos = clamp((biasedPos - volume.origin) / volume.spacing, vec3(0.0), maxCoord);

    ivec3 baseCoord = clamp(ivec3(floor(biasedGridPos)), ivec3(0), max(counts - 2, ivec3(0)));
    vec3 alpha = clamp(biasedGridPos - vec3(baseCoord), vec3(0.0), vec3(1.0));

    vec3 sumIrradiance = vec3(0.0);
    float sumWeight = 0.0;

    for (int i = 0; i < 8; i++) {
        ivec3 offset = ivec3(i & 1, (i >> 1) & 1, (i >> 2) & 1);
        ivec3 coord = clamp(baseCoord + offset, ivec3(0), counts - 1);
        int probeIdx = ddgiProbeIndex(coord, counts);
        vec3 probePos = ddgiProbePosition(coord, volume.origin, volume.spacing);

        // probes behind the surface should not light it, wrapped so the falloff stays smooth
        vec3 dirToProbe = normalize(probePos - worldPos);
        float wrap = (dot(dirToProbe, N) + 1.0) * 0.5;
        float weight = wrap * wrap + 0.2;

        // chebyshev visibility test: is there geometry between the probe and the surface?
        vec3 probeToPoint = biasedPos - probePos;
        float dist = length(probeToPoint);
        vec2 visUV = ddgiAtlasUV(probeIdx, probeToPoint, DDGI_VISIBILITY_RES, counts);
        vec2 moments = textureLod(visibilityAtlas, visUV, 0.0).rg;
        if (dist > moments.x) {
            float variance = abs(moments.x * moments.x - moments.y);
            float d = dist - moments.x;
            float chebyshev = variance / (variance + d * d);
            weight *= max(chebyshev * chebyshev * chebyshev, 0.0);
        }

        // keep a tiny weight so a fully occluded cell still blends instead of dividing by zero
        weight = max(0.000001, weight);

        // crush small weights so a barely visible probe does not leak light
        const float crushThreshold = 0.2;
        if (weight < crushThreshold) {
            weight *= weight * weight / (crushThreshold * crushThreshold);
        }

        vec3 trilinear = max(mix(1.0 - alpha, alpha, vec3(offset)), vec3(0.001));
        weight *= trilinear.x * trilinear.y * trilinear.z;

        vec2 irradianceUV = ddgiAtlasUV(probeIdx, N, DDGI_IRRADIANCE_RES, counts);
        sumIrradiance += textureLod(irradianceAtlas, irradianceUV, 0.0).rgb * weight;
        sumWeight += weight;
    }

    return vec4(sumIrradiance / sumWeight * DDGI_PI, coverage);
}

#endif
