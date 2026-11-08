#ifndef RESTIR_GLSL
#define RESTIR_GLSL

// restir gi (ouyang et al. 2021) for the diffuse ssrgi ray: a reservoir keeps one sample out of everything it has
// seen (this frame's ray, last frame's reservoir, neighbors), picked in proportion to how much it lights the pixel.
// world space. stored in four rgba32f images:
//   0: visible point xyz, M          1: sample point xyz, W
//   2: radiance rgb, confidence      3: octahedral visible normal xy, sample normal xy
// confidence 0 is a miss (no hit to reuse), M 0 is an empty reservoir (sky)
struct Reservoir {
    vec3 visiblePos;
    vec3 visibleNormal;
    vec3 samplePos;
    vec3 sampleNormal;
    vec3 radiance;
    float confidence;
    float wSum;
    float M;
    float W;
};

const float RESTIR_PI = 3.14159265359;

vec2 octWrap(vec2 v) {
    return (1.0 - abs(v.yx)) * vec2(v.x >= 0.0 ? 1.0 : -1.0, v.y >= 0.0 ? 1.0 : -1.0);
}

vec2 octEncode(vec3 n) {
    n /= abs(n.x) + abs(n.y) + abs(n.z);
    vec2 e = n.z >= 0.0 ? n.xy : octWrap(n.xy);
    return e;
}

vec3 octDecode(vec2 e) {
    vec3 n = vec3(e, 1.0 - abs(e.x) - abs(e.y));
    if (n.z < 0.0) {
        n.xy = octWrap(n.xy);
    }
    return normalize(n);
}

vec4 packReservoir0(Reservoir r) { return vec4(r.visiblePos, r.M); }
vec4 packReservoir1(Reservoir r) { return vec4(r.samplePos, r.W); }
vec4 packReservoir2(Reservoir r) { return vec4(r.radiance, r.confidence); }
vec4 packReservoir3(Reservoir r) { return vec4(octEncode(r.visibleNormal), octEncode(r.sampleNormal)); }

Reservoir unpackReservoir(vec4 t0, vec4 t1, vec4 t2, vec4 t3) {
    Reservoir r;
    r.visiblePos = t0.xyz;
    r.M = t0.w;
    r.samplePos = t1.xyz;
    r.W = t1.w;
    r.radiance = t2.rgb;
    r.confidence = t2.a;
    r.visibleNormal = octDecode(t3.xy);
    r.sampleNormal = octDecode(t3.zw);
    r.wSum = 0.0;
    return r;
}

Reservoir emptyReservoir(vec3 visiblePos, vec3 visibleNormal) {
    Reservoir r;
    r.visiblePos = visiblePos;
    r.visibleNormal = visibleNormal;
    r.samplePos = visiblePos;
    r.sampleNormal = visibleNormal;
    r.radiance = vec3(0.0);
    r.confidence = 0.0;
    r.wSum = 0.0;
    r.M = 0.0;
    r.W = 0.0;
    return r;
}

// what the pixel at (pos, normal) wants: bright hits along its normal. the confidence keeps dark hits pickable,
// they still say the hemisphere is blocked there, so the combine pass doesn't fill them with sky
float restirTarget(vec3 radiance, float confidence, vec3 samplePos, vec3 pos, vec3 normal) {
    if (confidence <= 0.0) {
        return 0.0;
    }
    vec3 toSample = samplePos - pos;
    float len = length(toSample);
    float cosTheta = len > 1e-4 ? max(dot(normal, toSample / len), 0.0) : 0.0;
    return confidence * (dot(radiance, vec3(0.2126, 0.7152, 0.0722)) + 0.01) * cosTheta;
}

// streaming weighted reservoir update, the sample replaces the kept one with probability w / wSum
bool updateReservoir(inout Reservoir r, Reservoir candidate, float w, float M, float u) {
    r.wSum += w;
    r.M += M;
    if (w > 0.0 && u * r.wSum < w) {
        r.samplePos = candidate.samplePos;
        r.sampleNormal = candidate.sampleNormal;
        r.radiance = candidate.radiance;
        r.confidence = candidate.confidence;
        return true;
    }
    return false;
}

void finalizeReservoir(inout Reservoir r) {
    float target = restirTarget(r.radiance, r.confidence, r.samplePos, r.visiblePos, r.visibleNormal);
    r.W = (target > 0.0 && r.M > 0.0) ? r.wSum / (r.M * target) : 0.0;
}

// the cosine weighted estimate the plain ssrgi wrote (rgb = radiance * confidence, a = confidence), from the kept sample
vec4 resolveReservoir(Reservoir r) {
    if (r.W <= 0.0) {
        return vec4(0.0);
    }
    vec3 toSample = r.samplePos - r.visiblePos;
    float len = length(toSample);
    float cosTheta = len > 1e-4 ? max(dot(r.visibleNormal, toSample / len), 0.0) : 0.0;
    float weight = cosTheta / RESTIR_PI * r.W;
    return vec4(r.radiance * r.confidence, r.confidence) * weight;
}

#endif
