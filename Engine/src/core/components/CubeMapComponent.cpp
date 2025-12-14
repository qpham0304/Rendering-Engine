#include "CubeMapComponent.h"
#include "graphics/utils/Utils.h"
#include "core/features/Camera.h"

CubeMapComponent::CubeMapComponent(const std::string& m_path)
{
	imagebasedRenderer.init(m_path);
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

void CubeMapComponent::reloadTexture(const std::string& m_path)
{
    imagebasedRenderer.onTextureReload(m_path);
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
