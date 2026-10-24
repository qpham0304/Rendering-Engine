#pragma once

#include <memory>
#include "graphics/framework/OpenGL/core/ShaderOpenGL.h"
#include "core/components/LightComponent.h"
#include "graphics/framework/OpenGL/core/DepthMap.h"
#include "core/features/Object.h"

class ShadowMapRenderer : public Object
{

private:
	std::unique_ptr<ShaderOpenGL> shader;
	std::unique_ptr<DepthMap> depthMap;

public:
	ShadowMapRenderer() { setFootprint("ShadowMapRenderer", sizeof(ShadowMapRenderer), MemorySystem::Graphics); };
	ShadowMapRenderer(unsigned int width, unsigned int height);

	void renderShadow(Light& light, std::vector<ModelOpenGL>& models, std::vector<glm::mat4>& modelMatrices);
	void init(const unsigned int width, const unsigned int height);
	unsigned int depthTexture();
	unsigned int getWidth();
	unsigned int getHeight();

};