#version 460 core

in vec2 TexCoords;
uniform sampler2D skyLUT;
uniform sampler2D transmittanceLUT;
uniform sampler2D multipleScatteredLUT;
uniform vec2 LUTResolution;
uniform float iTime;
uniform vec3 camPos;
// uniform mat4 viewMatrix;
// uniform mat4 projectionMatrix;
uniform mat4 invView;
uniform mat4 invProjection;

const float PI = 3.14159265358;
const float groundRadiusMM = 6.360;     // Units are in megameters.
const float atmosphereRadiusMM = 6.460;
vec3 viewPos = vec3(0.0, groundRadiusMM + 0.05, 0.0); // 200M above the ground.

const vec2 tLUTRes = vec2(256.0, 64.0);
const vec2 msLUTRes = vec2(32.0, 32.0);
// Doubled the vertical skyLUT res from the paper, looks way better for sunrise.
const vec2 skyLUTRes = vec2(200.0, 200.0) * 4.0;

const vec3 rayleighScatteringBase = vec3(5.802, 13.558, 33.1);
const float rayleighAbsorptionBase = 0.0;

const float mieScatteringBase = 3.996;
const float mieAbsorptionBase = 4.4;

const vec3 ozoneAbsorptionBase = vec3(0.650, 1.881, .085);
const int numScatteringSteps = 64;

// animate sun movement
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

vec3 getValFromSkyLUT(vec3 rayDir, vec3 sunDir) {
    float height = length(viewPos);
    vec3 up = viewPos / height;
    
    float horizonAngle = safeacos(sqrt(height * height - groundRadiusMM * groundRadiusMM) / height);
    float altitudeAngle = horizonAngle - acos(dot(rayDir, up)); // Between -PI/2 and PI/2
    float azimuthAngle; // Between 0 and 2*PI
    if (abs(altitudeAngle) > (0.5*PI - .0001)) {
        azimuthAngle = 0.0; // Looking nearly straight up or down.
    } else {
        vec3 right = cross(sunDir, up);
        vec3 forward = cross(up, right);
        
        vec3 projectedDir = normalize(rayDir - up*(dot(rayDir, up)));
        float sinTheta = dot(projectedDir, right);
        float cosTheta = dot(projectedDir, forward);
        azimuthAngle = atan(sinTheta, cosTheta) + PI;
    }
    
    // Non-linear mapping of altitude angle. See Section 5.3 of the paper.
    float v = 0.5 + 0.5*sign(altitudeAngle)*sqrt(abs(altitudeAngle)*2.0/PI);
    vec2 uv = vec2(azimuthAngle / (2.0*PI), v);
    uv *= skyLUTRes;
    uv /= LUTResolution.xy;
    
    return texture(skyLUT, uv).rgb;
}

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

