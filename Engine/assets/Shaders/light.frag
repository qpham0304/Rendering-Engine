#version 460 core

in vec2 uv;
in vec3 normal; 

out vec4 FragColor;

uniform vec3 lightColor;
uniform samplerCube irradianceMap;


void main()
{
	vec4 irradiance = texture(irradianceMap, normal);
	FragColor = mix(vec4(lightColor, 1.0f), irradiance, 0.2);
}
