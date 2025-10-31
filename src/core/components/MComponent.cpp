#include "MComponent.h"

//// dependency on components remove when done
//Component::Component() {
//	normalMatrix = glm::mat4(1.0f);
//	modelMatrix = glm::mat4(1.0f);
//	selected = false;
//	showAxis = false;
//	hasAnimation = false;
//	id = Utils::uuid::get_uuid();
//	name = id;
//
//	// list of shader that implemented with their uniform
//	// allow gui to control them
//	attributes["default"] = { "" };
//}
//
//void Component::setUniform()
//{
//	shaderProgram_ptr->Activate();
//	shaderProgram_ptr->setMat4("matrix", modelMatrix);
//}
//
//Component::Component(const char* path)
//{
//	normalMatrix = glm::mat4(1.0f);
//	modelMatrix = glm::mat4(1.0f);
//	selected = false;
//	showAxis = false;
//	hasAnimation = false;
//	id = Utils::uuid::get_uuid();
//	name = id;
//
//	shaderProgram_ptr.reset(new Shader("Shaders/default.vert", "Shaders/default.frag", "Shaders/default.geom"));
//	//shaderProgram_ptr.reset(new Shader("Shaders/default-2.vert", "Shaders/default-2.frag"));
//	model_ptr.reset(new Model(path));
//	const char* fileName = std::strrchr(path, '/');
//	name = fileName + 1;
//}
//
//Component::Component(Component&& other) noexcept
//	: selected(other.selected),
//	showAxis(other.showAxis),
//	hasAnimation(other.hasAnimation),
//	countVertices(other.countVertices),
//	deltaTime(other.deltaTime),
//	id(std::move(other.id)),
//	attributes(std::move(other.attributes)),
//	name(std::move(other.name)),
//	animation_ptr(std::move(other.animation_ptr)),
//	animator_ptr(std::move(other.animator_ptr)),
//	shaderProgram_ptr(std::move(other.shaderProgram_ptr)),
//	modelMatrix(std::move(other.modelMatrix)),
//	normalMatrix(std::move(other.normalMatrix)),
//	model_ptr(std::move(other.model_ptr)),
//	scaleVector(std::move(other.scaleVector)),
//	translateVector(std::move(other.translateVector)),
//	rotationVector(std::move(other.rotationVector)),
//	material(std::move(other.material)),
//	materialPBR(std::move(other.materialPBR))
//{
//	std::cout << "component " << id << " moved\n";
//}
//
//Component& Component::operator=(Component&& other) noexcept
//{
//	if (this != &other) {
//		selected = other.selected;
//		showAxis = other.showAxis;
//		hasAnimation = other.hasAnimation;
//		countVertices = other.countVertices;
//		deltaTime = other.deltaTime;
//		id = std::move(other.id);
//		attributes = std::move(other.attributes);
//		name = std::move(other.name);
//		animation_ptr = std::move(other.animation_ptr);
//		animator_ptr = std::move(other.animator_ptr);
//		shaderProgram_ptr = std::move(other.shaderProgram_ptr);
//		modelMatrix = std::move(other.modelMatrix);
//		normalMatrix = std::move(other.normalMatrix);
//		model_ptr = std::move(other.model_ptr);
//		scaleVector = std::move(other.scaleVector);
//		translateVector = std::move(other.translateVector);
//		rotationVector = std::move(other.rotationVector);
//		material = std::move(other.material);
//		materialPBR = std::move(other.materialPBR);
//	}
//	std::cout << "component " << other.id << " operator moved\n";
//	return *this;
//}
//
//void Component::renderShadow(Shader& shader)
//{
//	shader.setMat4("matrix", modelMatrix);
//	shader.setBool("hasAnimation", hasAnimation);
//	if (hasAnimation) {
//		auto aru_transforms_shadow = animator_ptr->GetFinalBoneMatrices();
//		for (int i = 0; i < aru_transforms_shadow.size(); ++i)
//			shader.setMat4("finalBonesMatrices[" + std::to_string(i) + "]", aru_transforms_shadow[i]);
//		model_ptr->Draw(shader);
//	}
//	else
//		model_ptr->Draw(shader);
//}
//
//void Component::loadAnimation(const char* path)
//{
//	hasAnimation = true;
//	animation_ptr.reset(new Animation(path, model_ptr.get()));
//	animator_ptr.reset(new Animator(animation_ptr.get()));
//}
//
//void Component::updateAnimation(float deltaTime)
//{
//	this->deltaTime = deltaTime;
//	animator_ptr->UpdateAnimation(deltaTime);
//}
//
//void Component::translate(glm::vec3 translate)
//{
//	translateVector = translate;
//	modelMatrix = glm::translate(glm::mat4(1.0f), translate);
//	rotate(rotationVector);
//	modelMatrix = glm::scale(modelMatrix, scaleVector);
//}
//
//bool Component::canAnimate()
//{
//	return hasAnimation;
//}
//
//glm::mat4  Component::getTransform()
//{
//	//glm::mat4 translateMatrix = glm::translate(glm::mat4(1.0f), translateVector);
//	//glm::mat4 rotationMatrix = glm::toMat4(glm::quat(rotationVector));
//	//glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), scaleVector);
//	//modelMatrix = translateMatrix * rotationMatrix * scaleMatrix;
//
//	modelMatrix = glm::translate(glm::mat4(1.0f), translateVector);
//	glm::mat4 rot = glm::toMat4(glm::quat(rotationVector));
//	modelMatrix *= rot;
//	modelMatrix = glm::scale(modelMatrix, scaleVector);
//
//	return modelMatrix;
//}
//
//void Component::rotate(glm::vec3 rotate)
//{
//	//TODO: broken if used with slider but guizmo works fine, fix if have time
//	//rotationVector = rotate;
//	modelMatrix = glm::translate(glm::mat4(1.0f), translateVector);
//	glm::mat4 rot = glm::toMat4(glm::quat(rotationVector));
//	modelMatrix *= rot;
//	modelMatrix = glm::scale(modelMatrix, scaleVector);
//}
//
//void Component::scale(glm::vec3 scale)
//{
//	scaleVector = scale;
//	modelMatrix = glm::translate(translateVector);
//	rotate(rotationVector);
//	modelMatrix = glm::scale(modelMatrix, scale);
//}
//
//glm::mat4 Component::getModelMatrix()
//{
//	return modelMatrix;
//}
//
//glm::mat4 Component::getNormalMatrix()
//{
//	return normalMatrix;
//}
//
//bool Component::getShowAxisState()
//{
//	return showAxis;
//}
//
//void Component::select()
//{
//	selected = true;
//}
//
//void Component::unSelect()
//{
//	selected = false;
//}
//
//int Component::getNumVertices()
//{
//	return countVertices;
//}
//
//bool Component::isSelected()
//{
//	return selected;
//}
//
//std::string Component::getID()
//{
//	return id;
//}
//
//std::string Component::getName()
//{
//	return name;
//}
