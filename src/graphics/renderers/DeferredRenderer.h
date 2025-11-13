#pragma once

#include <memory>
#include <Shader.h>
class Camera;

class DeferredRenderer
{
private:
	unsigned int gBuffer;
	unsigned int gPosition;
	unsigned int gNormal;
	unsigned int gColorSpec;
	unsigned int gAlbedoSpec;
	unsigned int rboDepth;
    unsigned int gDepth;
	int width;
	int height;


public:
	DeferredRenderer(const int width, const int height);
	std::unique_ptr<Shader> geometryShader;
	std::unique_ptr<Shader> colorShader;
	void renderGeometry(Camera* cameraPtr, std::vector<Component*>& components);
	void renderGeometry(Camera* cameraPtr, Component& component);
	void renderColor(Camera* cameraPtr, std::vector<Light>& lights);

	unsigned int getGBuffer();
	unsigned int getGPosition();
	unsigned int getGNormal();
	unsigned int getGColorspec();
	unsigned int getGAlbedoSpec();
	unsigned int getGDepth();
};

