#version 460 core

struct Material {
    sampler2D diffuse;
    sampler2D specular;    
    float shininess;

	//vec3 albedo;
	//float metallic;
	//float roughness;
	//float ao;
}; 

struct Light {
    vec3 position;
	vec4 color;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
	int sampleRadius;
};

uniform bool useTexture = false;
uniform bool enableFog = false;
uniform sampler2D albedoMap;
uniform sampler2D normalMap;
uniform sampler2D metallicMap;
uniform sampler2D roughnessMap;
uniform sampler2D aoMap;
uniform sampler2D emissiveMap;
uniform samplerCube irradianceMap;
uniform samplerCube prefilterMap;
uniform sampler2D brdfLUT;

uniform sampler2D specular0;
uniform sampler2D shadowMap;

uniform Material material;
uniform Light light;
vec4 lightColor = light.color;

const int MAX_NUM_LIGHTS = 4;
const float PI = 3.14159265359;
uniform vec3 lightPositions[MAX_NUM_LIGHTS];
uniform vec3 lightColors[MAX_NUM_LIGHTS];
uniform bool gamma;
uniform bool hasEmission = false;

in VS_OUT {
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
} frag_in;

vec3 updatedPos = frag_in.updatedPos;
vec4 fragPosLight = frag_in.fragPosLight;
vec3 camPos = frag_in.camPos;

out vec4 FragColor;


vec4 toonShader() {
	float intensity;
	vec4 color;
	vec3 lightDir = normalize(frag_in.lightPos - updatedPos);
	intensity = dot(lightDir,frag_in.normal);

	if (intensity > 0.95)
		color = vec4(0.5,0.5,0.5,1.0);
	else if (intensity > 0.5)
		color = vec4(0.3,0.3,0.3,1.0);
	else if (intensity > 0.25)
		color = vec4(0.2,0.2,0.2,1.0);
	else
		color = vec4(0.1,0.1,0.1,1.0);
	//#FFE5D8FF skin color
	return color;
}

float calcShadow() {
	vec3 projCoords = fragPosLight.xyz / fragPosLight.w;
	projCoords = projCoords * 0.5 + 0.5;
	float closestDepth = texture(shadowMap, projCoords.xy).r;   
	float currentDepth = projCoords.z;  
	float bias = max(0.05 * (1.0 - dot(frag_in.normal, normalize(frag_in.lightPos - updatedPos))), 0.005); 
	
	float shadow = 0.0;
	//shadow = (currentDepth > closestDepth + bias ) ? 1.0 : 0.0; 
	
	// sample area for soft shadow
	vec2 pixelSize = 1.0 / textureSize(shadowMap, 0);
	for(int y = -light.sampleRadius; y <= light.sampleRadius; y++)
	{
		for(int x = -light.sampleRadius; x <= light.sampleRadius; x++)
		{
		    float closestDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * pixelSize).r;
			if (currentDepth > closestDepth + bias)
				shadow += 1.0f;     
		}    
	}
	// Get average shadow
	shadow /= pow((light.sampleRadius * 2 + 1), 2);

	return shadow;
}

//------------------------//
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
} 

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a      = roughness*roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
	
    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
	
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return num / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);
	
    return ggx1 * ggx2;
}

vec3 getNormalFromMap()
{
    vec3 tangentNormal = texture(normalMap, frag_in.uv).xyz * 2.0 - 1.0;

    vec3 Q1  = dFdx(frag_in.updatedPos);
    vec3 Q2  = dFdy(frag_in.updatedPos);
    vec2 st1 = dFdx(frag_in.uv);
    vec2 st2 = dFdy(frag_in.uv);

    vec3 N   = normalize(frag_in.normal);
    vec3 T  = normalize(Q1*st2.t - Q2*st1.t);
    vec3 B  = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * tangentNormal);
}

