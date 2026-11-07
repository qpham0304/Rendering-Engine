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
	int method { 0 };
	float rtSoftness { 0.5f };
	int rtRayCount { 4 };
	int rtBlurRadius { 3 };
	int resolution { 1 };
	float pcssSoftness { 0.04f };
	float pcssMaxPenumbra { 0.75f };
	float depthBias { 0.01f };
	float slopeBias { 1.0f };
	float slopeBiasMax { 0.05f };
	float normalBias { 0.0f };
	float maxDepthSlope { 16.0f };

	// runtime state, not reflected
	glm::vec3 lightDir { glm::normalize(glm::vec3(1.0f)) };	// toward the sun, what the shadow map is rendered from
	glm::mat4 lightSpaceMatrix { 1.0f };

	DEFINE_TYPE(ShadowMapSettings,
		member("sunAzimuth", &ShadowMapSettings::sunAzimuth, FieldMeta{ .min = 0.0f, .max = 6.28f }),
		member("sunElevation", &ShadowMapSettings::sunElevation, FieldMeta{ .min = -3.14f, .max = 3.14f }),
		member("orthoExtent", &ShadowMapSettings::orthoExtent, FieldMeta{ .min = 1.0f, .max = 50.0f }),
		member("zNear", &ShadowMapSettings::zNear, FieldMeta{ .min = 0.01f, .max = 15.0f }),
		member("zFar", &ShadowMapSettings::zFar, FieldMeta{ .min = 50.0f, .max = 200.0f }),
		member("useOrtho", &ShadowMapSettings::useOrtho),
		member("blurRadius", &ShadowMapSettings::blurRadius, FieldMeta{ .min = 1.0f, .max = 64.0f }),
		member("blurSigma", &ShadowMapSettings::blurSigma, FieldMeta{ .min = 1.0f, .max = 30.0f, .format = "%.4f", .logarithmic = true }),
		member("method", &ShadowMapSettings::method, FieldMeta{ .widget = FieldWidget::Combo,
			.tooltip = "ray traced needs ray tracing support, it falls back to the shadow map without it",
			.options = "Shadow Map\0Ray Traced\0\0" }),
		member("rtSoftness", &ShadowMapSettings::rtSoftness, FieldMeta{ .min = 0.0f, .max = 5.0f, .tooltip = "degrees, 0 is a hard shadow" }),
		member("rtRayCount", &ShadowMapSettings::rtRayCount, FieldMeta{ .min = 1.0f, .max = 16.0f }),
		member("rtBlurRadius", &ShadowMapSettings::rtBlurRadius, FieldMeta{ .min = 0.0f, .max = 8.0f, .tooltip = "0 turns the blur off" }),
		member("resolution", &ShadowMapSettings::resolution, FieldMeta{ .widget = FieldWidget::Combo,
			.tooltip = "shadow map texels per side, orthoExtent sets how much of the world they cover",
			.options = "512\0" "1024\0" "2048\0" "4096\0\0" }),
		member("pcssSoftness", &ShadowMapSettings::pcssSoftness, FieldMeta{ .min = 0.0f, .max = 0.2f, .format = "%.3f", .tooltip = "how fast the penumbra widens with distance from the occluder" }),
		member("pcssMaxPenumbra", &ShadowMapSettings::pcssMaxPenumbra, FieldMeta{ .min = 0.05f, .max = 3.0f, .tooltip = "widest soft edge in world units, also how far the blocker search looks" }),
		member("depthBias", &ShadowMapSettings::depthBias, FieldMeta{ .min = 0.0f, .max = 0.2f, .format = "%.4f", .tooltip = "constant bias in world units" }),
		member("slopeBias", &ShadowMapSettings::slopeBias, FieldMeta{ .min = 0.0f, .max = 4.0f, .tooltip = "scales the bias for the surface slope inside one shadow texel" }),
		member("slopeBiasMax", &ShadowMapSettings::slopeBiasMax, FieldMeta{ .min = 0.0f, .max = 0.5f, .format = "%.3f", .tooltip = "cap on the slope bias in world units, larger lifts the shadow off its caster at a low sun" }),
		member("normalBias", &ShadowMapSettings::normalBias, FieldMeta{ .min = 0.0f, .max = 4.0f, .tooltip = "moves the lookup along the normal in shadow texels, also detaches contact at a low sun" }),
		member("maxDepthSlope", &ShadowMapSettings::maxDepthSlope, FieldMeta{ .min = 1.0f, .max = 64.0f, .tooltip = "cap on the receiver plane slope, keeps silhouettes from exploding" })
	)
};

