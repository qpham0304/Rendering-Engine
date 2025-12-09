#version 460 core

struct Material {
    sampler2D diffuse;
    sampler2D specular;    
    float shininess;
}; 

struct Light {
    vec3 position;
	vec4 color;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
	int sampleRadius;
};

in vec3 fragmentColor;
in vec2 uv;
in vec3 normal;
in vec3 updatedPos;
in vec4 fragPosLight;
in vec3 camPos;
in vec3 lightPos;
in mat3 TBN;
in vec3 fragPos;

uniform bool useTexture = false;
uniform bool enableFog = false;
uniform sampler2D diffuse0;
uniform sampler2D diffuse1;
uniform sampler2D shadowMap;
uniform sampler2D specular0;
uniform sampler2D normalMap0;
uniform float far_plane;
uniform samplerCube shadowMapPoint;

uniform Material material;
uniform Light light;
vec4 lightColor = light.color;
//vec3 lightPos = light.position;

out vec4 FragColor;

vec4 toonShader() {
	float intensity;
	vec4 color;
	vec3 lightDir = normalize(lightPos - updatedPos);
	intensity = dot(lightDir,normal);

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

vec3 gridSamplingDisk[20] = vec3[]
(
   vec3(1, 1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1, 1,  1), 
   vec3(1, 1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1, 1, -1),
   vec3(1, 1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0), vec3(-1, 1,  0),
   vec3(1, 0,  1), vec3(-1,  0,  1), vec3( 1,  0, -1), vec3(-1, 0, -1),
   vec3(0, 1,  1), vec3( 0, -1,  1), vec3( 0, -1, -1), vec3( 0, 1, -1)
);

float calcPointShadow(vec3 fragPos)
{
    // get vector between fragment position and light position
    vec3 fragToLight = fragPos - lightPos;
	float currentDepth = length(fragToLight);

    float shadow = 0.0;
    float bias = 0.15;
    int samples = 20;
    float viewDistance = length(camPos - fragPos);
    float diskRadius = (1.0 + (viewDistance / far_plane)) / 25.0;
    for(int i = 0; i < samples; ++i)
    {
        float closestDepth = texture(shadowMapPoint, fragToLight + gridSamplingDisk[i] * diskRadius).r;
        closestDepth *= far_plane;   // undo mapping [0;1]
        if(currentDepth - bias > closestDepth)
            shadow += 1.0;
    }
    shadow /= float(samples);
	return shadow;
} 

float calcShadow() {
	vec3 projCoords = fragPosLight.xyz / fragPosLight.w;
	projCoords = projCoords * 0.5 + 0.5;
	float closestDepth = texture(shadowMap, projCoords.xy).r;   
	float currentDepth = projCoords.z;  
	
	float bias = max(0.05 * (1.0 - dot(normal, normalize(lightPos - updatedPos))), 0.005); 
	
	float shadow = 0.0;
	if(currentDepth > 1.0f)
		return shadow;
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

vec4 pointLight() {
    //vec3 nm = texture(normalMap0, uv).rgb;
	//nm = nm * 2.0 - 1.0;
	//nm = normalize(nm * TBN);
	vec3 nm = normal;

	// ambient
    vec3 ambient = light.ambient * vec3(texture(diffuse0, uv));

	// diffuse
    vec3 lightDir = normalize(lightPos - fragPos);
    float diff = max(dot(lightDir, nm), 0.0);
    vec3 diffuse = light.diffuse * diff * vec3(texture(diffuse0, uv));
    
	// specular
    vec3 viewDir = normalize(camPos - fragPos);
    vec3 halfwayDir = normalize(lightDir + viewDir);  
    float spec = pow(max(dot(nm, halfwayDir), 0.0), material.shininess);
    vec3 specular = light.specular * spec * vec3(texture(specular0, uv));

	// calculate shadow
    //float shadow = calcShadow();
    float shadow = calcPointShadow(fragPos);

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
    vec3 ambient = light.ambient * vec3(texture(diffuse0, uv));

	// diffuse
    vec3 lightDir = normalize(lightPos - updatedPos);
    float diff = max(dot(lightDir, normal), 0.0);
    vec3 diffuse = light.diffuse * diff * vec3(texture(diffuse0, uv));
    
	// specular
    vec3 viewDir = normalize(camPos - updatedPos);
    //vec3 reflectDir = reflect(-lightDir, normal);
    vec3 halfwayDir = normalize(lightDir + viewDir);  
    float spec = pow(max(dot(normal, halfwayDir), 0.0), material.shininess);
    vec3 specular = light.specular * spec * vec3(texture(specular0, uv));

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
	vec3 normal = normalize(normal);

	// ambient
    vec3 ambient = light.ambient * vec3(texture(diffuse0, uv));

	// diffuse
    vec3 lightDir = normalize(-lightDirection);
    float diff = max(dot(lightDir, normal), 0.0);
    vec3 diffuse = light.diffuse * diff * vec3(texture(diffuse0, uv));
    
	// specular
    vec3 viewDir = normalize(camPos - updatedPos);
    vec3 halfwayDir = normalize(lightDir + viewDir);  
    float spec = pow(max(dot(normal, halfwayDir), 0.0), material.shininess);
    vec3 specular = light.specular * spec * vec3(texture(specular0, uv));

	// calculate shadow
    float shadow = calcShadow();
	diffuse *= (1.0 - shadow);
	specular *= (1.0 - shadow);
    
	vec3 lighting = (ambient + diffuse + specular) * lightColor.xyz;    
	return vec4(lighting, 1.0f);
}

float near = 0.1f;
float far = 100.0f;

float linearizeDepth(float depth) {
    float z = depth * 2.0 - 1.0;
    return (2.0 * near * far) / (far + near - z * (far - near));
}

float logDepth(float depth, float steepness, float offset) {
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
    float depth = logDepth(gl_FragCoord.z, 0.2f, 0.5);
    vec4 depthVal = vec4(depth * vec3(0.90f, 0.90f, 0.90f), 1.0f);

/*
	//vec3 color = useTexture ? texture(diffuse0, uv).rgb : vec3(0.5f, 0.5f, 0.5f);
    //vec3 texColor = mix(vec3(0.5f, 0.5f, 0.5f), texture(diffuse0, uv).rgb, useTexture);
    vec3 normal = normalize(normal);
    
	// ambient
    vec3 ambient = light.ambient * vec3(texture(diffuse0, uv));

	// diffuse
    vec3 lightDir = normalize(lightPos - updatedPos);
    float diff = max(dot(lightDir, normal), 0.0);
    vec3 diffuse = light.diffuse * diff * vec3(texture(diffuse0, uv));
    
	// specular
    vec3 viewDir = normalize(camPos - updatedPos);
    //vec3 reflectDir = reflect(-lightDir, normal);
    vec3 halfwayDir = normalize(lightDir + viewDir);  
    float spec = pow(max(dot(normal, halfwayDir), 0.0), material.shininess);
    vec3 specular = light.specular * spec * vec3(texture(specular0, uv));
    
	diffuse *= (1.0 - shadow);
	specular *= (1.0 - shadow);
    vec3 lighting = (ambient + diffuse + specular) * lightColor.xyz;   
*/
	
	vec3 lighting = pointLight().xyz;
	//vec3 lighting = directionalLight(vec3(-1.0f, -1.0f, -1.0f)).xyz;
	//vec3 lighting = spotLight(vec3(0.0f, -1.0f, 0.0f)).xyz;
	
	//vec4 res = enableFog ? (1.0 - depth) * vec4(lighting, 1.0) + depthVal : vec4(lighting, 1.0);
	
	vec4 res = mix(vec4(lighting, 1.0), (1.0 - depth) * vec4(lighting, 1.0) + depthVal, float(enableFog));
	FragColor = res;
}