vec4 pointLightPBR() {
    vec3 albedo = pow(texture(albedoMap, frag_in.uv).rgb, vec3(2.2));
    float metallic = texture(metallicMap, frag_in.uv).b;
    float roughness = texture(roughnessMap, frag_in.uv).g;
    float ao = texture(aoMap, frag_in.uv).r;
	vec3 emissive = vec3(0.0f);
	if(hasEmission) // check if there is emissive material through another uniform
		emissive = texture(emissiveMap, frag_in.uv).rgb;

    vec3 N = getNormalFromMap();
    vec3 V = normalize(camPos - frag_in.updatedPos);
    vec3 R = reflect(-V, N); 
	
	vec3 F0 = vec3(0.04);	//0.04 realistic for most dielectric surfaces
	F0 = mix(F0, albedo, metallic);
	
	vec3 Lo = vec3(0.0);
	for(int i = 0; i < MAX_NUM_LIGHTS; i++) {
		vec3 L = normalize(lightPositions[i] - frag_in.fragPos);
		vec3 H = normalize(V + L);

		float distance = length(lightPositions[i] - frag_in.fragPos);
		float attenuation = 1.0 / (distance * distance);
		vec3 radiance = lightColors[i] * attenuation;
		
		//BRDF
		vec3 F  = fresnelSchlick(max(dot(H, V), 0.0), F0);
		float NDF = DistributionGGX(N, H, roughness);       
		float G   = GeometrySmith(N, V, L, roughness);

		vec3 numerator    = NDF * G * F;
		float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0)  + 0.0001; // 0.0001 to prevent zero division
		vec3 specular     = numerator / denominator; 

		vec3 kS = F;
		vec3 kD = vec3(1.0) - kS;
		kD *= 1.0 - metallic;	

		float NdotL = max(dot(N, L), 0.0);
		Lo += (kD * albedo / PI + specular) * radiance * NdotL;
	}
	//vec3 ambient = vec3(0.03) * albedo * ao;
	//vec3 F = fresnelSchlick(max(dot(N, V), 0.0), F0);
    vec3 F = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);
	vec3 kS = F;
    vec3 kD = 1.0 - kS;
    kD *= 1.0 - metallic;	  

    vec3 irradiance = texture(irradianceMap, N).rgb;
    vec3 diffuse = irradiance * albedo;

    // sample both the pre-filter map and the BRDF lut and combine them together as per the Split-Sum approximation to get the IBL specular part.
    const float MAX_REFLECTION_LOD = 4.0;
    vec3 prefilteredColor = textureLod(prefilterMap, R,  roughness * MAX_REFLECTION_LOD).rgb;    
    vec2 brdf  = texture(brdfLUT, vec2(max(dot(N, V), 0.0), roughness)).rg;
    vec3 specular = prefilteredColor * (F * brdf.x + brdf.y);
	vec3 ambient = (kD * diffuse + specular) * ao;
	
	vec3 color = ambient + Lo + emissive; 
	color = color / (color + vec3(1.0));					// HDR tone mapping
	color = gamma ? pow(color, vec3(1.0/2.2)) : color;		// Gamma correction
	
	return vec4(color, 1.0f);
	// return texture(metallicMap, frag_in.uv);
}
//------------------------//
vec4 pointLight() {
    //vec3 normal = texture(normal0, frag_in.uv).rgb;
    //normal = normalize(normal * 2.0 - 1.0);  // this normal is in tangent space
	//normal = normal * 2.0 - 1.0;
	//normal = normalize(normal * frag_in.TBN);
	vec3 normal = frag_in.normal;

	// ambient
    vec3 ambient = light.ambient * vec3(texture(albedoMap, frag_in.uv));

	// diffuse
    vec3 lightDir = normalize(frag_in.lightPos - updatedPos);
    float diff = max(dot(lightDir, normal), 0.0);
    vec3 diffuse = light.diffuse * diff * vec3(texture(albedoMap, frag_in.uv));
    
	// specular
    vec3 viewDir = normalize(camPos - updatedPos);
    //vec3 reflectDir = reflect(-lightDir, normal);
    vec3 halfwayDir = normalize(lightDir + viewDir);  
    float spec = pow(max(dot(normal, halfwayDir), 0.0), material.shininess);
    vec3 specular = light.specular * spec * vec3(texture(specular0, frag_in.uv));

	// calculate shadow
    float shadow = calcShadow();

	//TODO: add attenuation for light fall off
	diffuse *= (1.0 - shadow);
	specular *= (1.0 - shadow);

    vec3 lighting = (ambient + diffuse + specular) * lightColor.xyz; 
	return vec4(lighting, 1.0f);
}

