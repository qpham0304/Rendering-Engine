#pragma once

#include "graphics/framework/OpenGL/core/ShaderOpenGL.h"
#include "core/layers/Layer.h"
#include "graphics/framework/OpenGL/renderers/BloomRenderer.h"

class BloomPass : public Layer
{
private:
	unsigned int VAO, VBO;
	unsigned int hdrFBO;
	unsigned int colorBuffers[2];
	unsigned int rboDepth;
	unsigned int attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
	std::unique_ptr<ShaderOpenGL> bloomShader;
	BloomRenderer bloomRenderer;

public:
	BloomPass(const std::string& name);
	~BloomPass() = default;

	void onAttach(LayerManager* manager) override;
	void onDetach() override;
	void onUpdate() override;
	void onGuiUpdate() override;
	void onEvent(Event& event) override;
};

