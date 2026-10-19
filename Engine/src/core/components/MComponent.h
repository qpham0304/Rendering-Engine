#pragma once

#include "Engine/EngineCoreAPI.h"
#include "core/features/MathIncludes.h"
#include <concepts>
#include <string>
#include <nlohmann/json.hpp>
#include <entt/entt.hpp>
#include "core/features/Serializable.h"

namespace glm {
	inline void to_json(nlohmann::json& j, const glm::vec3& v) {
        j = nlohmann::json{v.x, v.y, v.z};
    }

    inline void from_json(const nlohmann::json& j, glm::vec3& v) {
        j.at(0).get_to(v.x);
        j.at(1).get_to(v.y);
        j.at(2).get_to(v.z);
    }

    inline void to_json(nlohmann::json& j, const glm::vec4& v) {
        j = nlohmann::json{v.x, v.y, v.z, v.w};
    }

    inline void from_json(const nlohmann::json& j, glm::vec4& v) {
        j.at(0).get_to(v.x);
        j.at(1).get_to(v.y);
        j.at(2).get_to(v.z);
        j.at(3).get_to(v.w);
    }

	inline void to_json(nlohmann::json& j, const glm::mat4& m) {
        j = nlohmann::json{m[0], m[1], m[2], m[3]};
    }

    inline void from_json(const nlohmann::json& j, glm::mat4& m) {
        j.at(0).get_to(m[0]);
        j.at(1).get_to(m[1]);
        j.at(2).get_to(m[2]);
		j.at(3).get_to(m[3]);
    }
}


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

	REFLECT_TYPE(TransformComponent,
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

	REFLECT_TYPE(NameComponent,
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
	
	REFLECT_TYPE(ModelComponent,
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

	REFLECT_TYPE(PrefabComponent,
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

    REFLECT_TYPE(SpriteComponent,
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

	REFLECT_TYPE(AnimationComponent,
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

	REFLECT_TYPE(AnimationStateComponent,
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

	REFLECT_TYPE(SkinningComponent,
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

	REFLECT_TYPE(CameraComponent,
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

	REFLECT_TYPE(ScriptComponent,
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

	REFLECT_TYPE(ColliderComponent,
		member("shapeID", &ColliderComponent::shapeID),
		member("colliderType", &ColliderComponent::colliderType),
		member("bodyType", &ColliderComponent::bodyType)
	);
};

struct MYGRAPHICSENGINE_EXPORT ParticleEmitter {
	ParticleEmitter() = default;

	uint32_t containerID;

	int behaviorType{ 0 };
	int emitMax{ 100 };
	int emitCount{ 0 };
	bool areRecycled{ false };
	float emitAccumulator{ 0.0f };
	float emitRate{ 1.0f };
	float lifetimeMin{ 1.0f };
	float lifetimeMax{ 1.0f };
	float speedMin{ 1.0f };
	float speedMax{ 1.0f };
	glm::vec3 spawnPosition{ 0.0f, 0.0f, 0.0f };
	glm::vec3 force{ 0.0f, 0.0f, 0.0f };
	bool resetPosition{ false };

	REFLECT_TYPE(ParticleEmitter,
		member("behaviorType", &ParticleEmitter::behaviorType),
		member("emitMax", &ParticleEmitter::emitMax),
		member("emitCount", &ParticleEmitter::emitCount),
		member("areRecycled", &ParticleEmitter::areRecycled),
		member("emitAccumulator", &ParticleEmitter::emitAccumulator),
		member("emitRate", &ParticleEmitter::emitRate),
		member("lifetimeMin", &ParticleEmitter::lifetimeMin),
		member("lifetimeMax", &ParticleEmitter::lifetimeMax),
		member("speedMin", &ParticleEmitter::speedMin),
		member("speedMax", &ParticleEmitter::speedMax),
		member("spawnPosition", &ParticleEmitter::spawnPosition),
		member("force", &ParticleEmitter::force)
	)
};

