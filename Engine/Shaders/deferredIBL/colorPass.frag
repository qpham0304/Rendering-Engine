#version 450 core

struct Light {
    vec3 position;
	vec3 color;
	int radius;
    int type;
};

const int MAX_NUM_LIGHTS = 100;
const float PI = 3.14159265359;

const float groundRadiusMM = 6.360;     // Units are in megameters.
const float atmosphereRadiusMM = 6.460;
vec3 viewPos = vec3(0.0, groundRadiusMM + 0.0002, 0.0); // 200M above the ground.

const vec2 tLUTRes = vec2(256.0, 64.0);
// Doubled the vertical skyLUT res from the paper, looks way better for sunrise.
const vec2 skyLUTRes = vec2(200.0, 200.0) * 4.0;

uniform vec2 iChannelResolution;

uniform sampler2D skyLUT;


out vec4 FragColor;

in vec2 uv;

uniform sampler2D gDepth;
uniform sampler2D gNormal;
uniform sampler2D gAlbedo;
uniform sampler2D gMetalRoughness;
uniform sampler2D gEmissive;
uniform sampler2D gDUV;
uniform samplerCube irradianceMap;
uniform samplerCube prefilterMap;
uniform sampler2D brdfLUT;
uniform sampler2D ssaoTex;
uniform sampler2D sunDepthMap;
uniform sampler2D atmosphereMap;

uniform Light lights[MAX_NUM_LIGHTS];
uniform mat4 inverseView;
uniform mat4 invProjection;
uniform bool gamma;
uniform mat4 viewMatrix;
uniform bool ssaoOn = false;
uniform mat4 sunlightMV;
uniform vec3 sunPos;
uniform int sampleRadius;
uniform float time;                 // Time variable for animation
uniform float rippleStrength;

//------------------------//
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
} 

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a      = roughness*roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
	
    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
	
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return num / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);
	
    return ggx1 * ggx2;
}


vec3 getViewSpacePosition(float z) {
    vec2 posCanonical  = uv * 2.0 - 1.0; //position in Canonical View Volume
	vec4 posView = invProjection * vec4(posCanonical, z , 1.0);
	posView /= posView.w;
	return posView.xyz;
}

float linearizeDepth(float depth) {
	float near = 0.1f;
	float far = 100.0f;
    float z = depth * 2.0 - 1.0;
    return (2.0 * near * far) / (far + near - z * (far - near));
}

float logDepth(float depth, float steepness, float offset) {
	float zVal = linearizeDepth(depth);
	return (1 / (1 + exp(-steepness * (zVal - offset))));
}

float getSunAltitude(float time) {
    const float periodSec = 60.0;
    const float halfPeriod = periodSec / 2.0;
    const float sunriseShift = 0.1;
    float cyclePoint = (1.0 - abs((mod(time,periodSec)-halfPeriod)/halfPeriod));
    cyclePoint = (cyclePoint*(1.0+sunriseShift))-sunriseShift;
    return PI * cyclePoint;
}

vec3 getSunDir(float time) {
    float altitude = getSunAltitude(time);
    return normalize(vec3(0.0, sin(altitude), -cos(altitude)));
}

float calcShadow(vec3 fragPos, vec3 normal) {
    vec4 fragPosLight = sunlightMV * vec4(fragPos, 1.0);
	vec3 projCoords = fragPosLight.xyz / fragPosLight.w;
	projCoords = projCoords * 0.5 + 0.5;
	float closestDepth = texture(sunDepthMap, projCoords.xy).r;   
	float currentDepth = projCoords.z;  
	float bias = max(0.05 * (1.0 - dot(normal, normalize(sunPos - fragPos))), 0.005); 
	
	float shadow = 0.0;
	//shadow = (currentDepth > closestDepth + bias ) ? 1.0 : 0.0; 
	
	// sample area for soft shadow
	vec2 pixelSize = 1.0 / textureSize(sunDepthMap, 0);
	for(int y = -sampleRadius; y <= sampleRadius; y++) {
		for(int x = -sampleRadius; x <= sampleRadius; x++) {
		    float closestDepth = texture(sunDepthMap, projCoords.xy + vec2(x, y) * pixelSize).r;
			if (currentDepth > closestDepth + bias) {
				shadow += 1.0f;     
            }
		}    
	}
	// Get average shadow
	shadow /= pow((sampleRadius * 2 + 1), 2);

	return shadow;
}

