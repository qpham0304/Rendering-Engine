#pragma once

#include "Engine/EngineCoreAPI.h"
#include "core/features/MathIncludes.h"
#include <concepts>
#include <string>
#include <nlohmann/json.hpp>
#include <entt/entt.hpp>
#include "core/features/Serializable.h"
#include "core/features/MathJson.h"


class MYGRAPHICSENGINE_EXPORT TransformComponent {
private:
	glm::mat4 modelMatrix = glm::mat4(1.0f);

public:
	glm::vec3 translateVec = glm::vec3(0.0f);
	glm::vec3 rotateVec = glm::vec3(0.0f);
	glm::vec3 scaleVec = glm::vec3(1.0f);

	TransformComponent() = default;
	TransformComponent(glm::mat4&& modelMatrix) : modelMatrix(modelMatrix) {};

	glm::mat4& getModelMatrix() {
		return modelMatrix;
	}

	void updateTransform() {
		glm::mat4 rotationMat = glm::toMat4(glm::quat(rotateVec));
		glm::mat4 translateMat = glm::translate(glm::mat4(1.0), translateVec);
		glm::mat4 scaleMat = glm::scale(glm::mat4(1.0), scaleVec);
		modelMatrix = translateMat * rotationMat * scaleMat;
	}

	void translate(glm::vec3 translate) {
		translateVec = translate;
		updateTransform();
	}

	void rotate(glm::vec3 rotate) {
		rotateVec = rotate;
		updateTransform();
	}

	void scale(glm::vec3 scale) {
		scaleVec = scale;
		updateTransform();
	}

	// called by from_json after the fields are loaded
	void onDeserialized() {
		updateTransform();
	}

	DEFINE_TYPE(TransformComponent,
		member("translateVec", &TransformComponent::translateVec),
		member("rotateVec", &TransformComponent::rotateVec),
		member("scaleVec", &TransformComponent::scaleVec)
	);

};

struct MYGRAPHICSENGINE_EXPORT NameComponent {
public:
	std::string name = "None";

	NameComponent() = default;
	NameComponent(const std::string& name) : name(name) {
		
	};

	operator std::string() const noexcept {
		return name;
	}

	DEFINE_TYPE(NameComponent,
		member("name", &NameComponent::name)
	);
};

struct MYGRAPHICSENGINE_EXPORT ModelComponent {
public:
	std::string path = "None";
	uint32_t modelID = 0;

	ModelComponent() = default;
	ModelComponent(std::string_view p) : path(p) {};
	ModelComponent(std::string_view p, uint32_t id) : path(p), modelID(id) {};

	void reset() {
		path = "None";
		modelID = 0;
	}
	
	DEFINE_TYPE(ModelComponent,
		member("path", &ModelComponent::path)
	);

};

struct MYGRAPHICSENGINE_EXPORT MeshComponent {
public:
	std::vector<uint32_t> meshIDs = {};

	MeshComponent() = default;
	MeshComponent(std::vector<uint32_t> ids) : meshIDs(ids) {};
	
	// NLOHMANN_DEFINE_TYPE_INTRUSIVE(MeshComponent, meshIDs);
};

struct MYGRAPHICSENGINE_EXPORT LightComponent {
public:
	glm::vec4 color;
	float intensity;
	float radius;

	LightComponent() = default;
	LightComponent(glm::vec4 c, float i, float r) : color(c), intensity(i), radius(r) {};

	//NLOHMANN_DEFINE_TYPE_INTRUSIVE(LightComponent, meshIDs);
};

struct MYGRAPHICSENGINE_EXPORT RelationshipComponent {
    entt::entity parent{ entt::null };
    std::vector<entt::entity> children;
};

struct MYGRAPHICSENGINE_EXPORT PrefabComponent {
    std::string prefabPath;

	DEFINE_TYPE(PrefabComponent,
		member("prefabPath", &PrefabComponent::prefabPath)
	);
};

struct MYGRAPHICSENGINE_EXPORT RenderTag {
	RenderTag() = default;

	std::vector<std::string> renderers;
};

struct MYGRAPHICSENGINE_EXPORT LightProbeComponent {
	LightProbeComponent() = default;

