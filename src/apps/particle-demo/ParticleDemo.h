#pragma once

#include "../../core/layers/AppLayer.h"
#include "ParticleGeometry.h"
#include <memory>

class ParticleDemo : public AppLayer
{
private:
	float speed = 0.001f;
	bool pause = true;
	bool reset = false;
	bool isPopulating = false;
	glm::vec3 spawnArea = glm::vec3(100.0, 10.0, 100.0);
	glm::vec3 direction = glm::vec3(0.0, 0.0, 0.0);
	unsigned int numInstances = 100000;
	int numRender = numInstances;
	float heightLimit = 100.0f;
	glm::vec2 randomRange = glm::vec2(-5.0, 5.0);

	glm::vec3 particleSize = glm::vec3(0.1, 0.1, 0.1);
	std::vector<glm::mat4> matrixModels;

	ParticleControl particleControl = ParticleControl(randomRange, spawnArea, heightLimit, -heightLimit, numInstances, particleSize);
	ParticleGeometry particleRenderer;

public: 
	ParticleDemo(const std::string& name);
	~ParticleDemo() = default;
	
	void OnAttach(LayerManager* manager) override;
	void OnDetach() override;
	void OnUpdate() override;
	void OnGuiUpdate() override;
	void OnEvent(Event& event) override;

	//can run the demo without the editor
	static int show_demo();
	static int run();
};

