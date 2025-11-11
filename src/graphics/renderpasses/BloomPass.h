#pragma once

#include "Shader.h"
#include "../../src/core/layers/Layer.h"
#include "../../src/graphics/renderer/BloomRenderer.h"

class BloomLayer : public Layer
{
private:
	unsigned int VAO, VBO;
	unsigned int hdrFBO;
	unsigned int colorBuffers[2];
	unsigned int rboDepth;
	unsigned int attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
	std::unique_ptr<Shader> bloomShader;
	BloomRenderer bloomRenderer;

public:
	BloomLayer(const std::string& name);
	~BloomLayer() = default;

	void onAttach(LayerManager* manager) override;
	void onDetach() override;
	void onUpdate() override;
	void onGuiUpdate() override;
	void onEvent(Event& event) override;
};

