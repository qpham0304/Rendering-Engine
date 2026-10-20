#include "DeferredRendererVulkan.h"
#include <window/AppWindow.h>
#include <core/resources/managers/TextureManager.h>
#include <core/resources/managers/MeshManager.h>
#include <core/resources/managers/ModelManager.h>
#include <core/resources/managers/DescriptorManager.h>
#include <core/scene/SceneManager.h>
#include <core/features/ServiceLocator.h>
#include <core/events/EventManager.h>
#include <core/features/Mesh.h>
#include <core/features/Camera.h>
#include <graphics/renderers/RenderDevice.h>
#include <graphics/framework/Vulkan/resources/textures/TextureVulkan.h>
#include <graphics/framework/Vulkan/resources/descriptors/DescriptorManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/materials/MaterialManagerVulkan.h>
#include <graphics/framework/Vulkan/resources/textures/TextureManagerVulkan.h>
#include <graphics/framework/Vulkan/renderers/RendererManagerVulkan.h>
#include <graphics/framework/vulkan/core/VulkanPipeline.h>
#include <graphics/framework/Vulkan/renderers/RenderDeviceVulkan.h>
#include <graphics/framework/vulkan/renderers/renderpasses/AmbientOcclusionPassVulkan.h>
#include <graphics/framework/vulkan/renderers/renderpasses/HiZPassVulkan.h>
#include <graphics/framework/vulkan/renderers/renderpasses/SSRGIPassVulkan.h>
#include <graphics/framework/vulkan/renderers/features/DDGIBuilderVulkan.h>
#include <graphics/framework/Vulkan/resources/buffers/DeviceAddressBufferVulkan.h>
#include <gui/GuiManager.h>
#include <imgui.h>

DeferredRendererVulkan::DeferredRendererVulkan()
	: RendererVulkan("DeferredRendererVulkan")
{

}

DeferredRendererVulkan::~DeferredRendererVulkan()
{

}

bool DeferredRendererVulkan::init(WindowConfig config)
{
	RendererVulkan::init(config);

	RendererVulkan* renderer = nullptr;
	renderer = rendererManagerVulkan->getRenderer("ShadowMapPassVulkan");
	shadowMapPass = dynamic_cast<ShadowMapPassVulkan*>(renderer);
	renderer = rendererManagerVulkan->getRenderer("ImageBasedRendererVulkan");
	imageBasedRenderer = dynamic_cast<ImageBasedRendererVulkan*>(renderer);
	renderer = rendererManagerVulkan->getRenderer("AmbientOcclusionPassVulkan");
	ambientOcclusionPass = dynamic_cast<AmbientOcclusionPassVulkan*>(renderer);
	renderer = rendererManagerVulkan->getRenderer("HiZPassVulkan");
	hiZPass = dynamic_cast<HiZPassVulkan*>(renderer);
	renderer = rendererManagerVulkan->getRenderer("GeometryPassVulkan");
	geometryPass = dynamic_cast<GeometryPassVulkan*>(renderer);
	renderer = rendererManagerVulkan->getRenderer("LightingPassVulkan");
	lightingPass = dynamic_cast<LightingPassVulkan*>(renderer);

	assert(shadowMapPass
		&& imageBasedRenderer
		&& ambientOcclusionPass
		&& hiZPass
		&& geometryPass
		&& lightingPass
		&& "failed to retrieve renderer"
	);

	// the geometry pass is registered before this renderer so the g-buffer exists already,
	// ao has to exist before the lighting pass (registered after this) builds its descriptors
	ambientOcclusionPass->init(config);
	hiZPass->init(config);

	return true;
}

bool DeferredRendererVulkan::onClose()
{
	return true;
}

void DeferredRendererVulkan::onUpdate()
{

}

void DeferredRendererVulkan::render(Camera& camera)
{
	RendererVulkan::render(camera);
}

