#pragma once

#include <nlohmann/json.hpp>
#include <glm/glm.hpp>
#include <concepts>
#include <string>
#include "animation/Animation.h" 	//TODO: resolve dependency between
#include "animation/Animator.h" 	// animation and animator class

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
	ModelComponent(std::string_view path) : path(path) {};

	void reset() {
		path = "None";
	}
	
	// NLOHMANN_DEFINE_TYPE_INTRUSIVE(ModelComponent, path, modelID);

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
	uint32_t meshID = 0;

	MeshComponent() = default;
	MeshComponent(uint32_t id) : meshID(id) {};
	
	NLOHMANN_DEFINE_TYPE_INTRUSIVE(MeshComponent, meshID);
};

struct RelationshipComponent {
    entt::entity parent{ entt::null };
    std::vector<entt::entity> children;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(RelationshipComponent, parent, children)
};

#include <string>

struct PrefabComponent {
    std::string prefabPath;

    friend void to_json(nlohmann::json& j, const PrefabComponent& p) {
        j = nlohmann::json{ {"prefabPath", p.prefabPath} };
    }
    friend void from_json(const nlohmann::json& j, PrefabComponent& p) {
        j.at("prefabPath").get_to(p.prefabPath);
    }
};

// struct ShaderComponent {
// 	std::string path;

// 	ShaderComponent(const std::string&& path) : path(path) {

// 	};
// };

// struct AnimationComponent {
// public:
// 	std::string path = "None";
// 	std::weak_ptr<Animation> animation;
// 	Animator animator;

// 	AnimationComponent() = default;
// 	AnimationComponent(std::string&& path, std::shared_ptr<Animation> animation) : path(path), animation(animation) {
// 		if (animation) {
// 			animator.Init(animation.get());
// 		}
// 	};
// 	AnimationComponent(std::string&& path) : path(path) {};

// 	void reset() {
// 		path = "None";
// 		animation.reset();
// 	}
// };

//struct AnimatorComponent {
//public:
//	std::string path;
//	std::weak_ptr<Animator> animation;
//	AnimatorComponent() = default;
//	AnimatorComponent(std::string&& path) : path(path) {};
//};

// enum LightType { POINT_LIGHT, DIRECTION_LIGHT, SPOT_LIGHT, AREA_LIGHT };

// struct MLightComponent {

// public:
// 	glm::vec3 color = glm::vec3(1.0f);
// 	glm::vec3 position = glm::vec3(0.0f);
// 	float radius = 0.0f;
// 	LightType type = POINT_LIGHT;

// 	MLightComponent() = default;
// 	MLightComponent(glm::vec3&& color, glm::vec3&& position) : color(color), position(position) {};
// 	MLightComponent(const glm::vec3& color, const glm::vec3& position, LightType type = POINT_LIGHT) 
// 		: color(color), position(position), type(type) {};
// };

// struct LayerTagComponent {
// public:
// 	std::string layerName = "";

// 	LayerTagComponent () = default;
// 	LayerTagComponent (const std::string& layerName) : layerName(layerName) {

// 	};
// };