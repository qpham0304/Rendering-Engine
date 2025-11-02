#pragma once

#include <FrameBuffer.h>
#include <memory>
#include <Shader.h>
#include "Layer.h"
#include "../../src/graphics/renderer/SkyboxRenderer.h"

class AppLayer : public Layer
{
protected:
	FrameBuffer applicationFBO;
	std::unique_ptr<Camera> camera;
	std::unique_ptr<SkyboxRenderer> skybox;
	unsigned int VAO, VBO;
	bool isActive;

	void renderControl();
	void renderApplication(const int& fboTexture = -1);

public:
	AppLayer(const std::string& name);
	~AppLayer();

	void OnAttach(LayerManager* manager) override;
	void OnDetach() override;
	void OnUpdate() override;
	void OnGuiUpdate() override;
	void OnEvent(Event& event) override;
	const int& GetTextureID();
};

