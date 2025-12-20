#pragma once

#include "Widget.h"

class EntityControlWidget : public Widget
{
private: 

	bool isSelected = false;
	int id;

public:
	EntityControlWidget() : Widget() {}

	void render() override = 0;

	virtual void scale(glm::mat4& scaleMatrix) = 0;
	virtual void translate(glm::mat4& modelMatrix) = 0;
	virtual void rotate(glm::mat4& rotate) = 0;
};

