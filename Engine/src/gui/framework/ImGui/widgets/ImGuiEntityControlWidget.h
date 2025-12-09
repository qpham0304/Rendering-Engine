#pragma once

#include "../../src/gui/widgets/EntityControlWidget.h"

class ImGuiEntityControlWidget : public EntityControlWidget
{
	ImGuiEntityControlWidget();
	void render() override;

	virtual void scale(glm::mat4& scaleMatrix) = 0;
	virtual void translate(glm::mat4& modelMatrix) = 0;
	virtual void rotate(glm::mat4& rotate) = 0;
};

