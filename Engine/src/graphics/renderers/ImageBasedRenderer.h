#pragma once

#include <string>
#include <memory>
#include "graphics/framework/OpenGL/core/ShaderOpenGL.h"

class ImageBasedRenderer
{
private:
	unsigned int cubeVAO = 0;
	unsigned int cubeVBO = 0;
	unsigned int captureFBO, captureRBO;

	ShaderOpenGL equirectToCubeMapShader;
	ShaderOpenGL irradianceShader;
	ShaderOpenGL backgroundShader;
	ShaderOpenGL prefilterShader;
	ShaderOpenGL brdfShader;

	void setupCubeMap();
	void renderCubeMap();
	void setupIrradianceMap();
	void renderIrradianceMap();
	void setupPrefilterMap();
	void renderPrefilterMap();
	void setupBRDF();
	void renderBRDF();

public:
	ImageBasedRenderer();
	~ImageBasedRenderer() = default;

	void init(const std::string& path);
	void bindIrradianceMap();
	void bindPrefilterMap();
	void bindLUT();
	void onTextureReload(const std::string& path);
	void onTextureReload(const unsigned int& textureID);
	void free();

	unsigned int envCubemapTexture;
	unsigned int hdrTexture;

	unsigned int prefilterMap;
	unsigned int irradianceMap;
	unsigned int brdfLUTTexture;
};

