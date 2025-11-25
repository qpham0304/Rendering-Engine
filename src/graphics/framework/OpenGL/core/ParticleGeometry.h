#pragma once

#include "Shader.h"

#include <memory>

struct ParticleControl {
	glm::vec2 randomRange;
	glm::vec3 spawnArea; 
	float upperBound;
	float lowerBound;
	unsigned int numInstances;
	glm::vec3 size;


	ParticleControl(
		glm::vec2 randomRange,
		glm::vec3 spawnArea,
		float upperBound,
		float lowerBound,
		int numInstances,
		glm::vec3 size = glm::vec3(0.1))
		:	randomRange(randomRange),
			spawnArea(spawnArea),
			upperBound(upperBound),
			lowerBound(lowerBound),
			numInstances(numInstances),
			size(size)
	{

	}
};

class Camera;

class ParticleGeometry
{
private:
	//TODO: verify if array of structure is actually faster?
	std::vector<float> weights;
	std::vector<glm::vec3> flyDirections;
	std::vector<glm::vec3> randomDirs;

	glm::vec3 direction;
	glm::vec3 scale;
	glm::mat4 model;
	glm::mat4 translateMatrix;
	glm::mat4 scaleMatrix;
	glm::mat4 rotationMatrix;
	glm::mat4 transformMatrix;

	float upperBound;	// max heights particle can reach before it reset
	float lowerBound;

	unsigned int cubeVAO;
	unsigned int cubeVBO;
	unsigned int instanceVBO;
	bool firstInit = true;

public:
	std::vector<glm::mat4> matrixModels;	// temporary leave public
	std::vector<glm::mat4> tempMatricies;
	ParticleGeometry();

	void init(const ParticleControl& control);
	void clear();
	void reset();
	void render(Shader& shader, Camera* camera, int& numRender, float& speed, bool& pause);
};