	std::vector<glm::vec4> probeGrid;
	size_t bufferSize;
	uint32_t probesPerDimension;
	glm::uvec3 probeCounts { 0 };	// probes per axis, falls back to probesPerDimension on every axis when left at 0
	float spacing;
    glm::vec4 gridOrigin;

	//NLOHMANN_DEFINE_TYPE_INTRUSIVE(LightProbeComponent, probeGrid);
};

struct MYGRAPHICSENGINE_EXPORT SpriteComponent {
	SpriteComponent() = default;

	uint32_t textureID { 0 };
	glm::vec2 uvScale { 1.0, 1.0 };
	glm::vec2 uvOffset { 0.0 };

	std::string path { "None" };
	std::string targetRenderer { "None" };
	int numRows { 1 };
	int numCols { 1 };
	int frameIndex { 0 };
	glm::vec4 color { 1.0 };

	void setFrame(int frame) {
		unsigned int frameCount = numRows * numCols;
		if (frame < frameCount) {
			frameIndex = frame;
		}
	}

    DEFINE_TYPE(SpriteComponent,
		member("path", &SpriteComponent::path),
		member("targetRenderer", &SpriteComponent::targetRenderer),
		member("numRows", &SpriteComponent::numRows),
		member("numCols", &SpriteComponent::numCols),
		member("frameIndex", &SpriteComponent::frameIndex),
		member("color", &SpriteComponent::color)
	)
};

struct MYGRAPHICSENGINE_EXPORT AnimationComponent {
	AnimationComponent() = default;

	int frameCount{ 8 };
	float frameDuration{  1.0f / 24.0f };
	float frameDelay{ 1.0f / 24.0f };
	bool isRunning{ true };
	bool isLooping{ true };

	DEFINE_TYPE(AnimationComponent,
		member("frameCount", &AnimationComponent::frameCount),
		member("frameDuration", &AnimationComponent::frameDuration),
		member("frameDelay", &AnimationComponent::frameDelay),
		member("isLooping", &AnimationComponent::isLooping)
	);
};

struct MYGRAPHICSENGINE_EXPORT AnimationStateComponent {
	AnimationStateComponent() = default;

	std::string texturePath{ "None" };
	int startFrame{ 1 };
	int frameCount{ 1 };
	float frameDuration{ 0.0f };
	bool isLooping{ true };

	DEFINE_TYPE(AnimationStateComponent,
		member("texturePath", &AnimationStateComponent::texturePath),
		member("startFrame", &AnimationStateComponent::startFrame),
		member("frameCount", &AnimationStateComponent::frameCount),
		member("frameDuration", &AnimationStateComponent::frameDuration),
		member("isLooping", &AnimationStateComponent::isLooping)
	)
};

struct MYGRAPHICSENGINE_EXPORT SkinningComponent {
	SkinningComponent() = default;

	uint32_t boneMatricesID { 0 };
	uint32_t animatorID { 0 };
	std::string animationPath { "None" };
	std::string initialAnimation { "None" };

	DEFINE_TYPE(SkinningComponent,
		member("initialAnimation", &SkinningComponent::initialAnimation),
		member("animationPath", &SkinningComponent::animationPath)
	);
};

struct MYGRAPHICSENGINE_EXPORT CameraComponent {
	CameraComponent() = default;
	
	int viewWidth;
	int viewHeight;	
	glm::mat4 projection;
	glm::mat4 view;
	glm::vec3 orientation;

	DEFINE_TYPE(CameraComponent,
		member("view", &CameraComponent::view),
		member("orientation", &CameraComponent::orientation)
	);
};

struct MYGRAPHICSENGINE_EXPORT ScriptComponent {
	ScriptComponent() = default;
	ScriptComponent(std::string scriptPath) : path(scriptPath) {};

	std::string path { "None" };
	std::function<void()> onInit = []() {};
    std::function<void(double)> onUpdate = [](double) {};
    std::function<void(double)> onFixedUpdate = [](double) {};
    std::function<void()> onDestroy = []() {};
	bool initilized { false };

	DEFINE_TYPE(ScriptComponent,
		member("path", &ScriptComponent::path)
	);
};