void DeferredRendererVulkan::renderGui()
{
	SceneManager& sceneManager = SceneManager::getInstance();
	Scene* scene = sceneManager.getActiveScene();
	if (!scene) {
		m_logger->error("No scene to render");
		return;
	}

	auto entities = scene->getEntitiesWith<LightComponent>();
	auto& pushConstantLight = lightingPass->pushConstantLight;

	ImGui::Begin("Lights Control");
	if(ImGui::Button("Change Environment")) {
		std::string path;
		path = Utils::fileDialog();
		if(!path.empty()) {
			imageBasedRenderer->loadTexture(path);
		}
	}
	ImGui::SliderFloat("skybox detail", &pushConstantLight.skyboxDetail, 0.0f, 1.0f);
	ImGui::SliderFloat("Sun Azimuth", &shadowMapPass->sunAzimuth, 0.0f, 6.28f);
	ImGui::SliderFloat("Sun Elevation", &shadowMapPass->sunElevation, -3.14f, 3.14f);
	ImGui::SliderFloat("Sun view Area", &shadowMapPass->s, 1.0f, 25.0f);
	ImGui::SliderFloat("Sun zNear", &shadowMapPass->zNear, 0.01f, 15.0f);
	ImGui::SliderFloat("Sun zFar", &shadowMapPass->zFar, 50.0f, 200.0f);

	// ImGui::SliderFloat("Sun Azimuth", &shadowMapRenderer->sunAzimuthDeg, 0.0f, 360.0f);
	// ImGui::SliderFloat("Sun Elevation", &shadowMapRenderer->sunElevationDeg, 0.0f, 90.0f);
	ImGui::DragFloat("G phase function", &pushConstantLight.G, 0.01f, 0.0f, 1.0f);
	ImGui::DragFloat("scattering Scale", &pushConstantLight.scatteringScale, 0.01f, 0.0f, 5.0f);

	ImGui::Checkbox("Light Ortho", &shadowMapPass->useOrtho);
	ImGui::SameLine();
	bool aoChecked = (pushConstantLight.aoOn != 0);
	if (ImGui::Checkbox("aoOn", &aoChecked)) {
		pushConstantLight.aoOn = aoChecked ? 1 : 0;
	}
	ImGui::SameLine();
	bool denoiserChecked = (denoiserOn != 0);
	if (ImGui::Checkbox("reproject", &denoiserChecked)) {
		denoiserOn = denoiserChecked ? 1 : 0;
	}
	ImGui::SameLine();
	bool shouldCombineChecked = (shouldCombine != 0);
	if (ImGui::Checkbox("combine", &shouldCombineChecked)) {
		shouldCombine = shouldCombineChecked ? 1 : 0;
	}
	ImGui::SliderInt("aoBlurRadius", &ambientOcclusionPass->blurrPushConstant.blurRadius, 1.0f, 16.0f);
	ImGui::SliderFloat("aoBlurScale", &ambientOcclusionPass->blurrPushConstant.scale, 1.0f, 100.0f);
	ImGui::ColorEdit4("color", &lightingPass->sunColor[0]);
	ImGui::SliderFloat("intensity", &lightingPass->sunIntensity, 1.0f, 15.0f);
	ImGui::SliderFloat("Bias", &pushConstantLight.bias, 0.001f, 0.1f);
	ImGui::SliderFloat("Alpha", &pushConstantLight.alpha, 0.0001f, 0.01f);
	ImGui::SliderFloat("Lintstep Low", &pushConstantLight.lintstepLow, 0.01f, 1.0f);
	ImGui::SliderFloat("Lintstep High", &pushConstantLight.linstepHigh, 0.01f, 2.0f);
	ImGui::SliderFloat("Lit Bias", &pushConstantLight.litBias, 0.0001f, 0.01f, "%.4f", ImGuiSliderFlags_Logarithmic);
	uint32_t min_r = 1;
	uint32_t max_r = 64;
	ImGui::SliderScalar("radius", ImGuiDataType_U32, &shadowMapPass->pushconstant.radius, &min_r, &max_r);
	ImGui::SliderFloat("sigma", &shadowMapPass->pushconstant.sigma, 1.0f, 30.0f, "%.4f", ImGuiSliderFlags_Logarithmic);


	auto& lights = lightingPass->lights;
	int i = 0;
	for (auto& entity : entities) {
		TransformComponent& transform = entity.getComponent<TransformComponent>();
		LightComponent& light = entity.getComponent<LightComponent>();

		std::string name = entity.getComponent<NameComponent>().name + std::to_string(i);
		if (ImGui::CollapsingHeader(name.c_str())) {
			ImGui::SliderFloat4("Position", &transform.translateVec[0], -50.0, 50.0);
			ImGui::ColorEdit4("Color", &light.color[0]);
		}

		if(i < lights.size()) {	// lights are gathered by the lighting pass, it can be behind on the frame a light is added
			lights[i].color = light.color;
			lights[i].position = glm::vec4(transform.translateVec, 1.0f);
		}

		i++;
	}
	ImGui::End();

	ImGui::Begin("G-Buffer Debug");

	auto& gBuffer = geometryPass->renderTarget;
	ImGui::Image((ImTextureID)textureManagerVulkan->inspectTexture(lightingPass->renderTarget.colorTexture->id()), ImVec2(256, 144));
	ImGui::Image((ImTextureID)textureManagerVulkan->inspectTexture(gBuffer.gBufferPos->id()), ImVec2(256, 144));
	ImGui::Image((ImTextureID)textureManagerVulkan->inspectTexture(gBuffer.gBufferNorm->id()), ImVec2(256, 144));
	ImGui::Image((ImTextureID)textureManagerVulkan->inspectTexture(gBuffer.gBufferAlbedo->id()), ImVec2(256, 144));
	ImGui::Image((ImTextureID)textureManagerVulkan->inspectTexture(gBuffer.gPBR->id()), ImVec2(256, 144));
	ImGui::Image((ImTextureID)textureManagerVulkan->inspectTexture(gBuffer.depthTexture->id()), ImVec2(256, 144));
	ImGui::Image((ImTextureID)textureManagerVulkan->inspectTexture(gBuffer.gBufferMotion->id()), ImVec2(256, 144));
	ImGui::Image((ImTextureID)(textureManagerVulkan->inspectTexture(shadowMapPass->depthID)), ImVec2(256, 144));

	ImGui::End();
}

void DeferredRendererVulkan::_recreateResources()
{

}

void DeferredRendererVulkan::_cleanupResources()
{

}

void DeferredRendererVulkan::addToGraph(RenderGraphVulkan& graph)
{

}
