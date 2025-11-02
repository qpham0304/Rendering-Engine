#version 460 core

out vec4 FragColor;

in vec2 uv;
in vec3 normal;
in vec3 updatedPos;
in mat4 lightSpaceVP;

layout(std140, binding = 0) uniform Vec3Buffer {
	vec3 lc;
};

uniform vec3 lightPos;
uniform vec3 camPos;
uniform sampler2D diffuseMap;
uniform sampler2D specularMap;
uniform sampler2D depthMap;

uniform int SCREEN_WIDTH;
uniform int SCREEN_HEIGHT;
uniform int NUM_STEPS_INT = 15;
uniform float G = 0.7f; // Controls how much light will scatter in the forward direction
uniform float intensity = 1.0f;
uniform float scatterScale = 1.0f;
uniform vec3 lightColor;
uniform float time;
uniform vec3 windDirection;

const float PI = 3.14159265359f;
const mat4 DITHER_PATTERN = mat4
    (vec4(0.0f, 0.5f, 0.125f, 0.625f),
     vec4(0.75f, 0.22f, 0.875f, 0.375f),
     vec4(0.1875f, 0.6875f, 0.0625f, 0.5625f),
     vec4(0.9375f, 0.4375f, 0.8125f, 0.3125f));
	 
// Henyey-Greenstein phase function
float mieScattering(float cosTheta) {
	// float numerator = (1.0f - G*G);
	// float denominator =  (4.0f * PI * pow(1.0f + G * G - 2.0f * G * cosTheta, 1.5f));
	// return numerator / denominator;
	float result = 1.0f - G * G;
	result /= (4.0f * PI * pow(1.0f + G * G - (2.0f * G) * cosTheta, 1.5f));
	return result;
}


float rand(vec3 p) 
{
    return fract(sin(dot(p, vec3(12.345, 67.89, 412.12))) * 42123.45) * 2.0 - 1.0;
}

float valueNoise(vec3 p) 
{
    vec3 u = floor(p);
    vec3 v = fract(p);
    vec3 s = smoothstep(0.0, 1.0, v);
    
    float a = rand(u);
    float b = rand(u + vec3(1.0, 0.0, 0.0));
    float c = rand(u + vec3(0.0, 1.0, 0.0));
    float d = rand(u + vec3(1.0, 1.0, 0.0));
    float e = rand(u + vec3(0.0, 0.0, 1.0));
    float f = rand(u + vec3(1.0, 0.0, 1.0));
    float g = rand(u + vec3(0.0, 1.0, 1.0));
    float h = rand(u + vec3(1.0, 1.0, 1.0));
    
    return mix(mix(mix(a, b, s.x), mix(c, d, s.x), s.y),
               mix(mix(e, f, s.x), mix(g, h, s.x), s.y),
               s.z);
}


float fbm(vec3 p) 
{
    vec3 q = p;
    int numOctaves = 8;
    float weight = 0.5;
    float ret = 0.0;
    
    // fbm
    for (int i = 0; i < numOctaves; i++)
    {
        ret += weight * valueNoise(q); 
        q *= 2.0;
        weight *= 0.5;
    }
    return clamp(ret, 0.0, 1.0);
}

