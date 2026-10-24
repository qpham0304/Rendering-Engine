#pragma once

#include "core/features/Serializable.h"

#include <glm/glm.hpp>
#include <string>

// The tunables of each renderer, update every frame. Renderer registers its struct during initialization
// The editor draws whatever is registered through DEFINE_TYPE

struct BloomSettings {
	float strength { 0.04f };
	float exposure { 1.0f };
	float threshold { 0.5f };
	float softThreshold { 0.5f };
	bool lensDirt { true };

	DEFINE_TYPE(BloomSettings,
		member("strength", &BloomSettings::strength, FieldMeta{ .min = 0.0f, .max = 1.0f }),
		member("exposure", &BloomSettings::exposure, FieldMeta{ .min = 0.0f, .max = 1.0f }),
		member("threshold", &BloomSettings::threshold, FieldMeta{ .min = 0.0f, .max = 1.0f }),
		member("softThreshold", &BloomSettings::softThreshold, FieldMeta{ .min = 0.0f, .max = 1.0f }),
		member("lensDirt", &BloomSettings::lensDirt)
	)
};

struct ShadowMapSettings {
	float sunAzimuth { 0.0f };			// radians around y
	float sunElevation { 1.357f };		// radians above the horizon
	float orthoExtent { 25.0f };		// half size of the area the shadow map covers
	float zNear { 0.01f };
	float zFar { 105.0f };
	bool useOrtho { true };
	uint32_t blurRadius { 64 };			// moment blur
	float blurSigma { 15.0f };

	// runtime state, not reflected
	glm::vec3 lightDir { glm::normalize(glm::vec3(1.0f)) };	// toward the sun, what the shadow map is rendered from
	glm::mat4 lightSpaceMatrix { 1.0f };

	DEFINE_TYPE(ShadowMapSettings,
		member("sunAzimuth", &ShadowMapSettings::sunAzimuth, FieldMeta{ .min = 0.0f, .max = 6.28f }),
		member("sunElevation", &ShadowMapSettings::sunElevation, FieldMeta{ .min = -3.14f, .max = 3.14f }),
		member("orthoExtent", &ShadowMapSettings::orthoExtent, FieldMeta{ .min = 1.0f, .max = 25.0f }),
		member("zNear", &ShadowMapSettings::zNear, FieldMeta{ .min = 0.01f, .max = 15.0f }),
		member("zFar", &ShadowMapSettings::zFar, FieldMeta{ .min = 50.0f, .max = 200.0f }),
		member("useOrtho", &ShadowMapSettings::useOrtho),
		member("blurRadius", &ShadowMapSettings::blurRadius, FieldMeta{ .min = 1.0f, .max = 64.0f }),
		member("blurSigma", &ShadowMapSettings::blurSigma, FieldMeta{ .min = 1.0f, .max = 30.0f, .format = "%.4f", .logarithmic = true })
	)
};

struct LightingSettings {
	glm::vec4 sunColor { 1.0f };
	float sunIntensity { 2.0f };
	float skyboxDetail { 0.0f };
	bool ambientOcclusion { true };
	float phaseG { 0.7f };
	float scatteringScale { 0.2f };
	
	// moment shadow filtering
	float shadowBias { 0.001f };
	float momentAlpha { 0.0001f };
	float linstepLow { 0.2f };
	float linstepHigh { 1.0f };
	float litBias { 0.0005f };

	DEFINE_TYPE(LightingSettings,
		member("sunColor", &LightingSettings::sunColor, FieldMeta{ .widget = FieldWidget::Color }),
		member("sunIntensity", &LightingSettings::sunIntensity, FieldMeta{ .min = 1.0f, .max = 15.0f }),
		member("skyboxDetail", &LightingSettings::skyboxDetail, FieldMeta{ .min = 0.0f, .max = 1.0f }),
		member("ambientOcclusion", &LightingSettings::ambientOcclusion),
		member("phaseG", &LightingSettings::phaseG, FieldMeta{ .widget = FieldWidget::Drag, .min = 0.0f, .max = 1.0f, .tooltip = "G of the phase function" }),
		member("scatteringScale", &LightingSettings::scatteringScale, FieldMeta{ .widget = FieldWidget::Drag, .min = 0.0f, .max = 5.0f }),
		member("shadowBias", &LightingSettings::shadowBias, FieldMeta{ .min = 0.001f, .max = 0.1f }),
		member("momentAlpha", &LightingSettings::momentAlpha, FieldMeta{ .min = 0.0001f, .max = 0.01f, .format = "%.4f" }),
		member("linstepLow", &LightingSettings::linstepLow, FieldMeta{ .min = 0.01f, .max = 1.0f }),
		member("linstepHigh", &LightingSettings::linstepHigh, FieldMeta{ .min = 0.01f, .max = 2.0f }),
		member("litBias", &LightingSettings::litBias, FieldMeta{ .min = 0.0001f, .max = 0.01f, .format = "%.4f", .logarithmic = true })
	)
};

struct AmbientOcclusionSettings {
	int blurRadius { 4 };
	float blurScale { 100.0f };

	DEFINE_TYPE(AmbientOcclusionSettings,
		member("blurRadius", &AmbientOcclusionSettings::blurRadius, FieldMeta{ .min = 1.0f, .max = 16.0f }),
		member("blurScale", &AmbientOcclusionSettings::blurScale, FieldMeta{ .min = 1.0f, .max = 100.0f })
	)
};

struct SSRSettings {
	int downsample { 0 };
	int maxSteps { 64 };
	float thickness { 0.25f };
	float maxDistance { 500.0f };
	bool useTLAS { false };

