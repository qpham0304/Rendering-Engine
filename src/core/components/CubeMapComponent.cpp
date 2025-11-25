#include "CubeMapComponent.h"
#include "../src/graphics/utils/Utils.h"
#include <imgui.h>
#include "../../src/core/features/Camera.h"

CubeMapComponent::CubeMapComponent(const std::string& path)
{
	imagebasedRenderer.init(path);
    skyboxRenderer.updateTexture(imagebasedRenderer.envCubemapTexture);
}

CubeMapComponent::~CubeMapComponent()
{
    skyboxRenderer.free();
    imagebasedRenderer.free();
}

void CubeMapComponent::render(Camera* camera)
{
	skyboxRenderer.render(camera);
	//skyboxRenderer.render(camera, imagebasedRenderer.prefilterMap);
	//skyboxRenderer.render(camera, imagebasedRenderer.irradianceMap);
}

void CubeMapComponent::bindIBL()
{
    imagebasedRenderer.bindIrradianceMap();
    imagebasedRenderer.bindPrefilterMap();
    imagebasedRenderer.bindLUT();
}

void CubeMapComponent::reloadTexture(const std::string& path)
{
    imagebasedRenderer.onTextureReload(path);
    skyboxRenderer.updateTexture(imagebasedRenderer.envCubemapTexture);
}

void CubeMapComponent::reloadTexture(const unsigned int& textureID)
{
    imagebasedRenderer.onTextureReload(textureID);
    skyboxRenderer.updateTexture(imagebasedRenderer.envCubemapTexture);
}

unsigned int CubeMapComponent::envMap()
{
    return imagebasedRenderer.envCubemapTexture;
}

unsigned int CubeMapComponent::irradianceMap()
{
    return imagebasedRenderer.irradianceMap;
}

unsigned int CubeMapComponent::prefilterMap()
{
    return imagebasedRenderer.prefilterMap;
}

unsigned int CubeMapComponent::brdfLUT()
{
    return imagebasedRenderer.brdfLUTTexture;
}
