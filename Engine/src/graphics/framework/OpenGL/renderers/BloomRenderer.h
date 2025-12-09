#pragma once

#include "../core/BloomFBO.h"

class ShaderOpenGL;

class BloomRenderer
{
public:
	BloomRenderer();
	~BloomRenderer();
	bool Init(unsigned int windowWidth, unsigned int windowHeight);
	void Destroy();
	void RenderBloomTexture(unsigned int srcTexture, float filterRadius);
	unsigned int BloomTexture();
	unsigned int BloomMip_i(int index);

private:
	void RenderDownsamples(unsigned int srcTexture);
	void RenderUpsamples(float filterRadius);

	bool mInit;
	bloomFBO mFBO;
	glm::ivec2 mSrcViewportSize;
	glm::vec2 mSrcViewportSizeFloat;
	ShaderOpenGL* mDownsampleShader;
	ShaderOpenGL* mUpsampleShader;

	bool mKarisAverageOnDownsample = true;
};
