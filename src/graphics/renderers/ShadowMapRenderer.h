#pragma once

#include <memory>
#include <Shader.h>
#include "../../src/core/components/LightComponent.h"
#include "../../graphics/framework/OpenGL/core/DepthMap.h"

class ShadowMapRenderer
{

private:
	std::unique_ptr<Shader> shader;
	std::unique_ptr<DepthMap> depthMap;

public:
	ShadowMapRenderer() = default;
	ShadowMapRenderer(unsigned int width, unsigned int height);

	void renderShadow(Light& light, std::vector<Model>& models, std::vector<glm::mat4>& modelMatrices);
	void init(const unsigned int width, const unsigned int height);
	unsigned int depthTexture();
	unsigned int getWidth();
	unsigned int getHeight();

};