struct LightingSettings {
	glm::vec4 sunColor { 1.0f };
	float sunIntensity { 2.0f };
	float skyboxDetail { 0.0f };
	bool ambientOcclusion { true };
	float phaseG { 0.7f };

	// height fog the sun scatters in, god rays are its shadowed parts
	bool volumetricFog { true };
	float fogDensity { 0.005f };
	float fogHeightFalloff { 0.15f };
	float fogBaseHeight { 0.0f };
	float fogMaxDistance { 80.0f };
	int fogSteps { 24 };
	float fogSunScattering { 1.0f };
	float fogAmbientScattering { 1.0f };

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
		member("phaseG", &LightingSettings::phaseG, FieldMeta{ .min = 0.0f, .max = 0.95f, .tooltip = "how much the fog scatters forward, higher makes the shafts brighter looking toward the sun" }),
		member("volumetricFog", &LightingSettings::volumetricFog),
		member("fogDensity", &LightingSettings::fogDensity, FieldMeta{ .min = 0.0f, .max = 0.5f, .format = "%.4f", .logarithmic = true, .tooltip = "extinction per meter at the base height" }),
		member("fogHeightFalloff", &LightingSettings::fogHeightFalloff, FieldMeta{ .min = 0.0f, .max = 2.0f, .tooltip = "how fast the fog thins with height, 0 is uniform" }),
		member("fogBaseHeight", &LightingSettings::fogBaseHeight, FieldMeta{ .min = -50.0f, .max = 50.0f }),
		member("fogMaxDistance", &LightingSettings::fogMaxDistance, FieldMeta{ .min = 5.0f, .max = 500.0f, .tooltip = "how far the march goes, also the fog distance over the sky" }),
		member("fogSteps", &LightingSettings::fogSteps, FieldMeta{ .min = 4.0f, .max = 64.0f }),
		member("fogSunScattering", &LightingSettings::fogSunScattering, FieldMeta{ .min = 0.0f, .max = 20.0f, .tooltip = "brightens the sun shafts without thickening the fog, 1 is physical" }),
		member("fogAmbientScattering", &LightingSettings::fogAmbientScattering, FieldMeta{ .min = 0.0f, .max = 2.0f, .tooltip = "sky light in the fog, lower for interiors so the gaps between shafts stay dark, 1 is physical" }),
		member("shadowBias", &LightingSettings::shadowBias, FieldMeta{ .min = 0.001f, .max = 0.1f }),
		member("momentAlpha", &LightingSettings::momentAlpha, FieldMeta{ .min = 0.0001f, .max = 0.01f, .format = "%.4f" }),
		member("linstepLow", &LightingSettings::linstepLow, FieldMeta{ .min = 0.01f, .max = 1.0f }),
		member("linstepHigh", &LightingSettings::linstepHigh, FieldMeta{ .min = 0.01f, .max = 2.0f }),
		member("litBias", &LightingSettings::litBias, FieldMeta{ .min = 0.0001f, .max = 0.01f, .format = "%.4f", .logarithmic = true })
	)
};

struct AmbientOcclusionSettings {
	int method { 0 };
	float radius { 0.75f };
	float intensity { 2.0f };
	int rayCount { 4 };
	int blurType { 3 };
	int blurRadius { 4 };
	float blurScale { 100.0f };