vec4 spotLight(vec3 lightDirection) {
	float outerCone = 0.90f;
	float innerCone = 0.95f;

	// ambient
    vec3 ambient = light.ambient * vec3(texture(albedoMap, frag_in.uv));

	// diffuse
    vec3 lightDir = normalize(frag_in.lightPos - updatedPos);
    float diff = max(dot(lightDir, frag_in.normal), 0.0);
    vec3 diffuse = light.diffuse * diff * vec3(texture(albedoMap, frag_in.uv));
    
	// specular
    vec3 viewDir = normalize(camPos - updatedPos);
    //vec3 reflectDir = reflect(-lightDir, normal);
    vec3 halfwayDir = normalize(lightDir + viewDir);  
    float spec = pow(max(dot(frag_in.normal, halfwayDir), 0.0), material.shininess);
    vec3 specular = light.specular * spec * vec3(texture(specular0, frag_in.uv));

	float theta = dot(lightDir, normalize(-lightDirection));
	float epsilon = innerCone - outerCone;
	float intensity = clamp((theta - outerCone) / epsilon, 0.0, 1.0); 

	// calculate shadow
    float shadow = calcShadow();

	//TODO: add attenuation for light fall off
	diffuse *= intensity * (1.0 - shadow);
	specular *= intensity * (1.0 - shadow);

    vec3 lighting = (ambient + diffuse + specular) * lightColor.xyz; 
	return vec4(lighting, 1.0f);
}

vec4 directionalLight(vec3 lightDirection) {
	vec3 normal = normalize(frag_in.normal);

	// ambient
    vec3 ambient = light.ambient * vec3(texture(albedoMap, frag_in.uv));

	// diffuse
    vec3 lightDir = normalize(-lightDirection);
    float diff = max(dot(lightDir, normal), 0.0);
    vec3 diffuse = light.diffuse * diff * vec3(texture(albedoMap, frag_in.uv));
    
	// specular
    vec3 viewDir = normalize(camPos - updatedPos);
    vec3 halfwayDir = normalize(lightDir + viewDir);  
    float spec = pow(max(dot(normal, halfwayDir), 0.0), material.shininess);
    vec3 specular = light.specular * spec * vec3(texture(specular0, frag_in.uv));

	// calculate shadow
    float shadow = calcShadow();
	diffuse *= (1.0 - shadow);
	specular *= (1.0 - shadow);
    
	vec3 lighting = (ambient + diffuse + specular) * lightColor.xyz;    
	return vec4(lighting, 1.0f);
}

float linearizeDepth(float depth) {
	float near = 0.1f;
	float far = 100.0f;
    float z = depth * 2.0 - 1.0;
    return (2.0 * near * far) / (far + near - z * (far - near));
}

float logDepth(float depth, float steepness = 0.5f, float offset = 5.0f) {
	float zVal = linearizeDepth(depth);
	return (1 / (1 + exp(-steepness * (zVal - offset))));
}

void main()
{
	// calculate shadow
    float shadow = calcShadow();    
	// toon shading option
	vec4 toonShading = toonShader();
    // fog effect
    float depth = logDepth(gl_FragCoord.z, 0.2f);
    vec4 depthVal = vec4(depth * vec3(0.90f, 0.90f, 0.90f), 1.0f);
	
	//vec3 lighting = pointLight().xyz;
	//vec3 lighting = directionalLight(vec3(-1.0f, -1.0f, -1.0f)).xyz;
	//vec3 lighting = spotLight(vec3(0.0f, -1.0f, 0.0f)).xyz;
	vec3 lighting = pointLightPBR().xyz;

	vec4 res = mix(vec4(lighting, 1.0), (1.0 - depth) * vec4(lighting, 1.0) + depthVal, enableFog);
	FragColor = res;
}
