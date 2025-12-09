#version 450 core

in vec2 TexCoords;
uniform sampler2D transmittanceLUT;
uniform sampler2D multipleScatteredLUT;
uniform vec2 iChannelResolution0;
uniform vec2 iChannelResolution1;
uniform float iTime;
uniform vec3 camPos;
uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;

const float PI = 3.14159265358;
const float groundRadiusMM = 6.360;     // Units are in megameters.
const float atmosphereRadiusMM = 6.460;
vec3 viewPos = vec3(0.0, groundRadiusMM + 0.0002, 0.0); // 200M above the ground.

const vec2 tLUTRes = vec2(256.0, 64.0);
const vec2 msLUTRes = vec2(32.0, 32.0);
// Doubled the vertical skyLUT res from the paper, looks way better for sunrise.
const vec2 skyLUTRes = vec2(200.0, 200.0) * 4.0;

// These are per megameter.
const vec3 rayleighScatteringBase = vec3(5.802, 13.558, 33.1);
const float rayleighAbsorptionBase = 0.0;

const float mieScatteringBase = 3.996;
const float mieAbsorptionBase = 4.4;

const vec3 ozoneAbsorptionBase = vec3(0.650, 1.881, .085);
const int numScatteringSteps = 64;

/*
 * Animates the sun movement.
 */
float getSunAltitude(float time) {
    const float periodSec = 30.0;
    const float halfPeriod = periodSec / 2.0;
    const float sunriseShift = 0.1;
    float cyclePoint = (1.0 - abs((mod(time,periodSec)-halfPeriod)/halfPeriod));     // abs for half cycle
    // float cyclePoint = (1.0 - (mod(time,periodSec)-halfPeriod)/halfPeriod);
    cyclePoint = (cyclePoint*(1.0+sunriseShift))-sunriseShift;
    return PI * cyclePoint;
}

vec3 getSunDir(float time) {
    float altitude = getSunAltitude(time);
    return normalize(vec3(0.0, sin(altitude), -cos(altitude)));
}

float getMiePhase(float cosTheta) {
    const float g = 0.8;
    const float scale = 3.0/(8.0*PI);
    
    float num = (1.0-g*g)*(1.0+cosTheta*cosTheta);
    float denom = (2.0+g*g)*pow((1.0 + g*g - 2.0*g*cosTheta), 1.5);
    
    return scale*num/denom;
}

float getRayleighPhase(float cosTheta) {
    const float k = 3.0/(16.0*PI);
    return k*(1.0+cosTheta*cosTheta);
}

void getScatteringValues(vec3 pos, out vec3 rayleighScattering, out float mieScattering, out vec3 extinction) {
    float altitudeKM = (length(pos)-groundRadiusMM)*1000.0;
    // Note: Paper gets these switched up.
    float rayleighDensity = exp(-altitudeKM/8.0);
    float mieDensity = exp(-altitudeKM/1.2);
    
    rayleighScattering = rayleighScatteringBase*rayleighDensity;
    float rayleighAbsorption = rayleighAbsorptionBase*rayleighDensity;
    
    mieScattering = mieScatteringBase*mieDensity;
    float mieAbsorption = mieAbsorptionBase*mieDensity;
    
    vec3 ozoneAbsorption = ozoneAbsorptionBase*max(0.0, 1.0 - abs(altitudeKM-25.0)/15.0);
    
    extinction = rayleighScattering + rayleighAbsorption + mieScattering + mieAbsorption + ozoneAbsorption;
}

float safeacos(const float x) {
    return acos(clamp(x, -1.0, 1.0));
}

// From https://gamedev.stackexchange.com/questions/96459/fast-ray-sphere-collision-code.
float rayIntersectSphere(vec3 ro, vec3 rd, float rad) {
    float b = dot(ro, rd);
    float c = dot(ro, ro) - rad*rad;
    if (c > 0.0f && b > 0.0) return -1.0;
    float discr = b*b - c;
    if (discr < 0.0) return -1.0;
    // Special case: inside sphere, use far discriminant
    if (discr > b*b) return (-b + sqrt(discr));
    return -b - sqrt(discr);
}

/*
 * Same parameterization here.
 */