	DEFINE_TYPE(AmbientOcclusionSettings,
		member("method", &AmbientOcclusionSettings::method, FieldMeta{ .widget = FieldWidget::Combo,
			.tooltip = "ray traced needs ray tracing support, it falls back to GTAO without it",
			.options = "Alchemy\0GTAO\0Ray Traced\0\0" }),
		member("radius", &AmbientOcclusionSettings::radius, FieldMeta{ .min = 0.05f, .max = 5.0f, .tooltip = "world space reach of the occlusion" }),
		member("intensity", &AmbientOcclusionSettings::intensity, FieldMeta{ .min = 0.1f, .max = 4.0f }),
		member("rayCount", &AmbientOcclusionSettings::rayCount, FieldMeta{ .min = 1.0f, .max = 32.0f, .tooltip = "rays per pixel, ray traced only" }),
		member("blurType", &AmbientOcclusionSettings::blurType, FieldMeta{ .widget = FieldWidget::Combo,
			.options = "None\0Box\0Gaussian\0Bilateral\0\0" }),
		member("blurRadius", &AmbientOcclusionSettings::blurRadius, FieldMeta{ .min = 1.0f, .max = 16.0f }),
		member("blurScale", &AmbientOcclusionSettings::blurScale, FieldMeta{ .min = 1.0f, .max = 100.0f, .tooltip = "bilateral depth edge sharpness" })
	)
};

struct SSRSettings {
	int downsample { 0 };
	int maxSteps { 64 };
	float thickness { 0.25f };
	float maxDistance { 500.0f };
	bool useTLAS { false };
	float maxSampleLuminance { 8.0f };
	bool reflectionOnly { false };
	bool restir { true };
	float restirMaxHistory { 20.0f };
	int restirSpatialSamples { 4 };
	float restirSpatialRadius { 16.0f };
	float restirMaxJacobian { 10.0f };

	DEFINE_TYPE(SSRSettings,
		member("downsample", &SSRSettings::downsample, FieldMeta{ .min = 0.0f, .max = 2.0f, .tooltip = "0 full, 1 half, 2 quarter resolution" }),
		member("maxSteps", &SSRSettings::maxSteps, FieldMeta{ .min = 8.0f, .max = 256.0f, .tooltip = "hi-z traversal iterations per ray" }),
		member("thickness", &SSRSettings::thickness, FieldMeta{ .min = 0.01f, .max = 5.0f, .tooltip = "view space depth a hit may be behind the surface" }),
		member("maxDistance", &SSRSettings::maxDistance, FieldMeta{ .min = 1.0f, .max = 1000.0f }),
		member("useTLAS", &SSRSettings::useTLAS, FieldMeta{ .tooltip = "rays that leave the screen continue against the scene, needs ray tracing support" }),
		member("maxSampleLuminance", &SSRSettings::maxSampleLuminance, FieldMeta{ .min = 1.0f, .max = 64.0f, .tooltip = "brightest single ray sample, lower removes sparkles but darkens bright bounces" }),
		member("reflectionOnly", &SSRSettings::reflectionOnly, FieldMeta{ .tooltip = "trace only reflections, diffuse indirect comes from ddgi / the sky" }),
		member("restir", &SSRSettings::restir, FieldMeta{ .tooltip = "reuse diffuse ray samples over time and from neighbors (restir gi)" }),
		member("restirMaxHistory", &SSRSettings::restirMaxHistory, FieldMeta{ .min = 1.0f, .max = 64.0f, .tooltip = "how many past samples a reservoir may stand for, lower reacts faster to light changes" }),
		member("restirSpatialSamples", &SSRSettings::restirSpatialSamples, FieldMeta{ .min = 0.0f, .max = 16.0f, .tooltip = "neighbor reservoirs merged per pixel" }),
		member("restirSpatialRadius", &SSRSettings::restirSpatialRadius, FieldMeta{ .min = 1.0f, .max = 64.0f, .tooltip = "neighbor search radius in trace pixels" }),
		member("restirMaxJacobian", &SSRSettings::restirMaxJacobian, FieldMeta{ .min = 1.5f, .max = 100.0f, .tooltip = "rejects neighbor samples whose solid angle changes more than this, lower is safer near corners" })
	)
};

