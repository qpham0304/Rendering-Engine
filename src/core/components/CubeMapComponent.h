#pragma once

#include "../src/graphics/renderers/ImageBasedRenderer.h"
#include "../src/graphics/renderers/SkyboxRenderer.h"

class Camera;

class CubeMapComponent
{
private:
	// skybox's default constructor load some skybox texture by default
	// aka: six textures are loaded without being used
	// legacy code's skybox is referencing them so remove it when have chance later
	
public:
	SkyboxRenderer skyboxRenderer;
	ImageBasedRenderer imagebasedRenderer;

	CubeMapComponent(const std::string& path);
	~CubeMapComponent();

	void render(Camera* camera);
	void bindIBL();
	void reloadTexture(const std::string& path = "Textures/default/metallic.png");
	void reloadTexture(const unsigned int& textureID);
	unsigned int envMap();
	unsigned int irradianceMap();
	unsigned int prefilterMap();
	unsigned int brdfLUT();
};