vec3 volumetricMarch()
{
    float fogVolume = 0.0;
    vec4 color = vec4(0.0, 0.0, 0.0, 0.0);
    
    for (int i = 0; i < 150; i++)
    {
        // vec3 p = ro + fogVolume * rd;
		vec3 p = vec3(fogVolume);
        float density = fbm(p);
        
        // If density is unignorable...
        if (density > 1e-3)
        {
            // We estimate the color with w.r.t. density
            vec4 c = vec4(mix(vec3(1.0, 1.0, 1.0), vec3(0.0, 0.0, 0.0), density), density);
            // Multiply it by a factor so that it becomes softer
            c.a *= 0.4;
            c.rgb *= c.a;
            color += c * (1.0 - color.a);
        }
        
        // March forward a fixed distance
        fogVolume += max(0.05, 0.02 * fogVolume);
    }
    
    return clamp(color.rgb, 0.0, 1.0);
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

float calcShadow() {
	vec4 fragPosLight = lightSpaceVP * vec4(updatedPos, 1.0f);
	vec3 projCoords = fragPosLight.xyz / fragPosLight.w;
	projCoords = projCoords * 0.5 + 0.5;
	float closestDepth = texture(depthMap, projCoords.xy).r;   
	float currentDepth = projCoords.z;  
	
	float bias = max(0.05 * (1.0 - dot(normal, normalize(lightPos - updatedPos))), 0.005); 
	
	float shadow = 0.0;
	// if(currentDepth > 1.0f)
	// 	return shadow;
	//shadow = (currentDepth > closestDepth + bias ) ? 1.0 : 0.0; 
	
	// sample area for soft shadow
	vec2 pixelSize = 1.0 / textureSize(depthMap, 0);
	for(int y = -2; y <= 2; y++)
	{
		for(int x = -2; x <= 2; x++)
		{
		    float closestDepth = texture(depthMap, projCoords.xy + vec2(x, y) * pixelSize).r;
			if (currentDepth > closestDepth + bias)
				shadow += 1.0f;     
		}    
	}
	// Get average shadow
	shadow /= pow((2 * 2 + 1), 2);

	return shadow;
}

void main() {
    vec4  ambient = texture(diffuseMap, uv) * 0.5;

    vec3 lightDir = normalize(lightPos - updatedPos);
    
    // vec3 directionLight = vec3(-1.0f, -1.0f, -1.0f);
    // lightDir = normalize(-directionLight);    // for directional light
    vec3 viewDir = normalize(camPos - updatedPos);
    float distance = length(lightPos - updatedPos);

    float lightConstant = 1.0f;
    float lightLinear = 0.09f;
    float lightQuadratic = 0.032f;
    float attenuation = 1.0 / (lightConstant + lightLinear * distance + lightQuadratic * (distance * distance));
	attenuation *= intensity;

    float NdotL = max(0.0, dot(normal, lightDir));
    vec3 diffuse = NdotL * texture(diffuseMap, uv).rgb;

    vec3 halfwayDir = normalize(viewDir + lightDir);
    float NdotR = max(0.0, dot(normal, halfwayDir));
    vec3 specular = pow(NdotR, 32) * texture(specularMap, uv).rgb;

	vec3 L = lightDir;
	vec3 V = updatedPos - camPos;	//t1 - t0
	const float NUM_STEPS = float(NUM_STEPS_INT);
	float stepSize = length(V) / NUM_STEPS;
	V = normalize(V);
	vec3 step = V * stepSize;	//direction x step size dx
	vec3 position = camPos;
	position += step * DITHER_PATTERN[int(uv.x * SCREEN_WIDTH) % 4][int(uv.y * SCREEN_HEIGHT) % 4];
	vec3 volume = vec3(0.0f);
	
	vec4 color = vec4(0.0, 0.0, 0.0, 0.0);
	for(int i = 0; i < NUM_STEPS_INT; ++i) {
		vec4 fragPosLight = lightSpaceVP * vec4(position, 1.0f);
		vec3 projCoords = fragPosLight.xyz / fragPosLight.w * 0.5f + 0.5f;	// the uv position at every step

		float depth = texture(depthMap, projCoords.xy).r;
		// float bias = max(0.05 * (1.0 - dot(normal, normalize(lightPos - updatedPos))), 0.005); 
		
		float beersLaw = logDepth(projCoords.z, 0.2f, 0.5f);
		float density = fbm(position);	// TODO: THIS IS REALLY SLOW, find better approach when have time
		if (density > 1e-3) {	// If density is unignorable...
			// We estimate the color with w.r.t. density
			vec4 c = vec4(mix(vec3(1.0, 1.0, 1.0), vec3(0.0, 0.0, 0.0), density), density);
			// Multiply it by a factor so that it becomes softer
			c.a *= 0.4;
			c.rgb *= c.a;
			color += c * (1.0 - color.a) * mieScattering(dot(V, -L)) * (scatterScale * 0.5);
		}


		if(depth > projCoords.z){
		/*
			vec3 lightRayStep = lightPos - position;
			int numLightSteps = int(length(lightRayStep) / stepSize);  // Calculate number of steps for light ray
			lightRayStep /= float(numLightSteps);  // Normalize the light ray step
			float lightTransmittance = 1.0;
			vec3 lightRayPos = position;

			// March along the light ray
			for (int j = 0; j < numLightSteps; ++j) {
				vec4 lightPosLightSpace = lightSpaceVP * vec4(lightRayPos, 1.0f);
				vec3 lightProjCoords = lightPosLightSpace.xyz / lightPosLightSpace.w * 0.5f + 0.5f;
				
				// Sample the depth along the light ray
				float lightDepth = texture(depthMap, lightProjCoords.xy).r;
				if (lightDepth <= lightProjCoords.z) {
					// Calculate the attenuation for this segment
					float segmentAttenuation = exp(-scatterScale * stepSize);
					lightTransmittance *= segmentAttenuation;
				}
				lightRayPos += lightRayStep;
			}
		*/
			volume += mieScattering(dot(V, -L)) * scatterScale * lightColor;
		}
		position += step;
	}
	volume /= NUM_STEPS;
	vec3 cl = clamp(color.rgb, 0.0, 1.0);
    float shadow = calcShadow();
	FragColor = vec4(vec3(ambient) + (1.0f - shadow) * (diffuse + specular) + cl + volume, 1.0f) * attenuation;
	FragColor = FragColor / (FragColor + vec4(1.0));	//tone mapping
}