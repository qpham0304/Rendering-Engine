#pragma once

#include <memory>
#include "src/graphics/framework/OpenGL/core/ShaderOpenGL.h"
#include "Layer.h"
#include "../../src/graphics/renderers/SkyboxRenderer.h"
#include "../../src/logging/Logger.h"
#include "../../graphics/framework/OpenGL/renderers/FrameBuffer.h"

class AppLayer : public Layer
{
private:
	using Layer::setLogScopeEngine;
	using Layer::setLogScopeClient;

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

	int init() override;
	void onAttach(LayerManager* manager) override;
	void onDetach() override;
	void onUpdate() override;
	void onGuiUpdate() override;
	void onEvent(Event& event) override;
	const int& GetTextureID();
};