struct MYGRAPHICSENGINE_EXPORT ColliderComponent {
	ColliderComponent() = default;
	ColliderComponent(uint32_t id, uint32_t type) : shapeID(id), colliderType(type) {}
	
	uint32_t shapeID { 0 };
	std::string bodyType = "None";
	
	bool isActive { true };
	uint32_t colliderType { 0 };

	float mass { 1.0 };
	glm::vec3 center{ glm::vec3(0.0) };
	glm::mat3 inertia{ glm::mat3(1.0) };

	DEFINE_TYPE(ColliderComponent,
		member("shapeID", &ColliderComponent::shapeID),
		member("colliderType", &ColliderComponent::colliderType),
		member("bodyType", &ColliderComponent::bodyType)
	);
};

// TODO: currently match shaders/common/particles.glsl defines, make shared struct for this
enum class ParticleSpawnShape : int { Point = 0, Box, Sphere, Hemisphere, Cone, Circle, Count };
enum class ParticleRenderMode : int { Billboard = 0, AxisLockedY, VelocityStretched, Horizontal, Oriented, Mesh, Count };
enum class ParticleBlendMode : int { Additive = 0, Alpha, Count };
enum class ParticleTextureSource : int { None = 0, Emitter, Sprite, Model };	// where the drawn texture came from, highest priority first after None

struct MYGRAPHICSENGINE_EXPORT ParticleEmitter {
	ParticleEmitter() = default;

	uint32_t containerID { 0 };			// created by the particle renderer when left at 0

	// emission
	bool isPlaying { true };
	int maxParticles { 1000 };			// container capacity, the container is recreated when this changes
	float emitRate { 100.0f };			// particles per second
	int burstCount { 0 };				// particles spawned at once every burstInterval
	float burstInterval { 1.0f };		// seconds, if <= 0 only bursts on request
	bool localSpace { false };			// particles move with the emitter transform, forces act in emitter space

	// spawn shape, placed and oriented by the entity transform
	int spawnShape { 0 };				// ParticleSpawnShape
	glm::vec3 shapeExtents { 1.0f };	// box half extents
	float shapeRadius { 1.0f };
	float shapeConeAngle { 25.0f };		// degrees
	bool emitFromShell { false };		// spawn on the surface instead of inside the volume

	// initial velocity
	glm::vec3 direction { 0.0f, 1.0f, 0.0f };
	float spreadAngle { 15.0f };		// degrees around direction
	bool velocityFromShape { false };	// push outward from the shape instead of along direction
	float speedMin { 1.0f };
	float speedMax { 2.0f };

	float lifetimeMin { 1.0f };
	float lifetimeMax { 2.0f };

	// forces
	float gravityScale { 0.0f };
	glm::vec3 force { 0.0f };
	float drag { 0.0f };
	float turbulenceStrength { 0.0f };
	float turbulenceFrequency { 1.0f };
	float vortexStrength { 0.0f };		// swirl around the emitter up axis
	float attractStrength { 0.0f };		// pull toward the emitter, negative pushes away

	// size and rotation over life
	float sizeMin { 0.1f };
	float sizeMax { 0.1f };
	float sizeEndScale { 1.0f };		// size multiplier reached at the end of life
	float rotationMin { 0.0f };			// degrees
	float rotationMax { 0.0f };
	float angularVelocityMin { 0.0f };	// degrees per second
	float angularVelocityMax { 0.0f };

	// color over life
	glm::vec4 colorStart { 1.0f };
	glm::vec4 colorEnd { 1.0f, 1.0f, 1.0f, 0.0f };
	float fadeIn { 0.0f };				// fraction of the lifetime
	float fadeOut { 0.0f };

	// rendering
	int renderMode { 0 };
	int blendMode { 0 };
	float stretchFactor { 0.1f };		// extra length per unit of speed for velocity stretched quads
	std::string texturePath { "None" };
	int flipbookRows { 1 };
	int flipbookCols { 1 };
	float flipbookFPS { 0.0f };				// 0 plays the sheet once over the lifetime
	std::string meshPath { "$prim$Cube" };	// model path or primitive, first mesh is drawn
	float meshShading { 0.5f };
	bool meshDoubleSided { false };		// draw back faces too, for open meshes or models with flipped winding
	bool useModelTexture { true };		// mesh particles fall back to the model's albedo texture when no texture is set