struct TemporalSettings {
	float maxHistoryRough { 32.0f };
	float maxHistorySmooth { 2.0f };
	float maxHistoryMoving { 8.0f };
	float clampGamma { 1.5f };
	float depthTolerance { 0.03f };
	float normalThreshold { 0.95f };

	DEFINE_TYPE(TemporalSettings,
		member("maxHistoryRough", &TemporalSettings::maxHistoryRough, FieldMeta{ .min = 1.0f, .max = 128.0f, .tooltip = "frames accumulated on rough surfaces, lower ghosts less but is noisier" }),
		member("maxHistorySmooth", &TemporalSettings::maxHistorySmooth, FieldMeta{ .min = 1.0f, .max = 32.0f, .tooltip = "frames accumulated on smooth (mirror like) surfaces" }),
		member("maxHistoryMoving", &TemporalSettings::maxHistoryMoving, FieldMeta{ .min = 1.0f, .max = 64.0f, .tooltip = "cap on the history while a pixel moves" }),
		member("clampGamma", &TemporalSettings::clampGamma, FieldMeta{ .min = 0.25f, .max = 4.0f, .tooltip = "width of the neighborhood box the history is clamped to, lower ghosts less but flickers more" }),
		member("depthTolerance", &TemporalSettings::depthTolerance, FieldMeta{ .min = 0.001f, .max = 0.2f, .format = "%.3f", .tooltip = "relative view depth difference that still counts as the same surface" }),
		member("normalThreshold", &TemporalSettings::normalThreshold, FieldMeta{ .min = 0.5f, .max = 1.0f, .format = "%.3f", .tooltip = "minimum normal dot product that still counts as the same surface" })
	)
};

struct AtrousSettings {
	bool enabled { true };
	int iterations { 3 };
	float depthSigma { 0.02f };
	float normalPower { 64.0f };
	float luminanceSigma { 4.0f };
	float specularRoughnessStart { 0.1f };

	DEFINE_TYPE(AtrousSettings,
		member("enabled", &AtrousSettings::enabled, FieldMeta{ .tooltip = "spatial denoise of the temporal ssgi / ssr result" }),
		member("iterations", &AtrousSettings::iterations, FieldMeta{ .min = 1.0f, .max = 5.0f, .tooltip = "each pass doubles the sample spacing, 3 reaches about 16 pixels" }),
		member("depthSigma", &AtrousSettings::depthSigma, FieldMeta{ .min = 0.001f, .max = 0.2f, .format = "%.3f", .tooltip = "relative view depth change per pixel of distance that still blurs together" }),
		member("normalPower", &AtrousSettings::normalPower, FieldMeta{ .min = 1.0f, .max = 256.0f, .tooltip = "higher keeps creases and edges sharper" }),
		member("luminanceSigma", &AtrousSettings::luminanceSigma, FieldMeta{ .min = 0.1f, .max = 32.0f, .tooltip = "how many local standard deviations of brightness still blur together, higher is smoother" }),
		member("specularRoughnessStart", &AtrousSettings::specularRoughnessStart, FieldMeta{ .min = 0.0f, .max = 1.0f, .tooltip = "reflections below this roughness stay unfiltered so mirrors keep their detail" })
	)
};

struct SkyAtmosphereSettings {
	bool enabled { true };
	float intensity { 1.0f };
	float viewHeight { 0.2f };
	glm::vec4 groundAlbedo { 0.3f, 0.3f, 0.3f, 1.0f };
	float rayleighScale { 1.0f };
	float mieScale { 1.0f };
	float mieAnisotropy { 0.8f };
	bool ozone { true };

	uint32_t transmittance { 0 };
	uint32_t multiScattering { 0 };
	uint32_t environment { 0 };
	glm::vec3 sunTransmittance { 1.0f };	// filled by the sky each frame, not saved

