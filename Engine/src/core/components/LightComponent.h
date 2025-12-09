#pragma once

#include <glm/glm.hpp>

struct Light {
	glm::vec3 position;
	glm::vec4 color;
	glm::vec3 ambient;
	glm::vec3 diffuse;
	glm::vec3 specular;
	glm::mat4 mvp;
	int sampleRadius;

	Light(
		const glm::vec3& position,
		const glm::vec4& color,
		const glm::vec3& ambient = glm::vec3(0.2f, 0.2f, 0.2f),
		const glm::vec3& diffuse = glm::vec3(0.5f, 0.5f, 0.5f),
		const glm::vec3& specular = glm::vec3(1.0f, 1.0f, 1.0f),
		const glm::mat4& mvp = glm::mat4(0.0f),
		const int sampleRadius = 2)
	{
		this->position = position;
		this->color = color;
		this->ambient = ambient;
		this->diffuse = diffuse;
		this->specular = specular;
		this->mvp = mvp;
		this->sampleRadius = sampleRadius;
	};
};

class LightComponent
{

};

