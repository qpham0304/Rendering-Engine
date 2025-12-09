#pragma once

#include <core/layers/AppLayer.h>
#include <graphics/framework/OpenGL/core/ParticleGeometry.h>
#include <memory>

class ParticleDemo : public AppLayer
{
private:
	bool pause = true;
	bool reset = false;
	bool isPopulating = false;
	float speed = 0.001f;
	unsigned int numInstances = 100000;
	int numRender = numInstances;
	float heightLimit = 100.0f;
	glm::vec3 spawnArea = glm::vec3(100.0, 10.0, 100.0);
	glm::vec3 direction = glm::vec3(0.0, 0.0, 0.0);
	glm::vec2 randomRange = glm::vec2(-5.0, 5.0);

	glm::vec3 particleSize = glm::vec3(0.1, 0.1, 0.1);
	std::vector<glm::mat4> matrixModels;

	ParticleControl particleControl = ParticleControl(randomRange, spawnArea, heightLimit, -heightLimit, numInstances, particleSize);
	ParticleGeometry particleRenderer;

public: 
	ParticleDemo(const std::string& name);
	~ParticleDemo() = default;
	
	void onAttach(LayerManager* manager) override;
	void onDetach() override;
	void onUpdate() override;
	void onGuiUpdate() override;
	void onEvent(Event& event) override;

};