	// runtime state, not serialized
	uint32_t textureID { 0 };
	uint32_t activeTextureID { 0 };		// written by the renderer, the texture actually drawn
	int activeTextureSource { 0 };		// ParticleTextureSource of activeTextureID
	uint32_t meshModelID { 0 };
	std::string loadedTexturePath { "None" };
	std::string loadedMeshPath { "None" };
	float emitAccumulator { 0.0f };
	float burstTimer { 0.0f };
	int pendingBurst { 0 };
	bool resetRequested { false };

	DEFINE_TYPE(ParticleEmitter,
		member("isPlaying", &ParticleEmitter::isPlaying),
		member("maxParticles", &ParticleEmitter::maxParticles),
		member("emitRate", &ParticleEmitter::emitRate),
		member("burstCount", &ParticleEmitter::burstCount),
		member("burstInterval", &ParticleEmitter::burstInterval),
		member("localSpace", &ParticleEmitter::localSpace),
		member("spawnShape", &ParticleEmitter::spawnShape),
		member("shapeExtents", &ParticleEmitter::shapeExtents),
		member("shapeRadius", &ParticleEmitter::shapeRadius),
		member("shapeConeAngle", &ParticleEmitter::shapeConeAngle),
		member("emitFromShell", &ParticleEmitter::emitFromShell),
		member("direction", &ParticleEmitter::direction),
		member("spreadAngle", &ParticleEmitter::spreadAngle),
		member("velocityFromShape", &ParticleEmitter::velocityFromShape),
		member("speedMin", &ParticleEmitter::speedMin),
		member("speedMax", &ParticleEmitter::speedMax),
		member("lifetimeMin", &ParticleEmitter::lifetimeMin),
		member("lifetimeMax", &ParticleEmitter::lifetimeMax),
		member("gravityScale", &ParticleEmitter::gravityScale),
		member("force", &ParticleEmitter::force),
		member("drag", &ParticleEmitter::drag),
		member("turbulenceStrength", &ParticleEmitter::turbulenceStrength),
		member("turbulenceFrequency", &ParticleEmitter::turbulenceFrequency),
		member("vortexStrength", &ParticleEmitter::vortexStrength),
		member("attractStrength", &ParticleEmitter::attractStrength),
		member("sizeMin", &ParticleEmitter::sizeMin),
		member("sizeMax", &ParticleEmitter::sizeMax),
		member("sizeEndScale", &ParticleEmitter::sizeEndScale),
		member("rotationMin", &ParticleEmitter::rotationMin),
		member("rotationMax", &ParticleEmitter::rotationMax),
		member("angularVelocityMin", &ParticleEmitter::angularVelocityMin),
		member("angularVelocityMax", &ParticleEmitter::angularVelocityMax),
		member("colorStart", &ParticleEmitter::colorStart),
		member("colorEnd", &ParticleEmitter::colorEnd),
		member("fadeIn", &ParticleEmitter::fadeIn),
		member("fadeOut", &ParticleEmitter::fadeOut),
		member("renderMode", &ParticleEmitter::renderMode),
		member("blendMode", &ParticleEmitter::blendMode),
		member("stretchFactor", &ParticleEmitter::stretchFactor),
		member("texturePath", &ParticleEmitter::texturePath),
		member("flipbookRows", &ParticleEmitter::flipbookRows),
		member("flipbookCols", &ParticleEmitter::flipbookCols),
		member("flipbookFPS", &ParticleEmitter::flipbookFPS),
		member("meshPath", &ParticleEmitter::meshPath),
		member("meshShading", &ParticleEmitter::meshShading),
		member("meshDoubleSided", &ParticleEmitter::meshDoubleSided),
		member("useModelTexture", &ParticleEmitter::useModelTexture)
	)
};

// where a ribbon's points come from
enum class RibbonMode : int { Trail = 0, Beam, Lightning, Blade, Count };

// how the texture runs along a ribbon, Tile repeats per unit of length, Stretch fits it once over the whole length
enum class RibbonUVMode : int { Tile = 0, Stretch, Count };

