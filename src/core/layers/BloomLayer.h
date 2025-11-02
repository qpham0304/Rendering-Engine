#pragma once

#include "Layer.h"
#include "Shader.h"
#include "../../graphics/renderer/BloomRenderer.h"

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

	void OnAttach(LayerManager* manager) override;
	void OnDetach() override;
	void OnUpdate() override;
	void OnGuiUpdate() override;
	void OnEvent(Event& event) override;
};

