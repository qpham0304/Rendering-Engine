//#include "../../renderer/ShadowMapRenderer.h"
//#include "../../../core/components/MComponent.h"
//#include "../../../core/components/LightComponent.h"
//
//ShadowMapRenderer::ShadowMapRenderer(const unsigned int width, const unsigned int height)
//{
//	depthMap.reset(new DepthMap(width, height));
//	shader.reset(new Shader("Shaders/shadowMap.vert", "Shaders/shadowMap.frag"));
//}
//
//void ShadowMapRenderer::renderShadow(Light& light, std::vector<Model>& models, std::vector<glm::mat4>& modelMatrices)	//TODO: should be a Component since it contains positions
//{
//	depthMap->Bind();
//	glViewport(0, 0, depthMap->getWidth(), depthMap->getHeight());
//	glClearColor(0.1f, 0.1f, 0.1f, 1.0f); // RGBA
//	glClear(GL_DEPTH_BUFFER_BIT);
//	shader->Activate();
//	shader->setMat4("mvp", light.mvp);
//	for (int i = 0; i < models.size(); i++) {
//		shader->setMat4("matrix", modelMatrices[i]);
//		shader->setBool("hasAnimation", false);
//		models[i].Draw(*shader);
//	}
//	depthMap->Unbind();
//
//}
//
//void ShadowMapRenderer::init(unsigned int width, unsigned int height)
//{
//	depthMap.reset(new DepthMap(width, height));
//	shader.reset(new Shader("Shaders/shadowMap.vert", "Shaders/shadowMap.frag"));
//}
//
//unsigned int ShadowMapRenderer::depthTexture()
//{
//	return depthMap->texture;
//}
//
//unsigned int ShadowMapRenderer::getWidth()
//{
//	return depthMap->getWidth();
//}
//
//unsigned int ShadowMapRenderer::getHeight()
//{
//	return depthMap->getHeight();
//}
