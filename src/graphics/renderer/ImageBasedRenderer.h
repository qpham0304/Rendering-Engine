#pragma once

#include <string>
#include <Shader.h>
#include <memory>

class ImageBasedRenderer
{
private:
	unsigned int cubeVAO = 0;
	unsigned int cubeVBO = 0;
	unsigned int captureFBO, captureRBO;

	Shader equirectangularToCubemapShader;
	Shader irradianceShader;
	Shader backgroundShader;
	Shader prefilterShader;
	Shader brdfShader;

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