	DEFINE_TYPE(SkyAtmosphereSettings,
		member("enabled", &SkyAtmosphereSettings::enabled, FieldMeta{ .tooltip = "light the scene from the atmosphere instead of the environment map" }),
		member("intensity", &SkyAtmosphereSettings::intensity, FieldMeta{ .min = 0.0f, .max = 10.0f }),
		member("viewHeight", &SkyAtmosphereSettings::viewHeight, FieldMeta{ .min = 0.0f, .max = 100.0f, .tooltip = "km above the ground the sky is seen from" }),
		member("groundAlbedo", &SkyAtmosphereSettings::groundAlbedo, FieldMeta{ .widget = FieldWidget::Color }),
		member("rayleighScale", &SkyAtmosphereSettings::rayleighScale, FieldMeta{ .min = 0.0f, .max = 10.0f }),
		member("mieScale", &SkyAtmosphereSettings::mieScale, FieldMeta{ .min = 0.0f, .max = 10.0f }),
		member("mieAnisotropy", &SkyAtmosphereSettings::mieAnisotropy, FieldMeta{ .min = 0.0f, .max = 0.99f }),
		member("ozone", &SkyAtmosphereSettings::ozone),
		member("transmittance", &SkyAtmosphereSettings::transmittance, FieldMeta{ .widget = FieldWidget::Texture, .readOnly = true }),
		member("multiScattering", &SkyAtmosphereSettings::multiScattering, FieldMeta{ .widget = FieldWidget::Texture, .readOnly = true }),
		member("environment", &SkyAtmosphereSettings::environment, FieldMeta{ .widget = FieldWidget::Texture, .readOnly = true })
	)
};

// the sun as it reaches the scene: the set color, dimmed and tinted by the atmosphere while the sky atmosphere is on.
// every pass that lights with the sun takes it from here so direct light and gi agree
inline glm::vec4 sceneSunColor(const LightingSettings* lighting, const SkyAtmosphereSettings* sky)
{
	if (!lighting) {
		return glm::vec4(0.0f);
	}
	glm::vec4 color = lighting->sunColor * lighting->sunIntensity;
	if (sky && sky->enabled) {
		color *= glm::vec4(sky->sunTransmittance, 1.0f);
	}
	return color;
}

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

struct DLSSSettings {
	bool enabled { true };
	int mode { 1 };
	bool resetHistory { false };

	bool supported { false };
	uint32_t renderWidth { 0 };
	uint32_t renderHeight { 0 };

	DEFINE_TYPE(DLSSSettings,
		member("enabled", &DLSSSettings::enabled),
		member("mode", &DLSSSettings::mode, FieldMeta{ .widget = FieldWidget::Combo,
			.options = "DLAA\0Quality\0Balanced\0Performance\0Ultra Performance\0\0" }),
		member("resetHistory", &DLSSSettings::resetHistory, FieldMeta{ .widget = FieldWidget::Button }),
		member("supported", &DLSSSettings::supported, FieldMeta{ .readOnly = true }),
		member("renderWidth", &DLSSSettings::renderWidth, FieldMeta{ .readOnly = true }),
		member("renderHeight", &DLSSSettings::renderHeight, FieldMeta{ .readOnly = true })
	)
};

struct TonemapSettings {
	int tonemapper { 1 };
	float exposure { 1.0f };
	float gamma { 2.2f };

	DEFINE_TYPE(TonemapSettings,
		member("tonemapper", &TonemapSettings::tonemapper, FieldMeta{ .widget = FieldWidget::Combo,
			.options = "None\0Reinhard\0ACES (Narkowicz)\0ACES (Hill)\0Uncharted 2\0AgX\0Khronos PBR Neutral\0\0" }),
		member("exposure", &TonemapSettings::exposure, FieldMeta{ .min = 0.0f, .max = 8.0f }),
		member("gamma", &TonemapSettings::gamma, FieldMeta{ .min = 1.0f, .max = 3.0f })
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

