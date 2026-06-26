#pragma once

#include <concepts>
#include <string>
#include <nlohmann/json.hpp>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <entt/entt.hpp>

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

class Component {
public:
	Component() = default;
};

class TransformComponent {
private:
	glm::mat4 modelMatrix = glm::mat4(1.0f);
	bool isDirty = true;

public:
	glm::vec3 translateVec = glm::vec3(0.0f);
	glm::vec3 rotateVec = glm::vec3(0.0f);
	glm::vec3 scaleVec = glm::vec3(1.0f);

	TransformComponent() = default;
	TransformComponent(glm::mat4&& modelMatrix) : modelMatrix(modelMatrix) {
		
	};

	void updateTransform() {
		glm::mat4 rotationMat = glm::toMat4(glm::quat(rotateVec));
		glm::mat4 translateMat = glm::translate(glm::mat4(1.0), translateVec);
		glm::mat4 scaleMat = glm::scale(glm::mat4(1.0), scaleVec);
		modelMatrix = translateMat * rotationMat * scaleMat;
	}

	void translate(const glm::vec3& translate) {
		translateVec = translate;
		updateTransform();
	}

	void rotate(const glm::vec3& rotate) {
		rotateVec = rotate;
		updateTransform();
	}

	void scale(const glm::vec3& scale) {
		scaleVec = scale;
		updateTransform();
	}

	void translate(glm::vec3&& translate) {
		translateVec = translate;
		updateTransform();
	}

	void rotate(glm::vec3&& rotate) {
		rotateVec = rotate;
		updateTransform();
	}

	void scale(glm::vec3&& scale) {
		scaleVec = scale;
		updateTransform();
	}

	glm::mat4& getModelMatrix() {
		return modelMatrix;
	}

	// NLOHMANN_DEFINE_TYPE_INTRUSIVE(TransformComponent, translateVec, rotateVec, scaleVec);

	friend void to_json(nlohmann::json& j, const TransformComponent& t) {
		j = nlohmann::json{
			{"translateVec", {t.translateVec.x, t.translateVec.y, t.translateVec.z}},
			{"rotateVec",    {t.rotateVec.x, t.rotateVec.y, t.rotateVec.z}},
			{"scaleVec",     {t.scaleVec.x, t.scaleVec.y, t.scaleVec.z}}
		};
	}

	friend void from_json(const nlohmann::json& j, TransformComponent& t) {
		auto tr = j.at("translateVec");
		t.translateVec = glm::vec3(tr[0], tr[1], tr[2]);

		auto rt = j.at("rotateVec");
		t.rotateVec = glm::vec3(rt[0], rt[1], rt[2]);

		auto sc = j.at("scaleVec");
		t.scaleVec = glm::vec3(sc[0], sc[1], sc[2]);

		t.updateTransform();
	}

};


struct NameComponent {
public:
	std::string name = "";

	NameComponent() = default;
	NameComponent(const std::string& name) : name(name) {
		
	};

	operator std::string() const noexcept {
		return name;
	}

	NLOHMANN_DEFINE_TYPE_INTRUSIVE(NameComponent, name);
};

struct ModelComponent {
public:
	std::string path = "None";
	uint32_t modelID = 0;

	ModelComponent() = default;
	ModelComponent(std::string_view p) : path(p) {};

	void reset() {
		path = "None";
		modelID = 0;
	}
	
	friend void to_json(nlohmann::json& j, const ModelComponent& m) {
        j = nlohmann::json{{"path", m.path}};
    }

    friend void from_json(const nlohmann::json& j, ModelComponent& m) {
        m.path = j.value("path", "None");
        m.modelID = 0; 
    }

};

struct MeshComponent {
public:
	std::vector<uint32_t> meshIDs = {};

	MeshComponent() = default;
	MeshComponent(std::vector<uint32_t> ids) : meshIDs(ids) {};
	
	// NLOHMANN_DEFINE_TYPE_INTRUSIVE(MeshComponent, meshIDs);
};

struct LightComponent {
public:
	glm::vec4 color;
	float intensity;
	float radius;

	LightComponent() = default;
	LightComponent(glm::vec4 c, float i, float r) : color(c), intensity(i), radius(r) {};

	//NLOHMANN_DEFINE_TYPE_INTRUSIVE(LightComponent, meshIDs);
};

struct RelationshipComponent {
    entt::entity parent{ entt::null };
    std::vector<entt::entity> children;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(RelationshipComponent, parent, children)
};

struct PrefabComponent {
    std::string prefabPath;

    friend void to_json(nlohmann::json& j, const PrefabComponent& p) {
        j = nlohmann::json{ {"prefabPath", p.prefabPath} };
    }
    friend void from_json(const nlohmann::json& j, PrefabComponent& p) {
        j.at("prefabPath").get_to(p.prefabPath);
    }
};

struct RenderTag {
	RenderTag() = default;

	std::vector<std::string> renderers;
};


struct LightProbeComponent {
	LightProbeComponent() = default;

	std::vector<glm::vec4> probeGrid;
	size_t bufferSize;
	uint32_t probesPerDimension;
	float spacing;
    glm::vec4 gridOrigin;
};


struct SpriteComponent {
	SpriteComponent() = default;

	std::string path { "None" };
	std::string targetRenderer { "None" };
	uint32_t textureID { 0 };
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

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(
		SpriteComponent,
		path,
		targetRenderer,
		numRows,
		numCols,
		frameIndex,
		color
	)

	// static void Reflect() {
    //     entt::meta<SpriteComponent>()
    //         .data<&SpriteComponent::path>("Path"_hs)
    //         .data<&SpriteComponent::targetRenderer>("TargetRenderer"_hs);
    // }
};

struct AnimationComponent {
	AnimationComponent() = default;

	int frameCount{ 8 };
	float frameDuration{ 0.0f };
	float frameDelay{ 0.0f };
	bool isRunning{ true };
	bool isLooping{ true };
	bool isDone{ false };

	// NLOHMANN_DEFINE_TYPE_INTRUSIVE(
	// 	AnimationComponent,
	// 	frameDuration,
	// 	frameCount,
	// 	frameDelay,
	// );
};

struct CameraComponent {
	CameraComponent() = default;
	
	int viewWidth;
	int viewHeight;	
	glm::mat4 projection;
	glm::mat4 view;
	glm::vec3 orientation;

	NLOHMANN_DEFINE_TYPE_INTRUSIVE(
		CameraComponent,
		viewWidth,
		viewHeight,
		projection,
		view,
		orientation
	);
};

struct ScriptComponent {
	ScriptComponent() = default;

	std::string path;
	std::function<void(double)> script;

	NLOHMANN_DEFINE_TYPE_INTRUSIVE(ScriptComponent, path);
};