vec3 getValFromTLUT(sampler2D tex, vec2 bufferRes, vec3 pos, vec3 sunDir) {
    float height = length(pos);
    vec3 up = pos / height;
	float sunCosZenithAngle = dot(sunDir, up);
    vec2 uv = vec2(tLUTRes.x*clamp(0.5 + 0.5*sunCosZenithAngle, 0.0, 1.0),
                   tLUTRes.y*max(0.0, min(1.0, (height - groundRadiusMM)/(atmosphereRadiusMM - groundRadiusMM))));
    uv /= bufferRes;
    return texture(tex, uv).rgb;
}
vec3 getValFromMultiScattLUT(sampler2D tex, vec2 bufferRes, vec3 pos, vec3 sunDir) {
    float height = length(pos);
    vec3 up = pos / height;
	float sunCosZenithAngle = dot(sunDir, up);
    vec2 uv = vec2(msLUTRes.x*clamp(0.5 + 0.5*sunCosZenithAngle, 0.0, 1.0),
                   msLUTRes.y*max(0.0, min(1.0, (height - groundRadiusMM)/(atmosphereRadiusMM - groundRadiusMM))));
    uv /= bufferRes;
    return texture(tex, uv).rgb;
}

// Buffer C calculates the actual sky-view! It's a lat-long map (or maybe altitude-azimuth is the better term),
// but the latitude/altitude is non-linear to get more resolution near the horizon.
vec3 raymarchScattering(vec3 pos, vec3 rayDir, vec3 sunDir, float tMax, float numSteps) {
    float cosTheta = dot(rayDir, sunDir);
    
	float miePhaseValue = getMiePhase(cosTheta);
	float rayleighPhaseValue = getRayleighPhase(-cosTheta);
    
    vec3 lum = vec3(0.0);
    vec3 transmittance = vec3(1.0);
    float t = 0.0;
    for (float i = 0.0; i < numSteps; i += 1.0) {
        float newT = ((i + 0.3)/numSteps)*tMax;
        float dt = newT - t;
        t = newT;
        
        vec3 newPos = pos + t*rayDir;
        
        vec3 rayleighScattering, extinction;
        float mieScattering;
        getScatteringValues(newPos, rayleighScattering, mieScattering, extinction);
        
        vec3 sampleTransmittance = exp(-dt*extinction);

        vec3 sunTransmittance = getValFromTLUT(transmittanceLUT, iChannelResolution0.xy, newPos, sunDir);
        vec3 psiMS = getValFromMultiScattLUT(multipleScatteredLUT, iChannelResolution1.xy, newPos, sunDir);
        
        vec3 rayleighInScattering = rayleighScattering*(rayleighPhaseValue*sunTransmittance + psiMS);
        vec3 mieInScattering = mieScattering*(miePhaseValue*sunTransmittance + psiMS);
        vec3 inScattering = (rayleighInScattering + mieInScattering);

        // Integrated scattering within path segment.
        vec3 scatteringIntegral = (inScattering - inScattering * sampleTransmittance) / extinction;

        lum += scatteringIntegral*transmittance;
        
        transmittance *= sampleTransmittance;
    }
    return lum;
}

out vec4 FragColor;
void main( )
{
    vec2 fragCoord = gl_FragCoord.xy;
    if (fragCoord.x >= (skyLUTRes.x+1.5) || fragCoord.y >= (skyLUTRes.y+1.5)) {
        return;
    }
    float u = clamp(fragCoord.x, 0.0, skyLUTRes.x-1.0)/skyLUTRes.x;
    float v = clamp(fragCoord.y, 0.0, skyLUTRes.y-1.0)/skyLUTRes.y;
    
    float azimuthAngle = (u - 0.5)*2.0*PI;
    // Non-linear mapping of altitude. See Section 5.3 of the paper.
    float adjV;
    if (v < 0.5) {
		float coord = 1.0 - 2.0*v;
		adjV = -coord*coord;
	} else {
		float coord = v*2.0 - 1.0;
		adjV = coord*coord;
	}
    
    float height = length(viewPos);
    vec3 up = viewPos / height;
    float horizonAngle = safeacos(sqrt(height * height - groundRadiusMM * groundRadiusMM) / height) - 0.5*PI;
    float altitudeAngle = adjV*0.5*PI - horizonAngle;
    
    float cosAltitude = cos(altitudeAngle);
    vec3 rayDir = vec3(cosAltitude*sin(azimuthAngle), sin(altitudeAngle), -cosAltitude*cos(azimuthAngle));

    vec3 sd = vec3(1.0, 0.01, 0.5);
    // sd = getSunDir(iTime);
    float sunAltitude = (0.5*PI) - acos(dot(sd, up));
    vec3 sunDir = vec3(0.0, sin(sunAltitude), -cos(sunAltitude));
    
    float atmoDist = rayIntersectSphere(viewPos, rayDir, atmosphereRadiusMM);
    float groundDist = rayIntersectSphere(viewPos, rayDir, groundRadiusMM);
    float tMax = (groundDist < 0.0) ? atmoDist : groundDist;
    vec3 lum = raymarchScattering(viewPos, rayDir, sunDir, tMax, float(numScatteringSteps));
    FragColor = vec4(lum, 1.0);
}