vec3 getValFromTLUT(vec2 bufferRes, vec3 sunDir, vec3 rayDir) {
    float height = length(viewPos);
    vec3 up = viewPos / height;
	// float sunCosZenithAngle = dot(sunDir, up);
    // vec2 uv = vec2(tLUTRes.x*clamp(0.5 + 0.5*sunCosZenithAngle, 0.0, 1.0),
    //                tLUTRes.y*max(0.0, min(1.0, (height - groundRadiusMM)/(atmosphereRadiusMM - groundRadiusMM))));
    // uv /= bufferRes;
    // vec3 res = texture(skyLUT, uv).rgb;
    // return vec3(1.0, 1.0, 1.0);


    // Angle between the sun and the local "up" vector (zenith angle).
    float sunCosZenithAngle = dot(sunDir, up);
    // Fake transmission factor: decreases as the sun nears the horizon.
    float transmission = smoothstep(0.0, 0.1, sunCosZenithAngle);
    // Adjust brightness near the horizon to simulate reddening/scattering.
    float horizonFactor = smoothstep(-0.1, 0.5, dot(rayDir, up)); 
    transmission *= horizonFactor;
    return vec3(transmission);
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

        vec3 sunTransmittance = getValFromTLUT(transmittanceLUT, LUTResolution.xy, newPos, sunDir);
        vec3 psiMS = getValFromMultiScattLUT(multipleScatteredLUT, LUTResolution.xy, newPos, sunDir);
        
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

vec3 jodieReinhardTonemap(vec3 c) {
    // From: https://www.shadertoy.com/view/tdSXzD
    float l = dot(c, vec3(0.2126, 0.7152, 0.0722));
    vec3 tc = c / (c + 1.0);
    return mix(c / (l + 1.0), tc, tc);
}

vec3 sunWithBloom(vec3 rayDir, vec3 sunDir) {
    const float sunSolidAngle = 0.53*PI/180.0;
    const float minSunCosTheta = cos(sunSolidAngle);

    float cosTheta = dot(rayDir, sunDir);
    if (cosTheta >= minSunCosTheta) return vec3(1.0);
    
    float offset = minSunCosTheta - cosTheta;
    float gaussianBloom = exp(-offset*50000.0)*0.5;
    float invBloom = 1.0/(0.02 + offset*300.0)*0.01;
    return vec3(gaussianBloom+invBloom);
}

out vec4 FragColor;
void main()
{
    vec2 fragCoord = gl_FragCoord.xy;
    vec3 sd = vec3(1.0, 0.01, 0.5);
    // sd = getSunDir(iTime);
    vec3 sunDir = sd;
    
    float camFOVWidth = PI/3.5;
    float camWidthScale = 2.0*tan(camFOVWidth/2.0);
    float camHeightScale = camWidthScale*LUTResolution.y/LUTResolution.x;
    
    // vec3 camDir = normalize(vec3(0.0, 0.27, -1.0));
    // vec3 camRight = normalize(cross(camDir, vec3(0.0, 1.0, 0.0)));
    // vec3 camUp = normalize(cross(camRight, camDir));
    
    vec2 xy = 2.0 * (fragCoord.xy / LUTResolution.xy) - 1.0;
    // vec3 rayDir = normalize(camDir + camRight*xy.x*camWidthScale + camUp*xy.y*camHeightScale);

    // vec4 rayClip = vec4(xy, -1.0, 1.0);
    // vec4 rayEye = invProjection * rayClip;
    // rayEye = vec4(rayEye.xy, -1.0, 0.0);
    // vec4 rayWorld = invView * rayEye;
    // vec3 rayDir = normalize(rayWorld.xyz);

    vec2 localUV = xy;
    vec4 target = invProjection * vec4(localUV.x, localUV.y, 1.0 ,1.0);
    vec3 rayDir = vec3(invView * vec4(normalize(target.xyz/target.w), 0.0));

    vec3 lum;

    if (length(viewPos) < atmosphereRadiusMM * 1.0){
        lum = getValFromSkyLUT(rayDir, sunDir);
    } else {    // use direct raymarch instead of textureLUT if view outside atmosphere radius
        float atmoDist = rayIntersectSphere(viewPos, rayDir, atmosphereRadiusMM);
        float groundDist = rayIntersectSphere(viewPos, rayDir, groundRadiusMM);
        float tMax = (groundDist < 0.0) ? atmoDist : groundDist;
        lum = raymarchScattering(viewPos, rayDir, sunDir, tMax, float(numScatteringSteps));
        // lum += vec3(1e-3,0.0,0.0); // for debug purpose
    }

    // Bloom should be added at the end, but this is subtle and works well.
    vec3 sunLum = sunWithBloom(rayDir, sunDir);
    // Use smoothstep to limit the effect, so it drops off to actual zero.
    sunLum = smoothstep(0.002, 1.0, sunLum);
    if (length(sunLum) > 0.0) {
        if (rayIntersectSphere(viewPos, rayDir, groundRadiusMM) >= 0.0) {
            sunLum *= 0.0;
        } else {
            // If the sun value is applied to this pixel
            // we need to calculate the transmittance to obscure it.
            sunLum *= getValFromTLUT(LUTResolution.xy, sunDir, rayDir);
        }
    }
    lum += sunLum;
    
    // Tonemapping and gamma. Super ad-hoc, probably a better way to do this.
    lum *= 20.0;
    lum = pow(lum, vec3(1.3));
    lum /= (smoothstep(0.0, 0.2, clamp(sunDir.y, 0.0, 1.0))*2.0 + 0.15);
    lum = jodieReinhardTonemap(lum);
    lum = pow(lum, vec3(1.0/2.2));
    
    FragColor = vec4(lum,1.0);
}