// a camera facing strip through a list of points, drawn after particles with the same blend modes
struct MYGRAPHICSENGINE_EXPORT RibbonComponent {
	RibbonComponent() = default;

	int mode { 0 };
	bool isPlaying { true };

	// look along the length, start is the entity end (trail head, beam and bolt origin)
	float widthStart { 0.2f };
	float widthEnd { 0.0f };
	glm::vec4 colorStart { 1.0f };
	glm::vec4 colorEnd { 1.0f, 1.0f, 1.0f, 0.0f };
	int blendMode { 0 };				// ParticleBlendMode
	std::string texturePath { "None" };
	float textureTiling { 1.0f };		// texture repeats per unit of length, or over the whole length when stretched
	float textureScroll { 0.0f };		// texture lengths per second, along the ribbon
	int uvMode { 0 };					// RibbonUVMode

	// trail: the entity's recent positions
	float trailLifetime { 0.5f };		// seconds a recorded point lasts
	float minSegmentLength { 0.05f };	// a new point is recorded once the entity moved this far
	int maxPoints { 64 };

	// beam and lightning: from the entity to a target
	glm::vec3 target { 0.0f, 5.0f, 0.0f };
	bool targetInLocalSpace { true };	// target moves and rotates with the entity
	int segments { 16 };

	// lightning
	float jitter { 0.15f };				// sideways displacement as a fraction of the bolt length
	float regenerateInterval { 0.06f };	// seconds between new bolt shapes
	int branches { 2 };
	float branchLength { 0.35f };		// fraction of the bolt length
	float flicker { 0.4f };				// brightness variation between bolt shapes

	// blade: the swept surface between two points on the entity, uses the trail lifetime and max points
	glm::vec3 bladeBase { 0.0f, 0.0f, 0.0f };	// local space, usually the hilt end of the blade
	glm::vec3 bladeTip { 0.0f, 1.0f, 0.0f };	// local space
	int subdivisions { 4 };						// smoothed points added between recorded frames, fast swings stay round

	// runtime state, not serialized
	struct TrailPoint {
		glm::vec3 position;
		float age;
	};
	struct BladeSample {
		glm::vec3 base;
		glm::vec3 tip;
		float age;
	};
	std::vector<TrailPoint> trailPoints;
	std::vector<BladeSample> bladeSamples;
	std::vector<std::vector<glm::vec3>> boltStrips;		// main bolt first, then branches
	float regenerateTimer { 0.0f };
	float boltBrightness { 1.0f };
	uint32_t seed { 1 };
	uint32_t textureID { 0 };
	std::string loadedTexturePath { "None" };

	DEFINE_TYPE(RibbonComponent,
		member("mode", &RibbonComponent::mode),
		member("isPlaying", &RibbonComponent::isPlaying),
		member("widthStart", &RibbonComponent::widthStart),
		member("widthEnd", &RibbonComponent::widthEnd),
		member("colorStart", &RibbonComponent::colorStart),
		member("colorEnd", &RibbonComponent::colorEnd),
		member("blendMode", &RibbonComponent::blendMode),
		member("texturePath", &RibbonComponent::texturePath),
		member("textureTiling", &RibbonComponent::textureTiling),
		member("textureScroll", &RibbonComponent::textureScroll),
		member("uvMode", &RibbonComponent::uvMode),
		member("trailLifetime", &RibbonComponent::trailLifetime),
		member("minSegmentLength", &RibbonComponent::minSegmentLength),
		member("maxPoints", &RibbonComponent::maxPoints),
		member("target", &RibbonComponent::target),
		member("targetInLocalSpace", &RibbonComponent::targetInLocalSpace),
		member("segments", &RibbonComponent::segments),
		member("jitter", &RibbonComponent::jitter),
		member("regenerateInterval", &RibbonComponent::regenerateInterval),
		member("branches", &RibbonComponent::branches),
		member("branchLength", &RibbonComponent::branchLength),
		member("flicker", &RibbonComponent::flicker),
		member("bladeBase", &RibbonComponent::bladeBase),
		member("bladeTip", &RibbonComponent::bladeTip),
		member("subdivisions", &RibbonComponent::subdivisions)
	)
};