	DEFINE_TYPE(SSRSettings,
		member("downsample", &SSRSettings::downsample, FieldMeta{ .min = 0.0f, .max = 2.0f, .tooltip = "0 full, 1 half, 2 quarter resolution" }),
		member("maxSteps", &SSRSettings::maxSteps, FieldMeta{ .min = 8.0f, .max = 256.0f, .tooltip = "hi-z traversal iterations per ray" }),
		member("thickness", &SSRSettings::thickness, FieldMeta{ .min = 0.01f, .max = 5.0f, .tooltip = "view space depth a hit may be behind the surface" }),
		member("maxDistance", &SSRSettings::maxDistance, FieldMeta{ .min = 1.0f, .max = 1000.0f }),
		member("useTLAS", &SSRSettings::useTLAS, FieldMeta{ .tooltip = "rays that leave the screen continue against the scene, needs ray tracing support" })
	)
};

struct ImageBasedSettings {
	std::string environmentMap { "assets/textures/hdr/farm_field_puresky_2k.hdr" };
	// "assets/textures/hdr/newport_loft.hdr",
	// "assets/textures/hdr/meadow_1k.hdr",
	// "assets/textures/night-skybox/top.jpg",
	// "assets/textures/hdr/photo_studio_loft_hall_2k.hdr",

	DEFINE_TYPE(ImageBasedSettings,
		member("environmentMap", &ImageBasedSettings::environmentMap, FieldMeta{ .widget = FieldWidget::FilePath })
	)
};

struct DeferredSettings {
	bool temporalReprojection { true };
	bool combine { true };

	uint32_t litColor { 0 };
	uint32_t position { 0 };
	uint32_t normal { 0 };
	uint32_t albedo { 0 };
	uint32_t pbr { 0 };
	uint32_t depth { 0 };
	uint32_t motion { 0 };
	uint32_t shadowDepth { 0 };

	DEFINE_TYPE(DeferredSettings,
		member("temporalReprojection", &DeferredSettings::temporalReprojection),
		member("combine", &DeferredSettings::combine),
		member("litColor", &DeferredSettings::litColor, FieldMeta{ .widget = FieldWidget::Texture, .readOnly = true }),
		member("position", &DeferredSettings::position, FieldMeta{ .widget = FieldWidget::Texture, .readOnly = true }),
		member("normal", &DeferredSettings::normal, FieldMeta{ .widget = FieldWidget::Texture, .readOnly = true }),
		member("albedo", &DeferredSettings::albedo, FieldMeta{ .widget = FieldWidget::Texture, .readOnly = true }),
		member("pbr", &DeferredSettings::pbr, FieldMeta{ .widget = FieldWidget::Texture, .readOnly = true }),
		member("depth", &DeferredSettings::depth, FieldMeta{ .widget = FieldWidget::Texture, .readOnly = true }),
		member("motion", &DeferredSettings::motion, FieldMeta{ .widget = FieldWidget::Texture, .readOnly = true }),
		member("shadowDepth", &DeferredSettings::shadowDepth, FieldMeta{ .widget = FieldWidget::Texture, .readOnly = true })
	)
};

struct DDGISettings {
	bool enabled { true };
	bool indirectOnly { false };
	bool resetProbes { false };
	float intensity { 1.0f };
	float skyIntensity { 1.0f };
	float hysteresis { 0.97f };
	float normalBias { 0.3f };

	// the probe grid of the active scene, zero probes when it has none
	glm::ivec3 probeCounts { 0 };
	glm::vec3 gridMin { 0.0f };
	glm::vec3 gridMax { 0.0f };
	uint32_t irradianceAtlas { 0 };
	uint32_t visibilityAtlas { 0 };

	// runtime state, not reflected, written by the ddgi builder every frame,
	// the grid the atlases were last built with so samplers always match them
	bool hasProbeGrid { false };
	glm::vec4 gridOrigin { 0.0f };
	float gridSpacing { 0.0f };

	// the atlases hold usable probes, passes that sample them check this first
	bool isReady() const { return enabled && hasProbeGrid; }

	DEFINE_TYPE(DDGISettings,
		member("enabled", &DDGISettings::enabled),
		member("indirectOnly", &DDGISettings::indirectOnly),
		member("resetProbes", &DDGISettings::resetProbes, FieldMeta{ .widget = FieldWidget::Button }),
		member("intensity", &DDGISettings::intensity, FieldMeta{ .min = 0.0f, .max = 5.0f }),
		member("skyIntensity", &DDGISettings::skyIntensity, FieldMeta{ .min = 0.0f, .max = 2.0f }),
		member("hysteresis", &DDGISettings::hysteresis, FieldMeta{ .min = 0.0f, .max = 0.99f }),
		member("normalBias", &DDGISettings::normalBias, FieldMeta{ .min = 0.0f, .max = 1.0f }),
		member("probeCounts", &DDGISettings::probeCounts, FieldMeta{ .readOnly = true }),
		member("gridMin", &DDGISettings::gridMin, FieldMeta{ .format = "%.1f", .readOnly = true }),
		member("gridMax", &DDGISettings::gridMax, FieldMeta{ .format = "%.1f", .readOnly = true }),
		member("irradianceAtlas", &DDGISettings::irradianceAtlas, FieldMeta{ .widget = FieldWidget::Texture, .readOnly = true }),
		member("visibilityAtlas", &DDGISettings::visibilityAtlas, FieldMeta{ .widget = FieldWidget::Texture, .readOnly = true })
	)
};

