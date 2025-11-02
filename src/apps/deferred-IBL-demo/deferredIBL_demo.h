#pragma once

#include "../../core/layers/AppLayer.h"
#include "ParticleGeometry.h"
#include "../../graphics/renderer/ShadowMapRenderer.h"

class DeferredIBLDemo : public AppLayer
{
private:
	float frameCounter = 0.0f;
	float lastFrame = 0.0f;
	float deltaTime = 0.0f;
	unsigned int cubeVAO = 0;
	unsigned int cubeVBO = 0;
	unsigned int sphereVAO = 0;
	unsigned int indexCount = 0;
	float lf = 0.0f;

	float speed = 0.001f;
	bool pause = true;
	bool timePaused = false;
	float currentFrame = 0.0f;

	bool reset = false;
	bool isPopulating = false;
	glm::vec3 spawnArea = glm::vec3(30.0, 10.0, 30.0);
	glm::vec3 direction = glm::vec3(0.0, 0.0, 0.0);
	unsigned int numInstances = 100000;
	int numRender = numInstances;
	float heightLimit = 100.0f;
	glm::vec2 randomRange = glm::vec2(3.0, 5.0);

	glm::vec3 particleSize = glm::vec3(0.1, 0.1, 0.1);
	std::vector<glm::mat4> matrixModels;

	ParticleControl particleControl = ParticleControl(
		randomRange, 
		spawnArea, 
		heightLimit, 
		-heightLimit, 
		numInstances, 
		particleSize
	);
	ParticleGeometry particleRenderer;
	
	Shader particleShader;

	FrameBuffer lightPassFBO;

	unsigned int gBuffer;
	unsigned int gDepth, gNormal, gAlbedo, gMetalRoughness, gEmissive, gDUV, gPosition;

	Shader pbrShader;

	std::default_random_engine generator;
	std::vector<glm::vec3> ssaoKernel;
	unsigned int ssaoFBO;
	unsigned int ssaoTexture;
	unsigned int noiseTexture;
	unsigned int ssaoBlurFBO;
	unsigned int ssaoBlurTexture;


	FrameBuffer transmittanceLUT;
	FrameBuffer multipleScatteredLUT;
	FrameBuffer skyViewLUT;
	FrameBuffer atmosphereScene;

	FrameBuffer ssrSceneFBO;

	DepthMap depthMap;

	void setupBuffers();
	void renderPrePass();
	void renderDeferredPass();
	void setupSkyView();
	void renderSkyView();

	void setupSSAO();
	void renderSSAO();

	void setupSSR();
	void renderSSR();

	void setupShadow();
	void renderShadow();

	void setupForwardPass();
	void renderForwardPass();

public:
	DeferredIBLDemo(const std::string& name);
	~DeferredIBLDemo() = default;
	
	void OnAttach() override;
	void OnDetach() override;
	void OnUpdate() override;
	void OnGuiUpdate() override;
	void OnEvent(Event& event) override;

	//can run the demo without the editor
	static int show_demo();
	static int run();
};