float safeacos(const float x) {
    return acos(clamp(x, -1.0, 1.0));
}

vec3 getValFromSkyLUT(vec3 rayDir, vec3 sunDir) {
    float height = length(viewPos);
    vec3 up = viewPos / height; // Normalized up direction
    
    float horizonAngle = safeacos(sqrt(height * height - groundRadiusMM * groundRadiusMM) / height);
    float altitudeAngle = horizonAngle - acos(dot(rayDir, up)); // Between -PI/2 and PI/2
    float azimuthAngle; // Between 0 and 2*PI

    if (abs(altitudeAngle) > (0.5*PI - .0001)) {
        // Looking nearly straight up or down.
        azimuthAngle = 0.0;
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
    uv /= iChannelResolution.xy;
    
    return texture(skyLUT, uv).rgb;
}

// Functions for SH basis evaluation
vec3 evalSH(vec3 normal, vec3 shCoeffs[9]) {
    // Predefined spherical harmonic basis functions for 3rd-order SH
    float x = normal.x, y = normal.y, z = normal.z;

    return shCoeffs[0] +              // L00
           shCoeffs[1] * y +          // L1-1
           shCoeffs[2] * z +          // L10
           shCoeffs[3] * x +          // L11
           shCoeffs[4] * x * y +      // L2-2
           shCoeffs[5] * y * z +      // L2-1
           shCoeffs[6] * (3.0 * z * z - 1.0) * 0.5 + // L20
           shCoeffs[7] * x * z +      // L21
           shCoeffs[8] * (x * x - y * y); // L22
}

vec4 calcLighting() {
    float geometryDepthValue = texture(gDepth, uv).r;

    vec3 normal = texture(gNormal, uv).rgb;         // view space normal
    vec3 albedo = pow(texture(gAlbedo, uv).rgb, vec3(2.2));
    // vec3 metalRoughness = texture(gMetalRoughness, uv).rgb;
    // float metallic = metalRoughness.b;
    // float roughness = metalRoughness.g;
    float ao = texture(gAlbedo, uv).a;
	vec3 emissive = texture(gEmissive, uv).rgb;
    vec3 dudv = texture(gDUV, uv).rgb * 2.0 - 1.0;
    float SSAO = texture(ssaoTex, uv).r;
    
    vec3 metalRoughness = texture(gMetalRoughness, uv).rgb;
    // metalRoughness += dudv;
    float metallic = metalRoughness.b;
    float roughness = metalRoughness.g;

	float depth = texture(gDepth, uv).r * 2.0 - 1.0;
    // depth = linearizeDepth(depth);
    // depth = logDepth(depth, 0.5, 5.0);
	vec3 viewSpacePosition = getViewSpacePosition(depth);
    vec3 view = normalize(-viewSpacePosition);
    vec3 worldSpacePosition = mat3(inverseView) * viewSpacePosition;

    vec3 N = mat3(inverseView) * normal;;
    vec3 V = normalize(-worldSpacePosition);
    vec3 R = reflect(-V, N); 
	
	vec3 F0 = vec3(0.04);	//0.04 realistic for most dielectric surfaces
	F0 = mix(F0, albedo, metallic);
	
	vec3 Lo = vec3(0.0);
	for(int i = 0; i < 0; i++) {
        vec3 lightPosViewSpace = vec3(viewMatrix * vec4(lights[i].position, 1.0));
        
		vec3 L = normalize(lightPosViewSpace - viewSpacePosition);
		float distance = length(lightPosViewSpace - viewSpacePosition);
		vec3 H = normalize(view + L);

		float attenuation = 1.0 / (distance * distance);
		vec3 radiance = lights[i].color;
        radiance *= attenuation;
		
		vec3 F  = fresnelSchlick(max(dot(H, view), 0.0), F0);
        vec3 kS = F;
		vec3 kD = 1.0 - kS;
		kD *= 1.0 - metallic;	
        
		float NDF = DistributionGGX(normal, H, roughness);       
		float G   = GeometrySmith(normal, view, L, roughness);

		vec3 numerator    = NDF * G * F;
		float denominator = 4.0 * max(dot(normal, view), 0.0) * max(dot(normal, L), 0.0)  + 0.0001; // 0.0001 to prevent zero division
		vec3 specular     = numerator / denominator; 

		float NdotL = max(dot(normal, L), 0.0);
		Lo += (kD * albedo / PI + specular) * radiance * NdotL;

        // vec3 diffuse = max(dot(N, L), 0.0) * albedo * lights[i].color;
        // float spec = pow(max(dot(N, H), 0.0), 16.0);
        // vec3 specular = lights[i].color * spec * roughness;
        // // attenuation
        
		// float attenuation = 1.0 / (distance * distance);
		// vec3 radiance = lights[i].color;
        // radiance *= attenuation;

        // Lo += (diffuse + specular) * attenuation;

	}

    vec3 F = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);
	vec3 kS = F;
    vec3 kD = 1.0 - kS;
    kD *= 1.0 - metallic;	  

    vec2 localUV = uv * 2.0 - 1.0;
    vec4 target = invProjection * vec4(localUV.x, localUV.y, 1.0 ,1.0);
    vec3 rayDir = vec3(inverseView * vec4(normalize(target.xyz/target.w), 0.0));
    float theta = acos(rayDir.y);  // Latitude: angle from the zenith (up axis)
    float phi = atan(rayDir.z, rayDir.x);  // Longitude: angle around the xy-plane
    float u = phi / (2.0 * 3.14159265359);  // Normalize phi to [0, 1]
    float v = theta / 3.14159265359;  // Normalize theta to [0, 1]
    vec4 atmosphereAmbient = texture(atmosphereMap, uv);

    // Retrieve SH coefficients from the texture
    vec3 shCoeffs[9]; // SH coefficients for this fragment
    for (int i = 0; i < 9; i++) {
        shCoeffs[i] = texelFetch(atmosphereMap, ivec2(uv), 0).rgb;
    }

    // Evaluate the SH lighting using the normal
    vec3 ambientLight = evalSH(N, shCoeffs);


    vec3 irradiance = texture(irradianceMap, N).rgb;
    irradiance = mix(irradiance, ambientLight, 0.5);
    vec3 diffuse = irradiance * albedo;

    // sample both the pre-filter map and the BRDF lut and combine them together as per the Split-Sum approximation to get the IBL specular part.
    vec2 brdf  = texture(brdfLUT, vec2(max(dot(N, V), 0.0), roughness)).rg;
    const float MAX_REFLECTION_LOD = 4.0;
    vec3 prefilteredColor = textureLod(prefilterMap, R,  roughness * MAX_REFLECTION_LOD).rgb;    
    vec3 color;

    // skybox or background where there's no geometry to be exact
    if (geometryDepthValue == 1.0) {
        color = texture(prefilterMap, R).rgb;
        color = mix(color, atmosphereAmbient.rgb, 0.5);
        return vec4(color, 1.0);
        // color = atmosphereAmbient;
        // return atmosphereAmbient;
    }

    else {
        vec3 specular = prefilteredColor * (F * brdf.x + brdf.y);
	    vec3 ambient = (kD * diffuse + specular) * ao;
	
	    color = ambient + Lo + emissive; 

        int ssaoCondition = int(ssaoOn);
        color = ssaoCondition * SSAO * color + (1 - ssaoCondition) * color;
    }
    
	color = color / (color + vec3(1.0));					// HDR tone mapping
	color = gamma ? pow(color, vec3(1.0 / 2.2)) : color;		// Gamma correction
	
    float shadow = calcShadow(worldSpacePosition, N);

	return vec4(color, 1.0f);
}

void main() {
    vec4 lighting = calcLighting();
    FragColor = lighting;
}