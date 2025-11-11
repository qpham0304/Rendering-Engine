#pragma once

#include <FrameBuffer.h>
#include <memory>
#include <Shader.h>
#include "Layer.h"
#include "../../src/graphics/renderer/SkyboxRenderer.h"
#include "../../src/logging/Logger.h"

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

	void onAttach(LayerManager* manager) override;
	void onDetach() override;
	void onUpdate() override;
	void onGuiUpdate() override;
	void onEvent(Event& event) override;
	const int& GetTextureID();
};

