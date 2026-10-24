#ifndef ATMOSPHERE_GLSL
#define ATMOSPHERE_GLSL

// Hillaire 2020, "A Scalable and Production Ready Sky and Atmosphere Rendering Technique".
// distances in km, the planet center is the origin and +y is up at the viewer

const float ATMOSPHERE_PI = 3.14159265359;

layout(push_constant) uniform AtmosphereParams {
    vec4 rayleighScattering;    // rgb: per km at the ground, w: density scale height (km)
    vec4 mie;                   // x: scattering per km, y: extinction per km, z: density scale height (km), w: phase g
    vec4 ozoneAbsorption;       // rgb: per km at the peak, w: peak altitude (km)
    vec4 groundAlbedo;          // rgb, w: ozone layer half width (km)
    vec4 sunDirection;          // xyz: toward the sun
    vec4 sunIlluminance;        // rgb: sun color * intensity * sky intensity
    float bottomRadius;
    float topRadius;
    float viewHeight;           // km above the ground the environment is captured from
    float padding;
} atmosphere;

struct MediumSample {
    vec3 rayleighScattering;
    float mieScattering;
    vec3 scattering;
    vec3 extinction;
};

MediumSample sampleMedium(float height) {
    float rayleighDensity = exp(-height / atmosphere.rayleighScattering.w);
    float mieDensity = exp(-height / atmosphere.mie.z);
    float ozoneDensity = max(0.0, 1.0 - abs(height - atmosphere.ozoneAbsorption.w) / atmosphere.groundAlbedo.w);

    MediumSample medium;
    medium.rayleighScattering = atmosphere.rayleighScattering.rgb * rayleighDensity;
    medium.mieScattering = atmosphere.mie.x * mieDensity;
    medium.scattering = medium.rayleighScattering + vec3(medium.mieScattering);
    medium.extinction = medium.rayleighScattering + vec3(atmosphere.mie.y * mieDensity) + atmosphere.ozoneAbsorption.rgb * ozoneDensity;
    return medium;
}

// nearest positive hit with a sphere at the origin, -1 when there is none
float raySphere(vec3 origin, vec3 dir, float radius) {
    float b = dot(origin, dir);
    float c = dot(origin, origin) - radius * radius;
    float discriminant = b * b - c;
    if (discriminant < 0.0) {
        return -1.0;
    }
    float s = sqrt(discriminant);
    float t0 = -b - s;
    float t1 = -b + s;
    if (t0 > 0.0) {
        return t0;
    }
    return t1 > 0.0 ? t1 : -1.0;
}

float rayleighPhase(float cosTheta) {
    return 3.0 / (16.0 * ATMOSPHERE_PI) * (1.0 + cosTheta * cosTheta);
}

// Cornette-Shanks, what the paper uses for mie
float miePhase(float cosTheta, float g) {
    float g2 = g * g;
    float k = 3.0 / (8.0 * ATMOSPHERE_PI) * (1.0 - g2) / (2.0 + g2);
    return k * (1.0 + cosTheta * cosTheta) / pow(max(1.0 + g2 - 2.0 * g * cosTheta, 1e-4), 1.5);
}

// the transmittance and multiple scattering luts: u = cos zenith from -1 to 1, v = radius from ground to top
vec2 atmosphereLutUV(float radius, float cosZenith) {
    float height = (radius - atmosphere.bottomRadius) / (atmosphere.topRadius - atmosphere.bottomRadius);
    return vec2(clamp(cosZenith * 0.5 + 0.5, 0.0, 1.0), clamp(height, 0.0, 1.0));
}

void atmosphereLutParams(vec2 uv, out float radius, out float cosZenith) {
    cosZenith = uv.x * 2.0 - 1.0;
    radius = mix(atmosphere.bottomRadius, atmosphere.topRadius, uv.y);
}

// lut texel to a position and a direction in the plane of the zenith
void atmosphereLutRay(vec2 uv, out vec3 pos, out vec3 dir) {
    float radius;
    float cosZenith;
    atmosphereLutParams(uv, radius, cosZenith);
    pos = vec3(0.0, radius, 0.0);
    dir = vec3(sqrt(max(0.0, 1.0 - cosZenith * cosZenith)), cosZenith, 0.0);
}

vec3 sampleAtmosphereLut(sampler2D lut, vec3 pos, vec3 dir) {
    float radius = length(pos);
    return textureLod(lut, atmosphereLutUV(radius, dot(pos / radius, dir)), 0.0).rgb;
}

#endif
