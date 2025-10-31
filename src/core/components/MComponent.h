#pragma once

#include<glm/glm.hpp>
#include<string>
#include "../../graphics/utils/Utils.h"
#include "model.h"
#include "Animation.h"
#include "Animator.h"

/*
			oldSystem
*/
//struct Material {
//	glm::vec3 ambient;
//	glm::vec3 diffuse;
//	glm::vec3 specular;
//	float shininess;
//
//	Material(const glm::vec3 ambient, const glm::vec3 diffuse, const glm::vec3 specular, const float shininess) {
//		this->ambient = ambient;
//		this->diffuse = diffuse;
//		this->specular = specular;
//		this->shininess = shininess;
//	}
//};
//
//struct MaterialPBR {
//	glm::vec3 albedo;
//	float metalic;
//	float roughness;
//	float ao;
//
//	MaterialPBR(const glm::vec3 albedo, const float metalic, const float roughness, const float ao) {
//		this->albedo = albedo;
//		this->metalic = metalic;
//		this->roughness = roughness;
//		this->ao = ao;
//	}
//};
//
//
//
//
//class Component
//{
//
//private:
//
//protected:
//	bool selected;
//	bool showAxis;
//	bool hasAnimation;
//	int countVertices = 0;
//	float deltaTime = 0;
//	std::string id;			// better use true uuid but good enough for now
//	std::unordered_map<std::string, std::vector<std::string>> attributes;
//	std::string name;
//	std::unique_ptr<Animation> animation_ptr;
//	std::unique_ptr<Animator> animator_ptr;
//
//	std::unique_ptr<Shader> shaderProgram_ptr;
//	glm::mat4 modelMatrix;
//	glm::mat3 normalMatrix;
//
//public:
//	Component();
//	Component(const char* path);
//	Component(Component&& other) noexcept;
//	Component& operator=(Component&& other) noexcept;
//	~Component() = default;
//
//	std::unique_ptr<Model> model_ptr;
//
//	glm::vec3 scaleVector = glm::vec3(1.0f, 1.0f, 1.0f);
//	glm::vec3 translateVector = glm::vec3(0.0f, 0.0f, 0.0f);
//	glm::vec3 rotationVector = glm::vec3(0.0f, 0.0f, 0.0f);
//
//	Material material = Material(
//		glm::vec3(1.0f, 1.0f, 1.0f),
//		glm::vec3(0.5f, 0.5f, 0.5f),
//		glm::vec3(0.5f, 0.5f, 0.5f),
//		32.0f
//	);
//
//	MaterialPBR materialPBR = MaterialPBR(glm::vec3(0.5, 0.5, 0.5), 1.0, 1.0, 1.0);
//
//
//	// renderer
//	virtual void setUniform();
//	//virtual void renderPBR(Camera& camera, const Light& light, const UniformProperties& uniforms, const std::vector<Light*> lights);
//	//virtual void render(Camera& camera, const Light& light, const UniformProperties& uniforms);
//	virtual void renderShadow(Shader& shader);
//	virtual void loadAnimation(const char* path);
//	virtual void updateAnimation(float deltaTime);
//
//	// getter
//	int getNumVertices();
//	glm::mat4 getModelMatrix();
//	glm::mat4 getNormalMatrix();
//	bool getShowAxisState();
//	bool isSelected();
//	std::string getID();
//	std::string getName();
//	bool canAnimate();
//
//	// setter
//	void select();
//	void unSelect();
//	void translate(glm::vec3 translate);
//	void rotate(glm::vec3 matrix);
//	void scale(glm::vec3 scale);
//	glm::mat4 getTransform();
//};
//
//



/*


new system start from here

*/
class Component {
public:
	Component() = default;
};

class TransformComponent {
private:
	glm::mat4 modelMatrix = glm::mat4(1.0f);

public:
	glm::vec3 translateVec = glm::vec3(0.0f);
	glm::vec3 rotateVec = glm::vec3(0.0f);
	glm::vec3 scaleVec = glm::vec3(1.0f);

	TransformComponent() = default;
	TransformComponent(glm::mat4&& modelMatrix) : modelMatrix(modelMatrix) {
		
	};

	void updateTransform() {
		//glm::mat4 rotationMat = glm::rotate(glm::mat4(1.0f), rotateVec.x, glm::vec3(1.0, 0.0, 0.0))
		//						* glm::rotate(glm::mat4(1.0f), rotateVec.y, glm::vec3(0.0, 1.0, 0.0))
		//						* glm::rotate(glm::mat4(1.0f), rotateVec.z, glm::vec3(0.0, 0.0, 1.0));
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
};

struct ShaderComponent {
	std::string path;

	ShaderComponent(const std::string&& path) : path(path) {

	};
};

struct NameComponent {
public:
	std::string name = "";

	NameComponent() = default;
	NameComponent(const std::string& name) : name(name) {
		
	};
};

struct ModelComponent {
public:
	std::string path = "None";
	std::weak_ptr<Model> model;

	ModelComponent() = default;
	ModelComponent(std::string&& path, std::shared_ptr<Model> model) : path(path), model(model) {};
	ModelComponent(std::string&& path) : path(path) {};

	void reset() {
		path = "None";
		model.reset();
	}
};

struct AnimationComponent {
public:
	std::string path = "None";
	std::weak_ptr<Animation> animation;
	Animator animator;

	AnimationComponent() = default;
	AnimationComponent(std::string&& path, std::shared_ptr<Animation> animation) : path(path), animation(animation) {
		if (animation) {
			animator.Init(animation.get());
		}
	};
	AnimationComponent(std::string&& path) : path(path) {};

	void reset() {
		path = "None";
		animation.reset();
	}
};

//struct AnimatorComponent {
//public:
//	std::string path;
//	std::weak_ptr<Animator> animation;
//	AnimatorComponent() = default;
//	AnimatorComponent(std::string&& path) : path(path) {};
//};
enum LightType { POINT_LIGHT, DIRECTION_LIGHT, SPOT_LIGHT, AREA_LIGHT };

struct MLightComponent {

public:
	glm::vec3 color = glm::vec3(1.0);
	glm::vec3 position = glm::vec3(0.0);
	float radius;
	LightType type = POINT_LIGHT;

	MLightComponent() = default;
	MLightComponent(glm::vec3&& color, glm::vec3&& position) : color(color), position(position) {};
	MLightComponent(const glm::vec3& color, const glm::vec3& position, LightType type = POINT_LIGHT) 
		: color(color), position(position), type(type) {};
};

struct LayerTagComponent {
public:
	std::string layerName = "";

	LayerTagComponent () = default;
	LayerTagComponent (const std::string& layerName) : layerName(layerName) {

	};
};